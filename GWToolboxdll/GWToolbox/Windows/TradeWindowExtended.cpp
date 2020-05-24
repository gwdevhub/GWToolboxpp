#include "stdafx.h"
#include "TradeWindowExtended.h"
#include <regex>

#include <WinSock2.h>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <GWCA\Constants\Constants.h>

#include <GWCA\GameContainers\Array.h>
#include <GWCA\GameEntities\Player.h>
#include <GWCA\GameEntities\Agent.h>

#include <GWCA\Context\GameContext.h>
#include <GWCA\Context\WorldContext.h>

#include <GWCA\Packets\StoC.h>

#include <GWCA\Managers\UIMgr.h>
#include <GWCA\Managers\MapMgr.h>
#include <GWCA\Managers\ChatMgr.h>
#include <GWCA\Managers\GameThreadMgr.h>
#include <GWCA\Managers\StoCMgr.h>
#include <GWCA\Managers\PlayerMgr.h>
#include <GWCA\Managers\AgentMgr.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <Modules\Resources.h>
#include <Modules\GameSettings.h>

#include "logger.h"
#include "GuiUtils.h"
#include "GWToolbox.h"

#define TRADE_LOG_FILENAME "trade_log.txt"

static const uint32_t COST_PER_CONNECTION_MS = 30 * 1000;
static const uint32_t COST_PER_CONNECTION_MAX_MS = 60 * 1000;

using easywsclient::WebSocket;
using nlohmann::json;
using json_vec = std::vector<json>;

static const char ws_domain[] = "kamadan.gwtoolbox.com";
static const char ws_host[] = "wss://kamadan.gwtoolbox.com";
static const std::regex regex_check = std::regex("^/(.*)/[a-z]?$", std::regex::ECMAScript | std::regex::icase);

void TradeWindow::NotifyTradeBlockedInKamadan() {
	if (print_game_chat && only_show_trade_alerts_in_kamadan && filter_alerts && GetInKamadanAE1())
		Log::Info("Only trade alerts will be visible in the trade channel.\nYou can still view all Kamadan trade messages via Trade window.");
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
void TradeWindow::Initialize() {
	ToolboxWindow::Initialize();
	Resources::Instance().LoadTextureAsync(&button_texture, Resources::GetPath(L"img/icons", L"trade.png"));

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
		printf("WSAStartup Failed.\n");
		return;
	}

	messages = CircularBuffer<Message>(100);
	outpost_messages = CircularBuffer<Message>(500); // May want to increase size > 500 for a bigger log?
	outpost_messages_filtered_a = CircularBuffer<Message>(100); // Limit to last 100 matches
	outpost_messages_filtered_b = CircularBuffer<Message>(100); // Limit to last 100 matches

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

	// Callback to store trade messages when in Kamadan ae1.
	// This is a fallback for when kamadan.decltype.org goes down; trade window will still work in ae1
	// NOTE: Timestamps are based on received time; better to use a hook to `PrintChat` from the ChatMgr module when its exposed.
	GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageLocal>(&MessageLocal_Entry, [this](GW::HookStatus* status, GW::Packet::StoC::MessageLocal* pak) {
		if (pak->channel != GW::Chat::CHANNEL_TRADE)
			return;
		if (!GetInKamadanAE1())
			return;
		GW::Array<wchar_t>* buff = &GW::GameContext::instance()->world->message_buff;
		if (!buff->m_buffer || buff->m_size < 5)
			return; // No message, may have been cleared by another hook.
		Message m;
		m.timestamp = time(nullptr); // This could be better
		// wchar_t* from message buffer to Message.message
		std::wstring message;
		if (buff->m_buffer[0] == 0x108) {
			// Trade chat message (not party search window)
			message = std::wstring(&buff->m_buffer[2]);
		}
		else {
			// Trade chat message (via party search window)
			message = std::wstring(&buff->m_buffer[3]);
		}
		if (message.size() < 2)
			return;
		message[message.size() - 1] = '\0';
		if (message.empty())
			return;
		std::wstring player_name(GW::PlayerMgr::GetPlayerName(pak->player_number));
		if (player_name.empty())
			return;
		int size_needed = WideCharToMultiByte(CP_UTF8, 0, &message[0], (int)message.size(), NULL, 0, NULL, NULL);
		m.message = std::string(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, &message[0], (int)message.size(), &m.message[0], size_needed, NULL, NULL);

		size_needed = WideCharToMultiByte(CP_UTF8, 0, &player_name[0], (int)player_name.size(), NULL, 0, NULL, NULL);
		m.name = std::string(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, &player_name[0], (int)player_name.size(), &m.name[0], size_needed, NULL, NULL);

		outpost_messages.add(m);
		if (m.contains(searched_message)) {
			search_pending = true;
			search_local(searched_message);
		}
		localtradelogfile_dirty = true;
		// Block the trade message if we're only showing filtered messages
		if (print_game_chat
			&& only_show_trade_alerts_in_kamadan
			&& filter_alerts
			&& !FilterTradeMessage(buff->begin())) {
			status->blocked = true;
			buff->clear();
		}
		});
	GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::MapLoaded>(&OnTradeMessage_Entry, [&](GW::HookStatus*, GW::Packet::StoC::MapLoaded*) {
		NotifyTradeBlockedInKamadan();
		});
	NotifyTradeBlockedInKamadan();
}

void TradeWindow::Terminate() {
	ToolboxWindow::Terminate();
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
}

TradeWindow::~TradeWindow() {
	Terminate();
}

bool TradeWindow::GetInKamadanAE1() {
	return 	GetInKamadan() && GW::Map::GetDistrict() == 1 &&
		GW::Map::GetRegion() == GW::Constants::Region::America;
}
bool TradeWindow::GetInKamadan() {
	using namespace GW::Constants;
	switch (GW::Map::GetMapID()) {
	case MapID::Kamadan_Jewel_of_Istan_outpost:
	case MapID::Kamadan_Jewel_of_Istan_Halloween_outpost:
	case MapID::Kamadan_Jewel_of_Istan_Wintersday_outpost:
	case MapID::Kamadan_Jewel_of_Istan_Canthan_New_Year_outpost:
		return true;
	default:
		return false;
	}
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
	if (!enable_kamadan_decltype) {
		if (ws_chat) ws_chat->close();
		if (ws_window) ws_window->close();
		return;
	}
	if (!print_game_chat || GetInKamadan()) {
		if (ws_chat) ws_chat->close();
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
		Message msg = parse_json_message(res);
		if (FilterTradeMessage(msg.message)) {
			std::wstring name_ws = GuiUtils::StringToWString(msg.name);
			std::wstring msg_ws = GuiUtils::StringToWString(msg.message);
			wnsprintfW(buffer, sizeof(buffer), L"<a=1>%s</a>: <c=#f96677><quote>%s", name_ws.c_str(), msg_ws.c_str());
			GW::Chat::WriteChat(GW::Chat::CHANNEL_TRADE, buffer);
		}
		});
}

TradeWindow::Message TradeWindow::parse_json_message(json js) {
	TradeWindow::Message msg;
	try {
		if (js.find("s") != js.end()) {
			msg.name = js["s"].get<std::string>();
			msg.message = js["m"].get<std::string>();
			msg.timestamp = js["t"].get<time_t>() / 1000;
		}
		else {
			msg.name = js["name"].get<std::string>();
			msg.message = js["message"].get<std::string>();
			msg.timestamp = js["timstamp"].get<time_t>();
		}
	}
	catch(...) {
		Log::Error("Failed to parse message");
	}
	return msg;
}

void TradeWindow::fetch() {
	if (ws_window) {
		switch (ws_window->getReadyState()) {
		case WebSocket::CLOSED:
			delete ws_window;
			ws_window = nullptr;
			return;
		case WebSocket::CLOSING:
			ws_window->poll();
			return;
		}
	}
	if (!enable_kamadan_decltype || !visible) {
		if (ws_window) ws_window->close();
		return;
	}
	if (!ws_window) AsyncWindowConnect();
	if (!ws_window || ws_window->getReadyState() != WebSocket::OPEN) return;

	ws_window->poll();
	ws_window->dispatch([this](const std::string& data) {
		json res = json::parse(data.c_str());
		if (res.find("query") == res.end()) {
			// It's a new message
			Message msg = parse_json_message(res);
			if(msg.timestamp > 0)
				messages.add(msg);
		}
		else {
			search_pending = false;
			if (res["num_results"].get<size_t>() == 0)
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
	// for now we won't allow to enqueue more than 1 search, it shouldn't change anything because how fast we get the answers
	if (search_pending)
		return;

	search_pending = true;
	if (!ws_window || ws_window->getReadyState() != WebSocket::OPEN) {
		search_local(query);
		return;
	}



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
void TradeWindow::search_local(std::string query) {

	thread_jobs.push([this, query]() {
		CircularBuffer<Message>* filter_ptr = &outpost_messages_filtered_a;
		if (outpost_messages_filtered_ptr == filter_ptr)
			filter_ptr = &outpost_messages_filtered_b;
		filter_ptr->clear();
		if (query.empty()) {
			search_pending = false;
			searched_message = query;
			return;
		}
		unsigned int size = outpost_messages.size();
		for (unsigned int i = 0; i < size; i++) {
			if (!outpost_messages[i].contains(query))
				continue;
			filter_ptr->add(outpost_messages[i]);
		}
		outpost_messages_filtered_ptr = filter_ptr;
		search_pending = false;
		searched_message = query;
		});
}

void TradeWindow::Draw(IDirect3DDevice9* device) {
	if (!visible) return;
	// start the trade_searcher if its not active
	// if (!trade_searcher->is_active() && !trade_searcher->is_timed_out()) trade_searcher->search("");
	ImGui::SetNextWindowPosCenter(ImGuiSetCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(700, 400), ImGuiSetCond_FirstUseEver);
	if (ImGui::Begin(Name(), GetVisiblePtr(), GetWinFlags())) {
		/* Search bar header */
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
		if (ImGui::Button("Settings", ImVec2(80.0f, 0))) {
			show_alert_window = !show_alert_window;
		}

		CircularBuffer<Message>* messages_ptr = nullptr;

		/* Main trade chat area */
		ImGui::BeginChild("trade_scroll", ImVec2(0, -20.0f - ImGui::GetStyle().ItemInnerSpacing.y));
		/* Connection checks */
		if (!ws_window && !ws_window_connecting) {
			messages_ptr = searched_message.size() ? outpost_messages_filtered_ptr : &outpost_messages;
		}
		else if (ws_window_connecting || (ws_window && ws_window->getReadyState() == WebSocket::CONNECTING)) {
			messages_ptr = searched_message.size() ? outpost_messages_filtered_ptr : &outpost_messages;
		}
		else if (ws_window) {
			messages_ptr = &messages;
		}
		if (messages_ptr) {
			/* Display trade messages */
			bool show_time = ImGui::GetWindowWidth() > 600.0f;

			char timetext[128];
			time_t now = time(nullptr);

			const float innerspacing = ImGui::GetStyle().ItemInnerSpacing.x;
			const float time_width = (show_time ? 100.0f : 0.0f) * ImGui::GetIO().FontGlobalScale;
			const float playername_left = time_width + innerspacing; // player button left align
			const float playernamewidth = 160.0f * ImGui::GetIO().FontGlobalScale;
			const float message_left = playername_left + playernamewidth + innerspacing;

			size_t size = messages_ptr->size();
			for (unsigned int i = size - 1; i < size; i--) {
				Message& msg = (*messages_ptr)[i];
				ImGui::PushID(i);
				// ==== Sender name column ====
				if (show_time)
					ImGui::SetCursorPosX(playername_left);
				ImGui::Button(msg.name.c_str(), ImVec2(playernamewidth, 0));
				if (!ImGui::IsItemVisible()) {
					ImGui::PopID();
					continue; // No need to draw stuff that isn't on-screen.
				}
				if (ImGui::IsItemClicked()) {
					std::wstring name = GuiUtils::StringToWString(msg.name);
					// Control + click = target player
					if (ImGui::GetIO().KeysDown[VK_CONTROL]) {
						GW::Player* player = GW::PlayerMgr::GetPlayerByName(name.c_str());
						if (player) {
							GW::GameThread::Enqueue([player]() {
								GW::Agents::ChangeTarget(player->agent_id);
								});
						}
					}
					else {
						// open whisper to player
						GW::GameThread::Enqueue([msg]() {
							std::wstring name = GuiUtils::StringToWString(msg.name);
							GW::UI::SendUIMessage(GW::UI::kOpenWhisper, (wchar_t*)name.data(), nullptr);
							});
					}
				}
				// ==== time elapsed column ====
				if (show_time) {
					// negative numbers have came from this before, it is probably just server client desync
					clock_t time_since_message = now - msg.timestamp;

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
					ImGui::SameLine(playername_left - innerspacing - ImGui::CalcTextSize(timetext).x);
					ImGui::Text(timetext);
					ImGui::PopStyleColor();
					ImGui::PopFont();
				}

				// ==== Message column ====
				ImGui::SameLine(message_left);
				ImGui::TextWrapped("%s", msg.message.c_str());
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
		if (ws_window) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));
			ImGui::Button("Connected to kamadan.gwtoolbox.com", ImVec2(0, 20.0f));
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("GWToolbox is fetching new trade messages from %s", ws_domain);
		}
		else if (ws_window_connecting) {
			ImGui::Button("Connecting...", ImVec2(0, 20.0f));
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("GWToolbox is trying to connect to %s", ws_domain);
		}
		else if (messages_ptr && GetInKamadanAE1()) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));
			ImGui::Button("Connected Locally", ImVec2(0, 20.0f));
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("GWToolbox is fetching new trade messages from Trade chat in this outpost");
		}
		else {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
			if (enable_kamadan_decltype) {
				char str[128];
				sprintf(str, "Not Connected - Click to re-connect to %s", ws_domain);
				if (ImGui::Button(str, ImVec2(0, 20.0f)))
					AsyncWindowConnect(true);
			}
			else {
				if (ImGui::Button("Not Connected - Click to travel to Kamadan ae1", ImVec2(0, 20.0f)))
					GW::Map::Travel(GW::Constants::MapID::Kamadan_Jewel_of_Istan_outpost, GW::Constants::District::American, 1);
			}
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("GWToolbox isn't fetching new trade messages.");
		}

		/* Alerts window */
		if (show_alert_window) {
			ImGui::SetNextWindowSize(ImVec2(250, 220), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("Trade Settings", &show_alert_window)) {
				DrawAlertsWindowContent(true);
			}
			ImGui::End();
		}
	}
	ImGui::End();
}

void TradeWindow::DrawAlertsWindowContent(bool ownwindow) {
	ImGui::Text("Trade Window mode:");
	if (ImGui::RadioButton(ws_domain, enable_kamadan_decltype)) {
		AsyncWindowConnect(true);
	}
	char str[128];
	sprintf(str, "Show messages from %s in Trade Window", ws_domain);
	ImGui::ShowHelp(str);
	ImGui::SameLine();
	if (ImGui::RadioButton("In-Game Trade Chat", !enable_kamadan_decltype))
		enable_kamadan_decltype = false;
	ImGui::ShowHelp("Show messages from your Kamadan ae1 Trade Chat in Trade Window");
	ImGui::Text("Alerts:");
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
			edited |= ImGui::Checkbox("Block all other trade messages when in Kamadan ae1", &only_show_trade_alerts_in_kamadan);
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
	print_game_chat = ini->GetBoolValue(Name(), VAR_NAME(print_game_chat), false);
	filter_alerts = ini->GetBoolValue(Name(), VAR_NAME(filter_alerts), false);
	flash_window_on_trade_alert = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_trade_alert), flash_window_on_trade_alert);
	enable_kamadan_decltype = ini->GetBoolValue(Name(), VAR_NAME(enable_kamadan_decltype), enable_kamadan_decltype);
	only_show_trade_alerts_in_kamadan = ini->GetBoolValue(Name(), VAR_NAME(only_show_trade_alerts_in_kamadan), only_show_trade_alerts_in_kamadan);

	std::ifstream alert_file;
	alert_file.open(Resources::GetPath(L"AlertKeywords.txt"));
	if (alert_file.is_open()) {
		alert_file.get(alert_buf, ALERT_BUF_SIZE, '\0');
		alert_file.close();
		ParseBuffer(alert_buf, alert_words);
	}
	alert_file.close();
	// Load local trade log
	{
		outpost_messages.clear();
		std::ifstream local_trade_log_file(Resources::GetPath(L"LocalTradeLog.txt"));
		std::vector<std::vector<std::string> > dataList;
		std::string line = "";
		std::string delim("\x1");
		// Iterate through each line and split the content using delimeter
		while (getline(local_trade_log_file, line))
		{
			std::size_t current, previous = 0;
			current = line.find_first_of(delim);
			Message m;
			if (current != std::string::npos) {
				m.timestamp = stol(line.substr(previous, current - previous));
				previous = current + 1;
				current = line.find_first_of(delim, previous);
			}
			if (current != std::string::npos) {
				m.name = line.substr(previous, current - previous);
				previous = current + 1;
				current = line.find_first_of(delim, previous);
			}
			m.message = line.substr(previous, current - previous);
			if (m.timestamp && !m.name.empty() && !m.message.empty()) {
				outpost_messages.add(m);
			}
		}
		// Close the File
		local_trade_log_file.close();
	}
	search_pending = true;
	search_local(search_buffer);
}

void TradeWindow::SaveSettings(CSimpleIni* ini) {
	ToolboxWindow::SaveSettings(ini);

	ini->SetBoolValue(Name(), VAR_NAME(print_game_chat), print_game_chat);
	ini->SetBoolValue(Name(), VAR_NAME(filter_alerts), filter_alerts);
	ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_trade_alert), flash_window_on_trade_alert);
	ini->SetBoolValue(Name(), VAR_NAME(enable_kamadan_decltype), enable_kamadan_decltype);
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
	// Save local trade log
	if (localtradelogfile_dirty) {
		std::ofstream file1;
		file1.open(Resources::GetPath(L"LocalTradeLog.txt"));
		if (file1.is_open()) {
			std::string output_str;
			unsigned int size = outpost_messages.size();
			for (unsigned int i = 0; i < size; i++) {
				output_str += std::to_string(outpost_messages[i].timestamp);
				output_str += "\x1";
				output_str += outpost_messages[i].name;
				output_str += "\x1";
				output_str += outpost_messages[i].message;
				output_str += "\n";
				if (output_str.size() > 1024000) {
					file1.write(output_str.c_str(), output_str.size());
					output_str.clear();
				}
			}
			if (output_str.size()) {
				file1.write(output_str.c_str(), output_str.size());
				output_str.clear();
			}
			file1.close();
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

void TradeWindow::AsyncChatConnect(bool force) {
	if (!enable_kamadan_decltype) return;
	if (ws_chat) return;
	if (ws_chat_connecting) return;
	if (!force && !chat_rate_limiter.AddTime(COST_PER_CONNECTION_MS, COST_PER_CONNECTION_MAX_MS))
		return;
	ws_chat_connecting = true;
	thread_jobs.push([this]() {
		if (!(ws_chat = WebSocket::from_url(ws_host))) {
			Log::Log("Couldn't connect to the host '%s'", ws_host);
		}
		ws_chat_connecting = false;
		});
}

void TradeWindow::AsyncWindowConnect(bool force) {
	if (force) enable_kamadan_decltype = true;
	if (!enable_kamadan_decltype) return;
	if (ws_window) return;
	if (ws_window_connecting) return;
	if (!force && !window_rate_limiter.AddTime(COST_PER_CONNECTION_MS, COST_PER_CONNECTION_MAX_MS))
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
