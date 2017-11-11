#include "Updater.h"

#include <json.hpp>
#include <GWCA\Managers\GameThreadMgr.h>

#include <GWToolbox.h>
#include <Defines.h>
#include <logger.h>
#include <Modules\Resources.h>

void Updater::LoadSettings(CSimpleIni* ini) {
	ToolboxModule::LoadSettings(ini);
	mode = ini->GetLongValue(Name(), "update_mode", 2);
}

void Updater::SaveSettings(CSimpleIni* ini) {
	ToolboxModule::SaveSettings(ini);
	ini->SetLongValue(Name(), "update_mode", mode);
}

void Updater::DrawSettingInternal() {
	ImGui::Text("Update mode:");
	ImGui::RadioButton("Do not check for updates", &mode, 0);
	ImGui::RadioButton("Check and display a message", &mode, 1);
	ImGui::RadioButton("Check and ask before updating", &mode, 2);
	ImGui::RadioButton("Check and automatically update", &mode, 3);
}

void Updater::CheckForUpdate() {
	step = Checking;

	if (mode == 0) {
		step = Done;
		return;
	}

	server_version = "";
	Resources::Instance().EnqueueWorkerTask([this]() {
		// Here we are in the worker thread and can do blocking operations
		// Reminder: do not send stuff to gw chat from this thread!
		std::string version = Resources::Instance().Download(
			"https://raw.githubusercontent.com/HasKha/GWToolboxpp/master/resources/toolboxversion.txt");

		if (version.compare(GWTOOLBOX_VERSION) == 0) {
			// server and client versions match
			if (BETA_VERSION[0]) {
				// we are a beta/pre-release version. Version is the same, do update (e.g. 1.0 BETA -> 1.0).
				// do update
			} else {
				// We are a release version, up to date
				server_version = version;
				step = Done;
				return;
			} 
		} else if (version.empty()) {
			// Error getting server version. Server down? We can do nothing.
			GW::GameThread::Enqueue([]() {
				Log::Info("Error checking for updates");
			});
			step = Done;
			return;
		} else {
			// Server and client version mismatch. 
			if (BETA_VERSION[0]) {
				// we are beta/pre-release of next version, don't update
				server_version = version;
				step = Done;
				return;
			} else {
				// we are a release version, do update.
			}
		}


		if (version.empty()) {
			GW::GameThread::Enqueue([]() {
				Log::Info("Error checking for updates");
			});
			step = Done;
			return;
		}

		// get json release manifest
		std::string s = Resources::Instance().Download(
			std::string("https://api.github.com/repos/HasKha/GWToolboxpp/releases/tags/") + version + "_Release");

		if (s.empty()) {
			step = Done;
			return;
		}

		using Json = nlohmann::json;
		Json json = Json::parse(s.c_str());

		std::string tag_name = json["tag_name"];
		std::string body = json["body"];

		if (tag_name.empty()) {
			step = Done;
			return; 
		}

		// we have a new version!
		changelog = body;
		server_version = version;
		step = Asking;
	});
}

void Updater::Draw(IDirect3DDevice9* device) {
	if (step == Asking && !server_version.empty()) {
		static bool notified = false;
		if (!notified) {
			notified = true;
			Log::Warning("GWToolbox++ version %s is available! You have %s%s.",
				server_version.c_str(), GWTOOLBOX_VERSION, BETA_VERSION);
		}

		switch (mode) {
		case 0: // no updating
			step = Done;
			break;

		case 1: // check and warn

			step = Done;
			break;

		case 2: { // check and ask
			bool visible = true;
			ImGui::SetNextWindowSize(ImVec2(-1, -1), ImGuiSetCond_Appearing);
			ImGui::SetNextWindowPosCenter(ImGuiSetCond_Appearing);
			ImGui::Begin("Toolbox Update!", &visible);
			ImGui::Text("GWToolbox++ version %s is available! You have %s%s",
				server_version.c_str(), GWTOOLBOX_VERSION, BETA_VERSION);
			ImGui::Text("Changes:");
			ImGui::Text(changelog.c_str());

			ImGui::Text("");
			ImGui::Text("Do you want to update?");
			if (ImGui::Button("Later", ImVec2(100, 0))) {
				step = Done;
			}
			ImGui::SameLine();
			if (ImGui::Button("OK", ImVec2(100, 0))) {
				DoUpdate();
			}
			ImGui::End();
			if (!visible) {
				step = Done;
			}
			break;
		}

		case 3: // check and do
			DoUpdate();
			break;

		default:
			break;
		}

	} else if (step == Downloading) {
		static bool visible = true;
		if (mode == 2 && visible) {
			ImGui::SetNextWindowSize(ImVec2(-1, -1), ImGuiSetCond_Appearing);
			ImGui::SetNextWindowPosCenter(ImGuiSetCond_Appearing);
			ImGui::Begin("Toolbox Update!", &visible);
			ImGui::Text("GWToolbox++ version %s is available! You have %s",
				server_version.c_str(), GWTOOLBOX_VERSION);
			ImGui::Text("Changes:");
			ImGui::Text(changelog.c_str());

			ImGui::Text("");
			ImGui::Text("Downloading update...");
			if (ImGui::Button("Hide", ImVec2(100, 0))) {
				visible = false;
			}
			ImGui::End();
		}
	} else if (step == Success) {
		static bool visible = true;
		if (mode >= 2 && visible) {
			ImGui::SetNextWindowSize(ImVec2(-1, -1), ImGuiSetCond_Appearing);
			ImGui::SetNextWindowPosCenter(ImGuiSetCond_Appearing);
			ImGui::Begin("Toolbox Update!", &visible);
			ImGui::Text("GWToolbox++ version %s is available! You have %s",
				server_version.c_str(), GWTOOLBOX_VERSION);
			ImGui::Text("Changes:");
			ImGui::Text(changelog.c_str());

			ImGui::Text("");
			ImGui::Text("Update successful, please restart toolbox.");
			if (ImGui::Button("OK", ImVec2(100, 0))) {
				visible = false;
			}
			ImGui::End();
		}
	}

	// if step == Done do nothing
}

void Updater::DoUpdate() {
	Log::Warning("Downloading update...");

	step = Downloading;

	// 0. find toolbox dll path
	HMODULE module = GWToolbox::GetDLLModule();
	CHAR* dllfile = new CHAR[MAX_PATH];
	DWORD size = GetModuleFileName(module, dllfile, MAX_PATH);
	if (size == 0) {
		Log::Error("Updater error - cannot find GWToolbox.dll path");
		step = Done;
		return;
	}
	Log::Log("dll file name is %s\n", dllfile);

	// 1. rename toolbox dll
	CHAR* dllold = new CHAR[MAX_PATH];
	strcpy_s(dllold, MAX_PATH, dllfile);
	strcat_s(dllold, MAX_PATH, ".old");
	Log::Log("moving to %s\n", dllold);
	DeleteFile(dllold);
	MoveFile(dllfile, dllold);

	// 2. download new dll
	Resources::Instance().Download(
		dllfile,
		std::string("https://github.com/HasKha/GWToolboxpp/releases/download/") 
			+ server_version + "_Release/GWToolbox.dll", 
		[this, dllfile, dllold](bool success) {
		if (success) {
			step = Success;
			Log::Warning("Update successful, please restart toolbox.");
		} else {
			Log::Error("Updated error - cannot download GWToolbox.dll");
			MoveFile(dllold, dllfile);
			step = Done;
		}
		delete[] dllfile;
		delete[] dllold;
	});
}
