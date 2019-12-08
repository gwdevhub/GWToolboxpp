#include "stdafx.h"
#include "TradeWindow.h"

#include <WinSock2.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <GWCA\Constants\Constants.h>
#include <GWCA\Context\GameContext.h>
#include <GWCA\Context\WorldContext.h>

#include <GWCA\GameContainers\Array.h>

#include <GWCA\Packets\StoC.h>

#include <GWCA\Managers\UIMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\GameThreadMgr.h>
#include <GWCA\Managers\StoCMgr.h>

#include <Modules\Resources.h>

#include "logger.h"
#include "GuiUtils.h"
#include "GWToolbox.h"

// Every connection cost 30 seconds.
// You have 2 tries.
// After that, you can try every 30 seconds.
static const uint32_t COST_PER_CONNECTION_MS = 30 * 1000;
static const uint32_t COST_PER_CONNECTION_MAX_MS = 60 * 1000;

using easywsclient::WebSocket;
using nlohmann::json;
using json_vec = std::vector<json>;

static const char ws_host[] = "wss://kamadan.decltype.org/ws/";

static const std::regex regex_check = std::regex("^/(.*)/[a-z]?$", std::regex::ECMAScript | std::regex::icase);

void TradeWindow::NotifyTradeBlockedInKamadan() {
	if (print_game_chat && only_show_trade_alerts_in_kamadan && filter_alerts && GetInKamadan())
		Log::Info("Only trade alerts will be visible in the trade channel.\nYou can still view all Kamadan trade messages via Trade window.");
}
void TradeWindow::Initialize() {
	ToolboxWindow::Initialize();
	Resources::Instance().LoadTextureAsync(&button_texture, Resources::GetPath(L"img/icons", L"trade.png"));

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		Log::Log("WSAStartup Failed.\n");
		return;
	}

	messages = CircularBuffer<Message>(100);

	should_stop = false;
	worker = std::thread([this]() {
		while (!should_stop) {
			if (thread_jobs.empty()) {
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			else {
				thread_jobs.front()();
				thread_jobs.pop();
			}
		}
		});

	if (print_game_chat) AsyncChatConnect();

	GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::MapLoaded>(&OnTradeMessage_Entry, [&](GW::HookStatus*, GW::Packet::StoC::MapLoaded*) {
		NotifyTradeBlockedInKamadan();
		});
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageLocal>(&OnTradeMessage_Entry, [&](GW::HookStatus* status, GW::Packet::StoC::MessageLocal* pak) {
		if (!print_game_chat || !only_show_trade_alerts_in_kamadan || !filter_alerts) return;
		if (pak->channel != GW::Chat::CHANNEL_TRADE) return;
		if (!GetInKamadan()) return;
		GW::Array<wchar_t>* buff = &GW::GameContext::instance()->world->message_buff;
		if (!buff || !buff->valid())
			return;
		if (!FilterTradeMessage(buff->begin())) {
			status->blocked = true;
			buff->clear();
		}
		});
	NotifyTradeBlockedInKamadan();
}

void TradeWindow::Terminate() {
	should_stop = true;
	if (worker.joinable())
		worker.join();

	if (ws_chat) DeleteWebSocket(ws_chat);
	if (ws_window) DeleteWebSocket(ws_window);
	if (ws_chat || ws_window) {
		ws_chat = nullptr;
		ws_window = nullptr;
	}

	WSACleanup();
	ToolboxWindow::Terminate();
}

TradeWindow::~TradeWindow() {
	Terminate();
}

bool TradeWindow::GetInKamadan() {
	using namespace GW::Constants;
	switch (GW::Map::GetMapID()) {
	case MapID::Kamadan_Jewel_of_Istan_outpost:
	case MapID::Kamadan_Jewel_of_Istan_Halloween_outpost:
	case MapID::Kamadan_Jewel_of_Istan_Wintersday_outpost:
	case MapID::Kamadan_Jewel_of_Istan_Canthan_New_Year_outpost:
		return GW::Map::GetRegion() == GW::Constants::Region::America && GW::Map::GetDistrict() == 1;
	default:
		return false;
	}
}

bool TradeWindow::FilterTradeMessage(std::string msg) {
	if (!filter_alerts)
		return true;
	for (auto& word : alert_words) {
		if (std::regex_search(word, m, regex_check)) {
			try {
				word_regex = std::regex(m._At(1).str(), std::regex::ECMAScript | std::regex::icase);
			}
			catch (...) {
				// Silent fail; invalid regex
			}
			if (std::regex_search(msg, word_regex))
				return true;
		}
		else {
			auto found = std::search(msg.begin(), msg.end(), word.begin(), word.end(), [](char c1, char c2) -> bool {
				return tolower(c1) == c2;
				});
			if (found != msg.end())
				return true;
		}
	}
	return false;
}
bool TradeWindow::FilterTradeMessage(std::wstring msg) {
	std::string ws = GuiUtils::WStringToString(msg);
	return FilterTradeMessage(ws);
}

void TradeWindow::Update(float delta) {
	fetch();
	if (ws_chat) {
		switch (ws_chat->getReadyState()) {
			case WebSocket::CLOSED:
				delete ws_chat;
				ws_chat = nullptr;
				return;
			case WebSocket::CLOSING:
				ws_chat->poll();
				return;
		}
	}

	if (!print_game_chat || GetInKamadan()) {
		if(ws_chat) ws_chat->close();
		return;
	}
	if (!ws_chat && !ws_chat_connecting) {
		AsyncChatConnect();
		return;
	}

	if (!ws_chat || ws_chat->getReadyState() != WebSocket::OPEN)
		return;

	ws_chat->poll();
	ws_chat->dispatch([this](const std::string& data) {
		wchar_t buffer[512];
		json res = json::parse(data.c_str());

		// We don't support queries in the chat
		if (res.find("query") != res.end())
			return;

		std::string msg = res["message"].get<std::string>();
		if (FilterTradeMessage(msg)) {
			std::wstring name_ws = GuiUtils::StringToWString(res["name"].get<std::string>());
			std::wstring msg_ws = GuiUtils::StringToWString(msg);
			wnsprintfW(buffer, sizeof(buffer), L"<a=1>%s</a>: <c=#f96677><quote>%s", name_ws.c_str(), msg_ws.c_str());
			GW::Chat::WriteChat(GW::Chat::CHANNEL_TRADE, buffer);
		}
		});
}

TradeWindow::Message TradeWindow::parse_json_message(json js) {
	TradeWindow::Message msg;
	msg.name = js["name"].get<std::string>();
	msg.message = js["message"].get<std::string>();
	msg.timestamp = stoi(js["timestamp"].get<std::string>());
	return msg;
}

void TradeWindow::fetch() {
	if (ws_window == nullptr) return;
	if (ws_window->getReadyState() != WebSocket::OPEN) return;

	ws_window->poll();
	ws_window->dispatch([this](const std::string& data) {
		json res = json::parse(data.c_str());
		if (res.find("query") == res.end()) {
			// It's a new message
			Message msg = parse_json_message(res);
			messages.add(msg);
		}
		else {
			search_pending = false;
			if (res["num_results"].get<std::string>() == "0")
				return;
			json_vec results = res["results"].get<json_vec>();
			messages.clear();
			for (auto it = results.rbegin(); it != results.rend(); it++) {
				Message msg = parse_json_message(*it);
				messages.add(msg);
			}
		}
		});
}

void TradeWindow::search(std::string query) {
	if (!ws_window || ws_window->getReadyState() != WebSocket::OPEN)
		return;

	// for now we won't allow to enqueue more than 1 search, it shouldn't change anything because how fast we get the answers
	if (search_pending)
		return;

	search_pending = true;

	/*
	 * The protocol is the following:
	 *  - From a connected web socket, we send a Json formated packet with the format
	 *  {
	 *   "query":  str($query$),
	 *   "offset": int($page$),
	 *   "sugest": int(1 or 2)
	 *  }
	 */

	json request;
	request["query"] = query;
	request["offset"] = 0;
	request["suggest"] = 0;
	ws_window->send(request.dump());
}

void TradeWindow::Draw(IDirect3DDevice9* device) {
	if (visible) {
		if (ws_window == nullptr) {
			AsyncWindowConnect();
		}
	}
	else { // not visible
		if (ws_window) {
			ws_window->close();
			delete ws_window;
			ws_window = nullptr;
		}
		return;
	}

	if (ws_window && ws_window->getReadyState() == WebSocket::CLOSED) {
		delete ws_window;
		ws_window = nullptr;
	}

	// start the trade_searcher if its not active
	// if (!trade_searcher->is_active() && !trade_searcher->is_timed_out()) trade_searcher->search("");
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(700, 400), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
		/* Search bar header */
		static char search_buffer[256];
		ImGui::PushItemWidth((ImGui::GetWindowContentRegionWidth() - 80.0f - 80.0f - 80.0f - ImGui::GetStyle().ItemInnerSpacing.x * 6));
		if (ImGui::InputText("", search_buffer, 256, ImGuiInputTextFlags_EnterReturnsTrue)) {
			search(search_buffer);
		}
		ImGui::SameLine();
		if (ImGui::Button("Search", ImVec2(80.0f, 0))) {
			search(search_buffer);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear", ImVec2(80.0f, 0))) {
			GuiUtils::StrCopy(search_buffer, "", 256);
			search("");
		}
		ImGui::SameLine();
		if (ImGui::Button("Alerts", ImVec2(80.0f, 0))) {
			show_alert_window = !show_alert_window;
		}

		/* Main trade chat area */
		ImGui::BeginChild("trade_scroll", ImVec2(0, -20.0f - ImGui::GetStyle().ItemInnerSpacing.y));
		/* Connection checks */
		if (!ws_window && !ws_window_connecting) {
			ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("The connection to kamadan.decltype.com has timed out.").x) / 2);
			ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2);
			ImGui::Text("The connection to kamadan.decltype.com has timed out.");
			ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Click to reconnect").x) / 2);
			if (ImGui::Button("Click to reconnect")) {
				AsyncWindowConnect();
			}
		}
		else if (ws_window_connecting || (ws_window && ws_window->getReadyState() == WebSocket::CONNECTING)) {
			ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Connecting...").x) / 2);
			ImGui::SetCursorPosY(ImGui::GetWindowHeight() / 2);
			ImGui::Text("Connecting...");
		}
		else {
			const float font_scale = ImGui::GetIO().FontGlobalScale;
			/* Display trade messages */
			bool show_time = ImGui::GetWindowWidth() > 600.0f * font_scale;

			char timetext[128];
			time_t now = time(nullptr);
			
			const float innerspacing = ImGui::GetStyle().ItemInnerSpacing.x;
			const float time_width = show_time ? 100.0f * font_scale : 0.0f;
			const float playername_left = time_width + innerspacing; // player button left align
			const float playernamewidth = 160.0f * font_scale;
			const float message_left = playername_left + playernamewidth + innerspacing;

			size_t size = messages.size();
			for (unsigned int i = size - 1; i < size; i--) {
				Message& msg = messages[i];
				ImGui::PushID(i);

				// ==== time elapsed column ====
				if (show_time) {
					// negative numbers have came from this before, it is probably just server client desync
					int time_since_message = (int)now - msg.timestamp;

					ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::f16));
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.7f, .7f, .7f, 1.0f));

					// decide if days, hours, minutes, seconds...
					if ((int)(time_since_message / (60 * 60 * 24))) {
						int days = (int)(time_since_message / (60 * 60 * 24));
						_snprintf(timetext, 128, "%d %s ago", days, days > 1 ? "days" : "day");
					}
					else if ((int)(time_since_message / (60 * 60))) {
						int hours = (int)(time_since_message / (60 * 60));
						_snprintf(timetext, 128, "%d %s ago", hours, hours > 1 ? "hours" : "hour");
					}
					else if ((int)(time_since_message / (60))) {
						int minutes = (int)(time_since_message / 60);
						_snprintf(timetext, 128, "%d %s ago", minutes, minutes > 1 ? "minutes" : "minute");
					}
					else {
						_snprintf(timetext, 128, "%d %s ago", time_since_message, time_since_message > 1 ? "seconds" : "second");
					}
					ImGui::SetCursorPosX(playername_left - innerspacing - ImGui::CalcTextSize(timetext).x);
					ImGui::Text(timetext);
					ImGui::PopStyleColor();
					ImGui::PopFont();
				}

				// ==== Sender name column ====
				if (show_time) {
					ImGui::SameLine(playername_left);
				}
				if (ImGui::Button(msg.name.c_str(), ImVec2(playernamewidth, 0))) {
					// open whisper to player
					GW::GameThread::Enqueue([&msg]() {
						GW::UI::SendUIMessage(GW::UI::kOpenWhisper, (wchar_t*)GuiUtils::StringToWString(msg.name).c_str(), nullptr);
						});
				}

				// ==== Message column ====
				ImGui::SameLine(message_left);
				ImGui::TextWrapped("%s", msg.message.c_str());
				ImGui::PopID();
			}
		}
		ImGui::EndChild();

		/* Link to website footer */
		if (ImGui::Button("Powered by https://kamadan.decltype.org", ImVec2(ImGui::GetWindowContentRegionWidth(), 20.0f))) {
			CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
			ShellExecuteA(NULL, "open", "https://kamadan.decltype.org", NULL, NULL, SW_SHOWNORMAL);
		}

		/* Alerts window */
		if (show_alert_window) {
			ImGui::SetNextWindowSize(ImVec2(250, 220), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Trade Alerts", &show_alert_window)) {
				DrawAlertsWindowContent(true);
			}
			ImGui::End();
		}
	}
	ImGui::End();
}

void TradeWindow::DrawAlertsWindowContent(bool ownwindow) {
	ImGui::Text("Alerts");
	bool edited = false;
	edited |= ImGui::Checkbox("Send Kamadan ad1 trade chat to your trade chat", &print_game_chat);
	if (print_game_chat) {
		edited |= ImGui::Checkbox("Only show messages containing:", &filter_alerts);
		ImGui::TextDisabled("(Each line is a separate keyword. Not case sensitive.)");
		if (ImGui::InputTextMultiline("##alertfilter", alert_buf, ALERT_BUF_SIZE,
			ImVec2(-1.0f, 0.0f))) {

			ParseBuffer(alert_buf, alert_words);
			alertfile_dirty = true;
		}
		if (filter_alerts) {
			edited |= ImGui::Checkbox("Only show trade alerts when in Kamadan ae1", &only_show_trade_alerts_in_kamadan);
			ImGui::ShowHelp("You can still view Kamadan trade chat via the main Trade Window");
		}
	}
	if (edited) {
		NotifyTradeBlockedInKamadan();
	}
}

void TradeWindow::DrawSettingInternal() {
	DrawAlertsWindowContent(false);
}

void TradeWindow::LoadSettings(CSimpleIni* ini) {
	ToolboxWindow::LoadSettings(ini);
	print_game_chat = ini->GetBoolValue(Name(), VAR_NAME(print_game_chat), print_game_chat);
	filter_alerts = ini->GetBoolValue(Name(), VAR_NAME(filter_alerts), filter_alerts);
	only_show_trade_alerts_in_kamadan = ini->GetBoolValue(Name(), VAR_NAME(only_show_trade_alerts_in_kamadan), only_show_trade_alerts_in_kamadan);

	std::ifstream alert_file;
	alert_file.open(Resources::GetPath(L"AlertKeywords.txt"));
	if (alert_file.is_open()) {
		alert_file.get(alert_buf, ALERT_BUF_SIZE, '\0');
		alert_file.close();
		ParseBuffer(alert_buf, alert_words);
	}
	alert_file.close();
}


void TradeWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWindow::SaveSettings(ini);

	ini->SetBoolValue(Name(), VAR_NAME(print_game_chat), print_game_chat);
	ini->SetBoolValue(Name(), VAR_NAME(filter_alerts), filter_alerts);
	ini->SetBoolValue(Name(), VAR_NAME(only_show_trade_alerts_in_kamadan), only_show_trade_alerts_in_kamadan);
	
	if (alertfile_dirty) {
		std::ofstream bycontent_file;
		bycontent_file.open(Resources::GetPath(L"AlertKeywords.txt"));
		if (bycontent_file.is_open()) {
			bycontent_file.write(alert_buf, strlen(alert_buf));
			bycontent_file.close();
			alertfile_dirty = false;
		}
	}
}

void TradeWindow::ParseBuffer(const char* text, std::vector<std::string>& words) {
	words.clear();
	std::istringstream stream(text);
	std::string word;
	while (std::getline(stream, word)) {
		for (size_t i = 0; i < word.length(); i++)
			word[i] = tolower(word[i]);
		words.push_back(word);
	}
}

void TradeWindow::ParseBuffer(std::fstream stream, std::vector<std::string>& words) {
	words.clear();
	std::string word;
	while (std::getline(stream, word)) {
		for (size_t i = 0; i < word.length(); i++)
			word[i] = tolower(word[i]);
		words.push_back(word);
	}
}

void TradeWindow::AsyncChatConnect() {
	if (ws_chat) return;
	if (ws_chat_connecting) return;
	if (!chat_rate_limiter.AddTime(COST_PER_CONNECTION_MS, COST_PER_CONNECTION_MAX_MS))
		return;
	ws_chat_connecting = true;
	thread_jobs.push([this]() {
		if (!(ws_chat = WebSocket::from_url(ws_host))) {
			Log::Log("Couldn't connect to the host '%s'", ws_host);
		}
		ws_chat_connecting = false;
		});
}

void TradeWindow::AsyncWindowConnect() {
	if (ws_window) return;
	if (ws_window_connecting) return;
	if (!window_rate_limiter.AddTime(COST_PER_CONNECTION_MS, COST_PER_CONNECTION_MAX_MS))
		return;
	ws_window_connecting = true;
	thread_jobs.push([this]() {
		if (!(ws_window = WebSocket::from_url(ws_host))) {
			Log::Log("Couldn't connect to the host '%s'", ws_host);
		}
		ws_window_connecting = false;
		});
}

void TradeWindow::DeleteWebSocket(easywsclient::WebSocket* ws) {
	if (!ws) return;
	if (ws->getReadyState() == easywsclient::WebSocket::OPEN)
		ws->close();
	while (ws->getReadyState() != easywsclient::WebSocket::CLOSED)
		ws->poll();
	delete ws;
}
