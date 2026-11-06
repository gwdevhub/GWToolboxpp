# Guild Wars file-server fallback

`GuildWarsFileServerModule` provides asynchronous access to ArenaNet assets without
intercepting the client's download callbacks or writing to its archive.

## API

```cpp
GuildWarsFileServerModule::FetchFile(base_file_id,
    [](GuildWarsFileServerModule::FilePtr file, const std::string& error) {
        if (!file) {
            Log::Log("Asset download failed: %s", error.c_str());
            return;
        }
        ConsumeAsset(file->file_id, file->data);
    });
```

`FetchFile` can be called from any thread. Its return value says whether the
request was accepted, not whether a download succeeded. Invalid/reserved IDs,
shutdown, and a full pending queue return `false` without invoking the callback.
Accepted callbacks run from the module's game-thread `Update`, including cache
hits and errors. Shutdown cancels pending work and discards callbacks.

The callback receives immutable, shared-owned, decompressed **source asset** bytes
and their resolved revision ID. A retained `FilePtr` remains valid after cache
eviction. Callers can instead poll `GetFile`, which reports `NotRequested`,
`Pending`, `Ready`, or `Failed` and does not initiate a request. The session cache
is LRU-bounded to 128 MiB and 4,096 request entries; failures are cached until
eviction or restart. Duplicate base IDs share a pending request, and different
base IDs resolving to the same revision share live source bytes.

Downloads run on a dedicated worker, separate from the Resources decode workers.
Connects, sends and receives use deadlines and cancellation-aware nonblocking
socket waits. Windows DNS resolution runs on that worker too, but an outstanding
system resolver call must finish before unload can complete. The module keeps
the DLL loaded until its worker exits and joins it before destruction.

## Revision selection and transport

The client stores its dependency manifest under reserved archive alias `0x1001`.
The worker reads that local manifest and parses its zero-terminated records:

```text
0, current_id, base_id, dependency_base_ids..., 0, ...
```

This pins assets to the running client's manifest instead of using the latest
manifest advertised by the server. If the local manifest is unavailable, the
request fails rather than mixing client versions. Reserved IDs never enter the
network fallback, so manifest loading cannot recursively request itself.

The transport follows gwlauncher's `GwNet.cpp` and the TTS updater's
`file_client.py`: TCP `file1` through `file12.arenanetworks.com:6112`, the `0xF1`
handshake, a `0x3F2` full-file request with version zero, `0x5F2` metadata,
`0x6F2`/`0x6F3` data, and `0x7F3` flow-control credit. Connections are scoped to
one request and retried against alternate servers on failure. Packet sizes,
returned IDs, decompression sizes, and the CRC are checked before publishing any
bytes. The checksum is CRC-32 of the decompressed asset, continued over its
revision ID's four little-endian bytes.

The LZ/Huffman decoder is adapted from gwlauncher's `GwHuffman.cpp`. Toolbox's
older `UnpackGWDat` expects an archive-specific size trailer and cannot directly
consume the network framing.

## DAT and texture integration

`GwDatModule::ReadFile` remains nonblocking. It reads the original DAT snapshot
first, then uses/request-caches source bytes when the base payload is absent or
empty. Texture decodes that encountered pending network data are parked and
retried after completion. DirectX texture creation stays on the DX thread, and
the failure indicator remains false while a download is pending.

A missing optional stream on a locally available source file does not by itself
start another download. Unprocessed local or downloaded FFNA models can supply
stream `1` from inline icon chunk `0xFA3` and stream `0xC` from dye-mask chunk
`0xFAA`. Chunk lengths are validated before exposing their payloads. This also
lets downloaded item icons use the existing dye-blending path.

Every first accepted base-ID request also schedules a best-effort native
submission on the game thread. Submission waits for a loaded map, a game context,
and enabled downloads. The client performs dependency expansion, archive writes,
and derived-stream generation. A native submission is not a persistence
acknowledgment: the game still needs to finish its download and archive work.
There are no download completion hooks and no DAT index refreshes.

## Reverse-engineering anchors

The investigation used Ghidra project program `/Gw.39112.exe`, image base
`0x00400000`. These VAs are references, not hard-coded runtime addresses:

| VA | Role |
| --- | --- |
| `0x00832860` | DnApi asynchronous batch queue |
| `0x00833320` | Internal submission; `uint32_t __cdecl(uint32_t count, const uint32_t* base_ids, uint32_t flags)` |
| `0x007D2C30` | Check archive state and dispatch missing requests |
| `0x007D5A10` | Decompression/delta application and CRC validation |
| `0x007D57A0` | Asset processing, stream writes, and alias linking |
| `0x0079A300` | Extract `0xFA3`/`0xFAA` into streams `1`/`0xC` |

The module locates submission through the `DnApi.cpp` assertion
`count && DownloadIsEnabled()`. It validates the nearby absolute download-disable
guard before using the function. If scanning fails, native submission is disabled
while standalone fetching remains available. Flags zero select normal async work;
no loading-screen or cancelable/low-priority flags are introduced.

The preceding RE exercised the TTS downloader against live servers: model base
`204020` resolved to revision `379813`, and item assets `193281` and `157784`
contained checksum-valid inline `ATEXDXT3` icons. The new C++ module still needs
an in-game check of the scanned native call, missing-asset texture completion,
and unload during a transfer. Static checks do not establish those runtime results.
