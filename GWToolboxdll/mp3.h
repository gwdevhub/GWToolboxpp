#pragma once

#include <windows.h>
#include <strmif.h>
#include <control.h>

#pragma comment(lib, "strmiids.lib")

class Mp3 {
public:
    Mp3();
    ~Mp3();

    bool Load(LPCWSTR filename);
    void Cleanup();

    bool Play() const;
    bool Pause() const;
    bool Stop() const;

    // Poll this function with msTimeout = 0, so that it return immediately.
    // If the mp3 finished playing, WaitForCompletion will return true;
    bool WaitForCompletion(long msTimeout, long* EvCode) const;

    // -10000 is lowest volume and 0 is highest volume, positive value > 0 will fail
    bool SetVolume(long vol) const;

    // -10000 is lowest volume and 0 is highest volume
    long GetVolume() const;

    // Duration in units of 100ns (10,000,000 == 1 second).
    __int64 GetDuration() const;

    // Playing position in units of 100ns (10,000,000 == 1 second).
    __int64 GetCurrentPosition() const;

    // Equal pCurrent/pStop values seek then stop playing; they must still be two
    // distinct pointers.
    bool SetPositions(__int64* pCurrent, __int64* pStop, bool bAbsolutePositioning) const;

private:
    IGraphBuilder* pigb;
    IMediaControl* pimc;
    IMediaEventEx* pimex;
    IBasicAudio* piba;
    IMediaSeeking* pims;
    bool ready;
    __int64 duration;
};
