#include "stdafx.h"

#include "mp3.h"
#include <uuids.h>

Mp3::Mp3()
{
    pigb = nullptr;
    pimc = nullptr;
    pimex = nullptr;
    piba = nullptr;
    pims = nullptr;
    ready = false;
    duration = 0;
}

Mp3::~Mp3()
{
    Cleanup();
}

void Mp3::Cleanup()
{
    if (pimc) {
        pimc->Stop();
    }

    if (pigb) {
        pigb->Release();
        pigb = nullptr;
    }

    if (pimc) {
        pimc->Release();
        pimc = nullptr;
    }

    if (pimex) {
        pimex->Release();
        pimex = nullptr;
    }

    if (piba) {
        piba->Release();
        piba = nullptr;
    }

    if (pims) {
        pims->Release();
        pims = nullptr;
    }
    ready = false;
}

bool Mp3::Load(const LPCWSTR szFile)
{
    Cleanup();
    ready = false;
    if (SUCCEEDED(CoCreateInstance(CLSID_FilterGraph,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_IGraphBuilder,
        (void**)&this->pigb))) {
        pigb->QueryInterface(IID_IMediaControl, (void**)&pimc);
        pigb->QueryInterface(IID_IMediaEventEx, (void**)&pimex);
        pigb->QueryInterface(IID_IBasicAudio, (void**)&piba);
        pigb->QueryInterface(IID_IMediaSeeking, (void**)&pims);

        const HRESULT hr = pigb->RenderFile(szFile, nullptr);
        if (SUCCEEDED(hr)) {
            ready = true;
            if (pims) {
                pims->SetTimeFormat(&TIME_FORMAT_MEDIA_TIME);
                pims->GetDuration(&duration); // returns 10,000,000 for a second.
            }
        }
    }
    return ready;
}

bool Mp3::Play() const
{
    if (ready && pimc) {
        const HRESULT hr = pimc->Run();
        return SUCCEEDED(hr);
    }
    return false;
}

bool Mp3::Pause() const
{
    if (ready && pimc) {
        const HRESULT hr = pimc->Pause();
        return SUCCEEDED(hr);
    }
    return false;
}

bool Mp3::Stop() const
{
    if (ready && pimc) {
        const HRESULT hr = pimc->Stop();
        return SUCCEEDED(hr);
    }
    return false;
}

bool Mp3::WaitForCompletion(const long msTimeout, long* EvCode) const
{
    // @Cleanup: Add some logging
    if (ready && pimex) {
        const HRESULT hr = pimex->WaitForCompletion(msTimeout, EvCode);
        if (FAILED(hr)) {
            return false;
        }
        return *EvCode > 0;
    }

    return false;
}

bool Mp3::SetVolume(const long vol) const
{
    if (ready && piba) {
        const HRESULT hr = piba->put_Volume(vol);
        return SUCCEEDED(hr);
    }
    return false;
}

long Mp3::GetVolume() const
{
    if (ready && piba) {
        long vol = -1;
        const HRESULT hr = piba->get_Volume(&vol);

        if (SUCCEEDED(hr)) {
            return vol;
        }
    }

    return -1;
}

__int64 Mp3::GetDuration() const
{
    return duration;
}

__int64 Mp3::GetCurrentPosition() const
{
    if (ready && pims) {
        __int64 curpos = -1;
        const HRESULT hr = pims->GetCurrentPosition(&curpos);

        if (SUCCEEDED(hr)) {
            return curpos;
        }
    }

    return -1;
}

bool Mp3::SetPositions(__int64* pCurrent, __int64* pStop, const bool bAbsolutePositioning) const
{
    if (ready && pims) {
        DWORD flags = 0;
        if (bAbsolutePositioning) {
            flags = AM_SEEKING_AbsolutePositioning | AM_SEEKING_SeekToKeyFrame;
        }
        else {
            flags = AM_SEEKING_RelativePositioning | AM_SEEKING_SeekToKeyFrame;
        }

        const HRESULT hr = pims->SetPositions(pCurrent, flags, pStop, flags);

        if (SUCCEEDED(hr)) {
            return true;
        }
    }

    return false;
}
