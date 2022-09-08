#include "stdafx.h"

#include <Logger.h>
#include <memory.h>

#include <Modules/GWFileRequester.h>
#include <Modules/Resources.h>

#pragma warning( push )
#pragma warning(disable: 4245) // conversion from 'int' to 'unsigned int', signed/unsigned mismatch
#pragma warning(disable: 4244) // conversion from 'unsigned int' to 'unsigned char', possible loss of data
#pragma warning(disable: 4456) // declaration of 'i' hides previous local declaration
#pragma warning(disable: 4189) // local variable is initialized but not referenced
#include <GWDatBrowser/xentax.cpp>
#include <GWDatBrowser/AtexAsm.cpp>
#include <GWDatBrowser/AtexReader.cpp>
#pragma warning( pop )

#include <GWCA/GameEntities/Map.h>

#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Utils/GuiUtils.h>

#include <Timer.h>

namespace {
    char* strnstr(char* str, const char* substr, size_t n)
    {
        char* p = str, * pEnd = str + n;
        size_t substr_len = strlen(substr);

        if (0 == substr_len)
            return str; // the empty string is contained everywhere.

        pEnd -= (substr_len - 1);
        for (; p < pEnd; ++p)
        {
            if (0 == strncmp(p, substr, substr_len))
                return p;
        }
        return NULL;
    }

    void CmdDownloadMapFile(const wchar_t*, int, wchar_t**) {
        GW::AreaInfo* i = GW::Map::GetMapInfo();
        if (!(i && i->file_id))
            return;
        char buf[32];
        snprintf(buf, sizeof(buf), "downloadfile %d", i->file_id);
        GW::Chat::SendChat('/', buf);
    }
    void CmdDownloadFile(const wchar_t*, int argc, wchar_t** argv) {
        const wchar_t* syntax = L"Syntax: /downloadfile [file_id]";
        if (argc < 2)
            return Log::ErrorW(syntax);
        uint32_t file_id = 0;
        if (!GuiUtils::ParseUInt(argv[1], &file_id, 10))
            return Log::ErrorW(syntax);
        Log::Info("Downloading file_id %d...", file_id);
        GWFileRequester::Instance().load_file(file_id, [](GWFileRequester::GWResource* out) {
            Log::Info("File id %d downloaded! %p", out);
            });
    }
    void CmdDownloadImage(const wchar_t*, int argc, wchar_t** argv) {
        const wchar_t* syntax = L"Syntax: /downloadimage [file_id]";
        if (argc < 2)
            return Log::ErrorW(syntax);
        uint32_t file_id = 0;
        if (!GuiUtils::ParseUInt(argv[1], &file_id, 10))
            return Log::ErrorW(syntax);
        Log::Info("Downloading file_id %d...", file_id);
        GWFileRequester::Instance().load_file(file_id, [file_id](GWFileRequester::GWResource* out) {
            if (!out) {
                Log::Error("Failed to download file %d", file_id);
                return;
            }

            GWFileRequester::GWResource copy = *out;
            copy.data = (unsigned char*)malloc(copy.data_len);
            memcpy(copy.data, out->data, copy.data_len);
            GWFileRequester::Instance().load_texture_from_resource(copy, nullptr);
            Log::Info("Img id %d downloaded! %p", file_id);
            free(copy.data);
            });
    }
}

void GWFileRequester::Initialize() {
    ToolboxModule::Initialize();
    Terminate();
    needs_to_shutdown = false;
    socket_thread = std::thread([&]() {
        message_loop();
        });
    // Example, bday present FFNA > Image not working.
    /*GWFileRequester::Instance().load_file(193281, [](GWFileRequester::GWResource* out) {
        GWFileRequester::GWResource copy = *out;
        copy.data = (unsigned char*)malloc(copy.data_len);
        memcpy(copy.data, out->data, copy.data_len);
        GWFileRequester::Instance().load_texture_from_resource(copy, nullptr);
        free(copy.data);
        });
    */
    GW::Chat::CreateCommand(L"mapfile", CmdDownloadMapFile);
    GW::Chat::CreateCommand(L"downloadfile", CmdDownloadFile);
    GW::Chat::CreateCommand(L"downloadimage", CmdDownloadImage);
    // Example TODO, skill image raw texture
    // ...

}
void GWFileRequester::Terminate() {
    needs_to_shutdown = true;
    if (socket_thread.joinable())
        socket_thread.join();
    if (wsaData.wVersion) {
        WSACleanup();
        wsaData = { 0 };
    }
}
void GWFileRequester::log(const wchar_t* format, ...) {
    va_list vl;
    va_start(vl, format);
    wchar_t buf[512];
    vswprintf(buf, _countof(buf), format, vl);
    va_end(vl);
    Log::InfoW(buf);
}

int GWFileRequester::connect(uint32_t server_num) {
    struct addrinfo hints, * servinfo;
    if (fs_socket) {
        return 0;
    }
    // TODO: Cycle this for each domain 1 to 12?
    ASSERT(snprintf(socket_domain, sizeof(socket_domain), "file%d.arenanetworks.com", server_num) != -1);
    //Ensure that servinfo is clear
    memset(&hints, 0, sizeof hints); //make sure the struct is empty

    //setup hints
    hints.ai_family = AF_UNSPEC;    //IPv4 or IPv6 doesnt matter
    hints.ai_socktype = SOCK_STREAM;        //TCP stream socket

    //Start WSA to be able to make DNS lookup
    int res;
    if (!wsaData.wVersion && (res = WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
        log(L"Failed to call WSAStartup: %d", res);
        disconnect();
        return 1;
    }

    if ((res = getaddrinfo(socket_domain, "6112", &hints, &servinfo)) != 0) {
        log(L"Failed to getaddrinfo: %S, %S", socket_domain, gai_strerror(res));
        disconnect();
        return 1;
    }

    //setup socket
    if ((fs_socket = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == INVALID_SOCKET) {
        log(L"Failed to socket: %d", WSAGetLastError());
        disconnect();
        return 1;
    }

    //Connect
    if (::connect(fs_socket, servinfo->ai_addr, servinfo->ai_addrlen) == SOCKET_ERROR) {
        log(L"Failed to connect: %d", WSAGetLastError());
        disconnect();
        return 1;
    }

    //We dont need this anymore
    freeaddrinfo(servinfo);

    // Handshake
    CtoFS_Hello hello;
    if (this->send(&hello, sizeof(hello)) == -1) {
        return 1;
    }
    FStoC_MainManifest* main_manifest_ptr = (FStoC_MainManifest*)this->recv();
    if (!main_manifest_ptr) {
        log(L"FStoC_MainManifest empty");
        disconnect();
        return 1;
    }
    if (main_manifest_ptr->header != FStoC_Header::MainManifest) {
        log(L"FStoC_MainManifest invalid header %d", main_manifest_ptr->header);
        disconnect();
        return 1;
    }
    main_manifest = *main_manifest_ptr;

    log(L"Connected to file server %S, gw exe hash %d", socket_domain, main_manifest.exe_file());
    return 0;
}
int GWFileRequester::send(void* packet, int size) {
    if (!fs_socket) {
        log(L"send_blocking without a socket");
        disconnect();
        return -1;
    }
    int sent_bytes = ::send(fs_socket, (const char*)packet, size, NULL);
    if (sent_bytes == SOCKET_ERROR) {
        log(L"send failed: %d", WSAGetLastError());
        disconnect();
        return -1;
    }
    return sent_bytes;
}
int GWFileRequester::message_loop() {
    while (1) {
        if (needs_to_shutdown) {
            disconnect();
            return 0; // Exit thread.
        }
        if (!pending_resource_requests.empty()) {
            GWResourceRequest* file_request = pending_resource_requests.back();
            pending_resource_requests.pop();
            process_resource_request(file_request);
            delete file_request;
        }
        Sleep(500);
    }
    return 0;
}
int GWFileRequester::process_resource_request(GWResourceRequest* file_request) {
    namespace fs = std::filesystem;
    GWResource& gw_resource = file_request->resource;

    wchar_t filename[128];
    swprintf(filename, _countof(filename), L"%d.download", gw_resource.file_id);
    // Check if we've already downloaded it
    auto gwfiles = Resources::GetPath(L"data", L"gwfiles");
    Resources::EnsureFolderExists(gwfiles);
    auto path = Resources::GetPath(gwfiles, filename);
    if (fs::exists(path)) {
        gw_resource.data_len = (size_t)fs::file_size(path);
        std::ifstream f(path, std::ios::in | std::ios::binary);
        gw_resource.data = (unsigned char*)malloc(gw_resource.data_len);
        f.read((char*)gw_resource.data, gw_resource.data_len);
        f.close();
        log(L"Loaded file %s from disk",path.wstring().c_str());
        goto complete_file_request;
    }
    if (!connected()) {
        for (size_t i = 0; i < 12 && !connected(); i++) {
            this->connect(i + 1);
        }
    }
    if (!connected()) {
        goto complete_file_request;
    }
    {
        CtoFS_Request packet;
        packet.file_id = gw_resource.file_id;
        if (!this->send(&packet, sizeof(packet))) {
            goto complete_file_request;
        }
        FStoC_FileManifest* file_manifest_ptr = (FStoC_FileManifest*)this->recv(sizeof(FStoC_FileManifest));
        if (!file_manifest_ptr) {
            log(L"FStoC_FileManifest empty");
            disconnect();
            goto complete_file_request;
        }
        if (file_manifest_ptr->header != FStoC_Header::FileManifest) {
            log(L"FStoC_FileManifest invalid header %d", file_manifest_ptr->header);
            disconnect();
            goto complete_file_request;
        }
        FStoC_FileManifest file_manifest = *file_manifest_ptr;
        unsigned char* compressed = (unsigned char*)malloc(file_manifest.size_compressed + 1);
        ASSERT(compressed);
        size_t downloaded = 0;
        while (downloaded < file_manifest.size_compressed) {
            FStoC_FileData* response = (FStoC_FileData*)this->recv(sizeof(FStoC_FileData));
            if (!response) {
                log(L"recv_blocking failed");
                disconnect();
                goto complete_file_request;
            }
            if (!(response->header == FStoC_Header::FileData || response->header == FStoC_Header::MoreData)) {
                log(L"FStoC_FileData invalid header %d", response->header);
                disconnect();
                goto complete_file_request;
            }
            uint32_t data_len = response->size - 4;
            char* data = this->recv(data_len);
            if (!data) {
                log(L"recv_blocking failed");
                disconnect();
                goto complete_file_request;
            }

            if (downloaded + data_len > file_manifest.size_compressed) {
                log(L"FStoC_FileData potential buffer overflow (wanted %d bytes, only allocated %d)", downloaded + data_len, file_manifest.size_compressed);
                disconnect();
                goto complete_file_request;
            }
            memcpy(&compressed[downloaded], data, data_len);
            downloaded += data_len;
            if (downloaded < file_manifest.size_compressed) {
                CtoFS_RequestMore more;
                more.data_offset = downloaded;
                if (this->send(&more, sizeof(more)) == -1) {
                    goto complete_file_request;
                }
            }
        }
        Decompress d;
        gw_resource.data_len = file_manifest.size_decompressed;
        gw_resource.data = d.DecompressFile((unsigned int*)compressed, file_manifest.size_compressed, (int&)(gw_resource.data_len));
        if (!gw_resource.data) {
            // failed to decompress
            goto complete_file_request;
        }
        // Write to file
        {
            std::ofstream fout(path, std::ofstream::out);
            fout.write((char*)gw_resource.data, gw_resource.data_len);
            fout.close();
            log(L"Written file %s to disk", path.wstring().c_str());
        }
    }

complete_file_request:
    if (file_request->callback) {
        file_request->callback(gw_resource.data ? &gw_resource : nullptr);
    }
    if (gw_resource.data) {
        delete[] gw_resource.data;
    }
    return 0;
}
bool GWFileRequester::GWResource::image(GWResource* out) {
    struct FFNA_File {
        char type[4];
        uint32_t size;
        inline unsigned char* data() { return &((unsigned char*)this)[8]; };
    };
    struct FFNA {
        char type[4]; // 'ffna'
        uint8_t unk; // 2 ? 
        inline unsigned char* data() { return &((unsigned char*)this)[5]; };
    };

    std::vector<FFNA_File*> files;
    int prefix = *((int*)data);
    switch (prefix) {
    case 'XTTA':
    case 'XETA':
        out->file_id = file_id;
        out->data_len = data_len;
        out->data = data;
        return true;
    case 'anff':
        FFNA * ffna = (FFNA*)data;
        unsigned char* _begin = ffna->data();

        char* found = strnstr((char*)_begin, "ATEX", data_len);
        if (!found) {
            found = strnstr((char*)_begin, "ATTX", data_len);
        }
        if (found) {
            out->file_id = file_id;
            out->data_len = *(int*)(found - 4);;
            out->data = (unsigned char*)found;
            return true;
        }
    }
    return false;
}
void GWFileRequester::load_file(uint32_t file_id, std::function<void(GWResource*)> callback) {
    GWResourceRequest* request = new GWResourceRequest(file_id);
    request->callback = callback;
    pending_resource_requests.push(request);
}
bool GWFileRequester::load_texture_from_resource(GWResource& in, IDirect3DTexture9**) {
    GWResource image;
    if (!in.image(&image)) {
        log(L"Failed to find image file within GWResource %d", in.file_id);
        return false;
    }
    return false;
}
char* GWFileRequester::recv(uint32_t recv_len) {
    ASSERT(recv_len < sizeof(message_buffer));
    message_buffer[0] = '\0';
    int ret_len = ::recv(fs_socket, message_buffer, recv_len ? recv_len : sizeof(message_buffer), 0);
    if (ret_len == SOCKET_ERROR) {
        log(L"%S recv failed, %d", __func__, WSAGetLastError());
        disconnect();
        return nullptr;
    }
    if (!message_buffer[0]) {
        log(L"%S message empty; graceful close?", __func__);
        disconnect();
        return nullptr;
    }
    message_buffer[ret_len] = '\0';
    return message_buffer;
}
int GWFileRequester::disconnect()
{
    printf("Disconnected from server.\n");
    if (fs_socket) {
#ifdef WIN32
        shutdown(fs_socket, 2);
#endif
        closesocket(fs_socket);
        fs_socket = 0;
    }
    return 0;
}