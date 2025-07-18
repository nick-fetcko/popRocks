#pragma once

#include <windows.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <stdio.h>
#include <avrt.h>
#include <mutex>

#include "CApp.h"

class MyAudioSink {
public:
	float *buffer = nullptr;

	MyAudioSink(std::size_t bufferLength) : bufferLength(bufferLength) {
		currentBufferPos = 0;
		dataChanged = false;
		buffer = new float[bufferLength];
		memset(buffer, 0, sizeof(float) * bufferLength);
		//this->streamHandle = streamHandle;
	}
	~MyAudioSink() {
		delete[] buffer;
	}

	HRESULT SetFormat(WAVEFORMATEX *format) {
		// Don't worry, everything is gonna happy <3
		return S_OK;
	}

	HRESULT CopyData(BYTE *pData, UINT32 numFramesAvailable, BOOL *pDone) {
		if (!pData) return S_FALSE;

		std::unique_lock lock(mutex);

		auto currentPos = reinterpret_cast<float*>(pData);
		//std::cout << numFramesAvailable << std::endl;
		//buffer[0] = (*currentPos)*9000.0f;
		//BASS_ERROR_HANDLE
		//int ret = BASS_StreamPutData(streamHandle, pData, numFramesAvailable*sizeof(float)*2);
		for(UINT32 i = 0; i < numFramesAvailable; ++i) {
			buffer[currentBufferPos++] = *(currentPos++);
			if(currentBufferPos == bufferLength)
				currentBufferPos = 0;
		}
		
		dataChanged = true;

		return S_OK;
	}
	int currentBufferPos = 0;
	bool dataChanged = false;
	HSTREAM streamHandle = NULL;

	std::mutex mutex;

	std::atomic<bool> done = false;

private:
	std::size_t bufferLength = 0;
};

// Below was derived from
// https://learn.microsoft.com/en-us/windows/win32/coreaudio/capturing-a-stream

//-----------------------------------------------------------
// Record an audio stream from the default audio capture
// device. The RecordAudioStream function allocates a shared
// buffer big enough to hold one second of PCM audio data.
// The function uses this buffer to stream data from the
// capture device. The main loop runs every 1/2 second.
//-----------------------------------------------------------

// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC  10000000
#define REFTIMES_PER_MILLISEC  10000

#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { goto Exit; }
#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

HRESULT RecordAudioStream(MyAudioSink *pMySink)
{
    HRESULT hr;
    REFERENCE_TIME hnsRequestedDuration = REFTIMES_PER_SEC / 2;
    REFERENCE_TIME hnsActualDuration;
    UINT32 bufferFrameCount;
    UINT32 numFramesAvailable;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IAudioClient *pAudioClient = NULL;
    IAudioCaptureClient *pCaptureClient = NULL;
    WAVEFORMATEX *pwfx = NULL;
    UINT32 packetLength = 0;
    BOOL bDone = FALSE;
    BYTE *pData;
    DWORD flags;

    hr = CoCreateInstance(
           CLSID_MMDeviceEnumerator, NULL,
           CLSCTX_ALL, IID_IMMDeviceEnumerator,
           (void**)&pEnumerator);
    EXIT_ON_ERROR(hr)

    hr = pEnumerator->GetDefaultAudioEndpoint(
	eCapture, eConsole, &pDevice);
    EXIT_ON_ERROR(hr)

    hr = pDevice->Activate(
                    IID_IAudioClient, CLSCTX_ALL,
                    NULL, (void**)&pAudioClient);
    EXIT_ON_ERROR(hr)

    hr = pAudioClient->GetMixFormat(&pwfx);
	EXIT_ON_ERROR(hr)

	WAVEFORMATEXTENSIBLE *extensible = nullptr;
	if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		extensible = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(pwfx);
		if (extensible->SubFormat != KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
			goto Exit;
	}

    hr = pAudioClient->Initialize(
                         AUDCLNT_SHAREMODE_SHARED,
                         0,
                         hnsRequestedDuration,
                         0,
                         pwfx,
                         NULL);
    EXIT_ON_ERROR(hr)

    // Get the size of the allocated buffer.
    hr = pAudioClient->GetBufferSize(&bufferFrameCount);
    EXIT_ON_ERROR(hr)

    hr = pAudioClient->GetService(
                         IID_IAudioCaptureClient,
                         reinterpret_cast<void**>(&pCaptureClient));
    EXIT_ON_ERROR(hr)

    // Notify the audio sink which format to use.
    hr = pMySink->SetFormat(pwfx);
    EXIT_ON_ERROR(hr)

    // Calculate the actual duration of the allocated buffer.
    hnsActualDuration = static_cast<REFERENCE_TIME>(
		static_cast<double>(REFTIMES_PER_SEC) *
		bufferFrameCount / pwfx->nSamplesPerSec
	);

    hr = pAudioClient->Start();  // Start recording.
    EXIT_ON_ERROR(hr)

    // Each loop fills about half of the shared buffer.
    while (bDone == FALSE && !pMySink->done)
    {
		// Sleep for half the buffer duration.
		//Sleep(hnsActualDuration/REFTIMES_PER_MILLISEC/2);
		//Sleep(16);

        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        EXIT_ON_ERROR(hr)

        while (packetLength != 0)
        {
            // Get the available data in the shared buffer.
            hr = pCaptureClient->GetBuffer(
                                   &pData,
                                   &numFramesAvailable,
                                   &flags, NULL, NULL);
            EXIT_ON_ERROR(hr)

            if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
            {
                pData = NULL;  // Tell CopyData to write silence.
            }

            // Copy the available capture data to the audio sink.
            hr = pMySink->CopyData(
                              pData, numFramesAvailable, &bDone);
            EXIT_ON_ERROR(hr)

            hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
            EXIT_ON_ERROR(hr)

            hr = pCaptureClient->GetNextPacketSize(&packetLength);
            EXIT_ON_ERROR(hr)
        }
    }

    hr = pAudioClient->Stop();  // Stop recording.
    EXIT_ON_ERROR(hr)

Exit:
    CoTaskMemFree(pwfx);
    SAFE_RELEASE(pEnumerator)
    SAFE_RELEASE(pDevice)
    SAFE_RELEASE(pAudioClient)
    SAFE_RELEASE(pCaptureClient)

    return hr;
}
