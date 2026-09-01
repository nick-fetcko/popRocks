#ifdef WIN32
#pragma once

#include <windows.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <stdio.h>
#include <avrt.h>
#include <mutex>

#include <bass.h>

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
		nChannels = format->nChannels;

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
		for(UINT32 i = 0; i < numFramesAvailable * nChannels; ++i) {
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

	bool loopback = false;

	std::wstring deviceName;

private:
	std::size_t bufferLength = 0;

	WORD nChannels = 2;
};

HRESULT RecordAudioStream(MyAudioSink *pMySink);

#endif