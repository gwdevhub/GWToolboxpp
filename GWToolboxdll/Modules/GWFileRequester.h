#pragma once

#include <GWCA/Utilities/Hook.h>

#include <ToolboxModule.h>


class GWFileRequester : public ToolboxModule {
public:
    GWFileRequester() = default;
    GWFileRequester(const GWFileRequester&) = delete;
    static GWFileRequester& Instance() {
        static GWFileRequester instance;
        return instance;
    }
    void Initialize() override;
    void Terminate() override;

    const char* Name() const override { return "GW File Requester"; }
    bool HasSettings() override { return false; }
    const char8_t* Icon() const override { return ICON_FA_FILE_DOWNLOAD; }

    struct GWResource {
        uint32_t file_id = 0;
        unsigned char* data = 0;
        size_t data_len = 0;
        bool image(GWResource* out);
    };
    struct GWResourceRequest {
        std::function<void(GWResource*)> callback = 0;
        GWResource resource;
        GWResourceRequest(uint32_t _file_id) {
            resource.file_id = _file_id;
        }
    };

    void load_file(uint32_t file_id, std::function<void(GWResource*)> callback);
    bool load_texture_from_resource(GWResource& in, IDirect3DTexture9** out);
private:

    std::queue<GWResourceRequest*> pending_resource_requests;

    // Socket/conn related
    int fs_socket = 0;
    bool connected() const { return fs_socket != 0; }
    bool needs_to_shutdown = false;
    char socket_domain[64];
    std::thread socket_thread;
    WSAData wsaData = { 0 };
    FILE* dataout = 0;
    FILE* datain = 0;
    char message_buffer[0xFFFF];

    int connect(uint32_t server_num = 0);
    int disconnect();
    int send(void* packet, int size);
    int process_resource_request(GWResourceRequest* file_request);
    int message_loop();
    char* recv(uint32_t recv_len = 0);

    void log(const wchar_t* format, ...);

    enum CtoFS_Header : short {
        Hello = 0xF1,
        Request = 0x3F2,
        RequestMore = 0x7F3,
    };
    enum FStoC_Header : short {
        MainManifest = 0x2F1,
        NotFound = 0x4F2,
        FileManifest = 0x5F2,
        FileData = 0x6F2,
        MoreData = 0x6F3,
        Complete = 0x7F2

    };
#pragma pack(push,1)
    struct CtoFS_Hello {
        uint8_t unk0[5] = { 1, 0, 0, 0, 0 };
        CtoFS_Header header = CtoFS_Header::Hello;
        short size = 0x10;
        int game = 1;
        int unk4 = 0;
        int unk5 = 0;
    };
#pragma pack(pop)
    // HHIIIIIII
    struct FStoC_MainManifest {
        FStoC_Header header;
        short size;
        int data[7]; // Variable length
        int asset_manifest_file() { return data[1]; }
        int exe_file() { return data[2]; }
    };
    // Keep a copy of main manifest in memory.
    FStoC_MainManifest main_manifest;

    // Note: Multiple file hashs can be appended to this packet, but a minimum of two is always required.
    // Version will request a delta update from specified build to the current version. version = 0 will send entire file.
    struct CtoFS_Request {
        CtoFS_Header header = CtoFS_Header::Request;
        short size = 0xC;
        uint32_t file_id = 0;
        uint32_t version = 0;
    };
    static_assert(sizeof(CtoFS_Request) == 12,"CtoFS_Request has invalid size");

    struct CtoFS_RequestMore {
        CtoFS_Header header = CtoFS_Header::RequestMore;
        short size = 0x8;
        uint32_t data_offset;
    };
    static_assert(sizeof(CtoFS_RequestMore) == 8, "CtoFS_RequestMore has invalid size");

    struct FStoC_FileManifest {
        FStoC_Header header;
        short size;
        uint32_t file_id;
        uint32_t size_decompressed;
        uint32_t size_compressed;
        uint32_t crc;
    };
    static_assert(sizeof(FStoC_FileManifest) == 20, "FStoC_FileManifest has invalid size");

    struct FStoC_FileData { // And MoreData
        FStoC_Header header;
        short size;
        char* data() { return (char*)((this) + sizeof(*this)); }
    };
    static_assert(sizeof(FStoC_FileData) == 4, "FStoC_FileData has invalid size");

    struct FStoC_Complete { // And MoreData
        FStoC_Header header;
        short size;
        int file_size;
    };


};
