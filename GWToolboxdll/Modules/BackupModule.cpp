#include "stdafx.h"

#include <ImGuiAddons.h>
#include <Logger.h>
#include <Defines.h>
#include <Modules/Resources.h>
#include <Utils/SettingsRegistry.h>
#include <Utils/TextUtils.h>

#include "BackupModule.h"
#include "CodeOptimiserModule.h"

// ============================================================
// ZIP (STORE) writer/reader — no external dependencies
// ============================================================
namespace {

    // --- DOS date/time ----------------------------------------------------

    static void get_dos_dt(uint16_t& t, uint16_t& d)
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        t = static_cast<uint16_t>((st.wHour << 11) | (st.wMinute << 5) | (st.wSecond / 2));
        d = static_cast<uint16_t>(((st.wYear - 1980) << 9) | (st.wMonth << 5) | st.wDay);
    }

    // --- Little-endian helpers --------------------------------------------

    static void w16(FILE* f, uint16_t v) { fwrite(&v, 2, 1, f); }
    static void w32(FILE* f, uint32_t v) { fwrite(&v, 4, 1, f); }

    static bool r16(FILE* f, uint16_t& v) { return fread(&v, 2, 1, f) == 1; }
    static bool r32(FILE* f, uint32_t& v) { return fread(&v, 4, 1, f) == 1; }

    // --- ZIP entry --------------------------------------------------------

    struct ZipEntry {
        std::string  name;         // UTF-8, forward-slash separators
        std::vector<uint8_t> data;
        uint32_t     crc32 = 0;
        uint32_t     size  = 0;
        uint16_t     mod_time = 0;
        uint16_t     mod_date = 0;
        uint32_t     local_offset = 0; // filled during write
    };

    // --- ZIP write --------------------------------------------------------

    static bool zip_write(const std::filesystem::path& zip_path, const std::vector<ZipEntry>& entries)
    {
        FILE* f = _wfopen(zip_path.c_str(), L"wb");
        if (!f) return false;

        std::vector<uint32_t> offsets;
        offsets.reserve(entries.size());

        for (const auto& e : entries) {
            offsets.push_back(static_cast<uint32_t>(ftell(f)));

            const auto name_len = static_cast<uint16_t>(e.name.size());

            w32(f, 0x04034B50u);    // local file header signature
            w16(f, 20);             // version needed to extract (2.0)
            w16(f, 0x0800);         // general purpose bit flag: UTF-8
            w16(f, 0);              // compression method: STORE
            w16(f, e.mod_time);
            w16(f, e.mod_date);
            w32(f, e.crc32);
            w32(f, e.size);         // compressed size
            w32(f, e.size);         // uncompressed size
            w16(f, name_len);
            w16(f, 0);              // extra field length
            fwrite(e.name.data(), 1, name_len, f);
            if (!e.data.empty())
                fwrite(e.data.data(), 1, e.data.size(), f);
        }

        const uint32_t cd_offset = static_cast<uint32_t>(ftell(f));

        for (size_t i = 0; i < entries.size(); i++) {
            const auto& e = entries[i];
            const auto  name_len = static_cast<uint16_t>(e.name.size());

            w32(f, 0x02014B50u);    // central directory signature
            w16(f, 20);             // version made by
            w16(f, 20);             // version needed
            w16(f, 0x0800);         // UTF-8 flag
            w16(f, 0);              // STORE
            w16(f, e.mod_time);
            w16(f, e.mod_date);
            w32(f, e.crc32);
            w32(f, e.size);
            w32(f, e.size);
            w16(f, name_len);
            w16(f, 0);              // extra length
            w16(f, 0);              // comment length
            w16(f, 0);              // disk number start
            w16(f, 0);              // internal attributes
            w32(f, 0);              // external attributes
            w32(f, offsets[i]);     // relative offset of local header
            fwrite(e.name.data(), 1, name_len, f);
        }

        const uint32_t cd_size = static_cast<uint32_t>(ftell(f)) - cd_offset;
        const auto     n_entries = static_cast<uint16_t>(entries.size());

        w32(f, 0x06054B50u);        // end of central directory
        w16(f, 0);                  // disk number
        w16(f, 0);                  // disk with start of CD
        w16(f, n_entries);
        w16(f, n_entries);
        w32(f, cd_size);
        w32(f, cd_offset);
        w16(f, 0);                  // comment length

        fclose(f);
        return true;
    }

    // --- ZIP read/extract -------------------------------------------------

    static bool zip_extract(const std::filesystem::path& zip_path, const std::filesystem::path& dest_dir)
    {
        FILE* f = _wfopen(zip_path.c_str(), L"rb");
        if (!f) return false;

        fseek(f, 0, SEEK_END);
        const long file_size = ftell(f);

        // Locate End of Central Directory (scan backwards from EOF).
        // EOCD is 22 bytes minimum; allow for max 65535-byte comment.
        const long max_scan = std::min<long>(file_size, 22 + 65535);
        uint32_t cd_offset = 0;
        uint16_t n_entries = 0;
        bool found = false;

        for (long off = 22; off <= max_scan; off++) {
            fseek(f, file_size - off, SEEK_SET);
            uint32_t sig = 0;
            if (!r32(f, sig) || sig != 0x06054B50u) continue;

            // Skip: disk number (2), disk with CD (2), entries on disk (2).
            fseek(f, 6, SEEK_CUR);
            r16(f, n_entries);
            fseek(f, 4, SEEK_CUR);  // skip cd_size
            r32(f, cd_offset);
            found = true;
            break;
        }

        if (!found) { fclose(f); return false; }

        // Parse central directory.
        // Central dir entry layout after signature (4 bytes):
        //   ver_made(2), ver_need(2), flags(2), compression(2),
        //   mod_time(2), mod_date(2), crc(4), comp_sz(4), uncomp_sz(4),
        //   name_len(2), extra_len(2), comment_len(2),
        //   disk_start(2), int_attrs(2), ext_attrs(4), local_off(4)
        fseek(f, static_cast<long>(cd_offset), SEEK_SET);

        for (uint16_t i = 0; i < n_entries; i++) {
            uint32_t sig;
            if (!r32(f, sig) || sig != 0x02014B50u) break;

            uint16_t compression;
            uint32_t uncomp_sz;
            uint16_t name_len, extra_len, comment_len;
            uint32_t local_off;

            fseek(f, 6, SEEK_CUR);          // ver_made, ver_need, flags
            r16(f, compression);
            fseek(f, 8, SEEK_CUR);          // mod_time, mod_date, crc
            fseek(f, 4, SEEK_CUR);          // comp_sz (equal to uncomp_sz for STORE)
            r32(f, uncomp_sz);
            r16(f, name_len);
            r16(f, extra_len);
            r16(f, comment_len);
            fseek(f, 8, SEEK_CUR);          // disk_start, int_attrs, ext_attrs
            r32(f, local_off);

            std::string name(name_len, '\0');
            fread(name.data(), 1, name_len, f);
            fseek(f, extra_len + comment_len, SEEK_CUR);

            const long cd_pos = ftell(f);

            // Only handle STORE (no compression).
            if (compression != 0) { fseek(f, cd_pos, SEEK_SET); continue; }

            // Basic path-traversal guard.
            if (name.find("..") != std::string::npos || name.front() == '/') {
                fseek(f, cd_pos, SEEK_SET);
                continue;
            }

            // Seek to local file header.
            fseek(f, static_cast<long>(local_off), SEEK_SET);
            uint32_t local_sig;
            if (!r32(f, local_sig) || local_sig != 0x04034B50u) {
                fseek(f, cd_pos, SEEK_SET);
                continue;
            }

            // Skip 22 bytes of fixed local header fields, then name/extra.
            fseek(f, 22, SEEK_CUR);
            uint16_t local_name_len, local_extra_len;
            r16(f, local_name_len);
            r16(f, local_extra_len);
            fseek(f, local_name_len + local_extra_len, SEEK_CUR);

            // Build output path (ZIP uses '/' separators; Windows prefers '\').
            std::replace(name.begin(), name.end(), '/', '\\');
            const auto out_path = dest_dir / name;

            Resources::EnsureFolderExists(out_path.parent_path());

            if (uncomp_sz > 0) {
                std::vector<uint8_t> data(uncomp_sz);
                fread(data.data(), 1, uncomp_sz, f);

                FILE* out = _wfopen(out_path.c_str(), L"wb");
                if (out) {
                    fwrite(data.data(), 1, data.size(), out);
                    fclose(out);
                }
            }

            fseek(f, cd_pos, SEEK_SET);
        }

        fclose(f);
        return true;
    }

    // --- Validity check ---------------------------------------------------

    static bool is_valid_zip(const std::filesystem::path& path)
    {
        FILE* f = _wfopen(path.c_str(), L"rb");
        if (!f) return false;
        uint32_t sig = 0;
        fread(&sig, 4, 1, f);
        fclose(f);
        // A non-empty ZIP starts with a local file header (PK\x03\x04).
        // An empty ZIP starts with the EOCD record (PK\x05\x06) — both are valid.
        return sig == 0x04034B50u || sig == 0x06054B50u;
    }

    // ============================================================
    // Settings & state
    // ============================================================

    struct Settings {
        bool backup_text_files  = true;
        bool backup_image_files = false;
        bool backup_audio_files = false;
    } settings;

    // Extensions by category (all lower-case).
    constexpr std::string_view TEXT_EXTS[]  = {".ini", ".json", ".txt", ".tsv", ".csv", ".xml"};
    constexpr std::string_view IMAGE_EXTS[] = {".png", ".jpg", ".jpeg", ".bmp", ".dds"};
    constexpr std::string_view AUDIO_EXTS[] = {".mp3", ".wav", ".ogg"};

    // Extensions that are NEVER included regardless of settings.
    constexpr std::string_view ALWAYS_EXCLUDED[] = {".dmp", ".zip", ".dll", ".exe", ".old", ".pdb", ".lib"};

    static bool is_extension_allowed(std::string ext)
    {
        ext = TextUtils::ToLower(std::move(ext));

        for (const auto& e : ALWAYS_EXCLUDED)
            if (ext == e) return false;

        if (settings.backup_text_files)
            for (const auto& e : TEXT_EXTS)
                if (ext == e) return true;

        if (settings.backup_image_files)
            for (const auto& e : IMAGE_EXTS)
                if (ext == e) return true;

        if (settings.backup_audio_files)
            for (const auto& e : AUDIO_EXTS)
                if (ext == e) return true;

        return false;
    }

    // Read a file in binary mode into a byte vector.
    static bool read_binary(const std::filesystem::path& path, std::vector<uint8_t>& out)
    {
        FILE* f = _wfopen(path.c_str(), L"rb");
        if (!f) return false;
        fseek(f, 0, SEEK_END);
        const long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz < 0) { fclose(f); return false; }
        out.resize(static_cast<size_t>(sz));
        const size_t n = sz > 0 ? fread(out.data(), 1, sz, f) : 0;
        fclose(f);
        return static_cast<long>(n) == sz;
    }

    // Gathers files from the settings folder, respecting current extension settings.
    // text_only: ignore image/audio settings and only collect text files (for auto-backups).
    static bool collect_files(const std::filesystem::path& folder,
                              std::vector<ZipEntry>& entries,
                              bool text_only = false)
    {
        if (!std::filesystem::exists(folder)) return false;

        uint16_t mod_time, mod_date;
        get_dos_dt(mod_time, mod_date);

        const auto backups_subdir = (folder / "backups").lexically_normal();

        for (const auto& it : std::filesystem::recursive_directory_iterator(
                 folder, std::filesystem::directory_options::skip_permission_denied)) {
            if (!it.is_regular_file()) continue;

            const auto& p = it.path();

            // Skip anything inside the auto-backup storage folder.
            {
                const auto sub = p.lexically_relative(backups_subdir).lexically_normal();
                if (!sub.empty() && sub.begin()->string() != "..") continue;
            }

            const auto ext = TextUtils::ToLower(p.extension().string());

            // Always-excluded check.
            bool excluded = false;
            for (const auto& e : ALWAYS_EXCLUDED)
                if (ext == e) { excluded = true; break; }
            if (excluded) continue;

            if (text_only) {
                bool is_text = false;
                for (const auto& e : TEXT_EXTS)
                    if (ext == e) { is_text = true; break; }
                if (!is_text) continue;
            }
            else if (!is_extension_allowed(ext)) {
                continue;
            }

            std::vector<uint8_t> data;
            if (!read_binary(p, data)) continue;

            const auto rel = p.lexically_relative(folder).generic_string();

            ZipEntry e;
            e.name     = rel;
            e.data     = std::move(data);
            e.size     = static_cast<uint32_t>(e.data.size());
            e.crc32    = CodeOptimiserModule::Crc32(e.data.data(), e.data.size());
            e.mod_time = mod_time;
            e.mod_date = mod_date;
            entries.push_back(std::move(e));
        }
        return true;
    }

    // Adds a small info file to the ZIP so the archive is identifiable.
    static void add_info_entry(std::vector<ZipEntry>& entries, const std::string& ts)
    {
        uint16_t t, d;
        get_dos_dt(t, d);

        const std::string info = std::format(
            "GWToolbox++ Settings Backup\nTimestamp: {}\nVersion: {}\n",
            ts, GWTOOLBOXDLL_VERSION);

        ZipEntry e;
        e.name     = "_gwtoolbox_backup.txt";
        e.data     = std::vector<uint8_t>(info.begin(), info.end());
        e.size     = static_cast<uint32_t>(info.size());
        e.crc32    = CodeOptimiserModule::Crc32(e.data.data(), e.data.size());
        e.mod_time = t;
        e.mod_date = d;
        entries.insert(entries.begin(), std::move(e));
    }

    // Core backup creation. Called from worker thread.
    static bool do_create_backup(const std::filesystem::path& zip_path, bool text_only)
    {
        const auto settings_folder = Resources::GetSettingsFolderPath();

        std::vector<ZipEntry> entries;
        if (!collect_files(settings_folder, entries, text_only)) return false;

        const auto ts = TextUtils::FilenameTimestamp();
        add_info_entry(entries, ts);

        Resources::EnsureFolderExists(zip_path.parent_path());

        return zip_write(zip_path, entries);
    }

    // Restore state machine — driven from DrawSettingsInternal every frame.
    enum class RestoreStep { Idle, AskPreBackup, AskRestore, Restoring, Done };
    RestoreStep restore_step = RestoreStep::Idle;
    std::filesystem::path pending_restore_path;
    bool restore_succeeded = false;

    // ---- file dialog callbacks -------------------------------------------

    static void on_restore_file_chosen(const char* path)
    {
        if (!path) return;

        const std::filesystem::path p(path);
        if (!is_valid_zip(p)) {
            Log::Error("The selected file is not a valid ZIP archive.");
            return;
        }

        pending_restore_path = p;
        restore_step = RestoreStep::AskPreBackup;
    }

    static void on_save_file_chosen(const char* path)
    {
        if (!path) return;
        std::filesystem::path zip_path(path);
        if (zip_path.extension() != ".zip")
            zip_path += ".zip";

        Resources::EnqueueWorkerTask([zip_path] {
            const bool ok = do_create_backup(zip_path, false);
            if (ok)
                Log::Flash("Backup created: %s", zip_path.filename().string().c_str());
            else
                Log::Error("Failed to create backup.");
        });
    }

    // Performs the actual restore on the worker thread.
    static void do_restore(const std::filesystem::path& zip_path)
    {
        const auto dest = Resources::GetSettingsFolderPath();
        restore_succeeded = zip_extract(zip_path, dest);
        restore_step = RestoreStep::Done;
        if (restore_succeeded)
            Log::Flash("Settings restored from %s", zip_path.filename().string().c_str());
        else
            Log::Error("Failed to restore settings from backup.");
    }

} // namespace

// ============================================================
// BackupModule implementation
// ============================================================

void BackupModule::Initialize()
{
    ToolboxModule::Initialize();
    SettingsRegistry::Register(this, settings);
}

void BackupModule::LoadSettings(SettingsDoc& doc, ToolboxIni* legacy)
{
    ToolboxModule::LoadSettings(doc, legacy);
    doc.GetStruct(Name(), settings);
}

void BackupModule::SaveSettings(SettingsDoc& doc)
{
    ToolboxModule::SaveSettings(doc);
    doc.SetStruct(Name(), settings);
}

bool BackupModule::CreateAutoBackup()
{
    const auto backups_dir = Resources::GetSettingsFolderPath() / "backups";
    const auto zip_path    = backups_dir / std::format("auto_{}.zip", TextUtils::FilenameTimestamp());

    return do_create_backup(zip_path, /*text_only=*/true);
}

void BackupModule::DrawSettingsInternal()
{
    // ---- Create Backup section ------------------------------------------
    ImGui::TextUnformatted("File types to include in the backup:");

    ImGui::Checkbox("Text files (.ini, .json, .txt, .csv, .tsv, .xml)", &settings.backup_text_files);
    ImGui::Checkbox("Image files (.png, .jpg, .bmp, .dds)", &settings.backup_image_files);
    ImGui::Checkbox("Audio files (.mp3, .wav, .ogg)", &settings.backup_audio_files);

    ImGui::Spacing();

    if (ImGui::Button("Create backup now...")) {
        const auto ts  = TextUtils::FilenameTimestamp();
        const auto def = Resources::GetSettingsFolderPath() / std::format("backup_{}.zip", ts);
        Resources::SaveFileDialog(on_save_file_chosen, "zip", def.string().c_str());
    }

    ImGui::Separator();

    // ---- Restore section ------------------------------------------------
    ImGui::TextUnformatted("Restore from a previously created backup archive:");
    ImGui::Spacing();

    if (ImGui::Button("Browse for backup...")) {
        const auto def = Resources::GetSettingsFolderPath();
        Resources::OpenFileDialog(on_restore_file_chosen, "zip", def.string().c_str());
    }

    // ---- Restore confirmation modals ------------------------------------
    // These are opened once the file dialog callback sets restore_step.

    if (restore_step == RestoreStep::AskPreBackup) {
        ImGui::OpenPopup("Restore from backup?###backup_pre_restore");
        restore_step = RestoreStep::AskRestore; // prevent re-opening each frame
    }

    ImGui::SetNextWindowSize(ImVec2(-1.f, -1.f), ImGuiCond_Appearing);
    ImGui::SetNextWindowCenter(ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Restore from backup?###backup_pre_restore", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Restore from: %s", pending_restore_path.filename().string().c_str());
        ImGui::Spacing();
        ImGui::TextUnformatted("This will overwrite existing GWToolbox settings.");
        ImGui::TextUnformatted("Would you like to create a backup of current settings first?");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float scale  = ImGui::FontScale();
        const float btn_w  = 130.f * scale;

        if (ImGui::Button("Backup & Restore", ImVec2(btn_w + 20.f * scale, 0))) {
            ImGui::CloseCurrentPopup();
            const auto zip_path = pending_restore_path;
            Resources::EnqueueWorkerTask([zip_path] {
                if (!BackupModule::CreateAutoBackup())
                    Log::Warning("Pre-restore backup failed; proceeding with restore anyway.");
                do_restore(zip_path);
            });
            restore_step = RestoreStep::Restoring;
        }
        ImGui::SameLine();
        if (ImGui::Button("Restore Now", ImVec2(btn_w, 0))) {
            ImGui::CloseCurrentPopup();
            const auto zip_path = pending_restore_path;
            Resources::EnqueueWorkerTask([zip_path] { do_restore(zip_path); });
            restore_step = RestoreStep::Restoring;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(btn_w - 30.f * scale, 0))) {
            ImGui::CloseCurrentPopup();
            restore_step = RestoreStep::Idle;
        }

        ImGui::EndPopup();
    }
    else if (restore_step == RestoreStep::AskRestore &&
             !ImGui::IsPopupOpen("Restore from backup?###backup_pre_restore")) {
        // Popup dismissed without a button (Escape / click outside) → cancel.
        restore_step = RestoreStep::Idle;
    }

    if (restore_step == RestoreStep::Done) {
        restore_step = RestoreStep::Idle;
    }

    ImGui::Separator();

    // ---- Auto-backup info -----------------------------------------------
    const auto backups_dir = Resources::GetSettingsFolderPath() / "backups";
    ImGui::TextUnformatted("A text-file backup is automatically created before each toolbox update.");
    ImGui::TextDisabled("Auto-backup folder: %s", backups_dir.string().c_str());
}
