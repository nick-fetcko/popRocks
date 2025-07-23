#pragma once

#include <iostream>
#include <array>
#include <map>
#include <vector>
#include <optional>
#include <thread>

#include <string>
#include <bass.h>
#include <bassflac.h>
#include <basswasapi.h>

#include <glad/glad.h>
#include <SDL_net.h>
#include <SDL_ttf.h>

#include <Mmdeviceapi.h>

#include <SDL.h>

#include "FFtw3.h"

#include "MathCPP/Duration.hpp"

#include "AlbumArt.hpp"
#include "BeatDetect.hpp"
#include "CConsole.h"
#include "ColorChangeListener.hpp"
#include "Controls.hpp"
#include "DynamicGain.hpp"
#include "FPSCounter.hpp"
#include "LightPack.hpp"
#include "Mappings.h"
#include "Metadata.hpp"
#include "Playlist.hpp"
#include "Polyline.hpp"
#include "Preset.hpp"
#include "Renderer.hpp"
#include "Text.hpp"
#include "Volume.hpp"

using namespace MathsCPP;

class MyAudioSink;

DWORD CALLBACK InWasapiProc(void*, DWORD, void*);

class CApp : public ColorChangeListener {
public:
	CApp();
	~CApp();

	float GetScale(SDL_Window *window, int *w = nullptr, int *h = nullptr);
	const float &GetScale() const { return scale; }

	void OnInit();
	void OnResize(int width, int height, float scale = 1.0f);
	void OnLoop(const Delta &time); 
	void OnDestroy();

	void LoadFile(std::filesystem::path path, bool fromPlaylist = false);
	void PrepareFile(std::wstring fileName);
	void PlayPreparedFile();

	void SetColor(int r, int g, int b);

	void Listen();

	HSTREAM GetStreamHandle() const;

	void SetBufferLength(std::size_t bufferLength);
	void SetFftLength(std::size_t fftLength);

	void SetRotating(bool rotating) { this->rotating = rotating; }
	void SetRotationSpeed(float speed) { this->rotationSpeed = speed; }
	bool GetRotating() const { return rotating; }

	void SetDecayTime(Duration<Microseconds> time);
	void SetFadeTime(Duration<Microseconds> time);

	void SetGain(float gain) { this->gain = gain; }

	void SetStrobe(bool strobe) { this->strobe = strobe; }
	bool GetStrobe() const { return strobe; }
	void SetStrobeFrequency(Duration<Microseconds> freq);

	AlbumArt &GetAlbumArt() { return albumArt; }

	void OnColorChanged(const MathsCPP::Colour<float> &color, bool silent = false) override;

	void ToggleBlur();
	void SetBlurIntensity(float intensity);

	void Seek(double seconds);

	std::pair<int, int> GetWindowSize() { return { windowWidth, windowHeight }; }

	Controls &GetControls() { return controls; }
	void FadeControls(bool in);
	void TogglePlaying();

	void ToggleFullscreen();

	const Renderer *GetRenderer() const { return renderer; }

	void NextTrack();
	void PreviousTrack();

	void OnMouseClicked(const Vector2i &mousePos);

	void LoadPreset(std::size_t index);

	void AdvanceToNextTrack();

	void Open(const std::filesystem::path &path, const std::string &extension, bool exclusive);

	void StopExclusive();

private:
	void AddCommands();

	const Colour<float> &GetColor() const;
	void SetColor(float alpha = 1.0f) const;

	// We need to set our buffer to whichever
	// is larger out of bufferLength and fftLength
	void UpdateMaxBufferLength();

	void SeekTo(double seconds);

	inline void SwapBuffers();

	HSTREAM OpenWithFlags(const std::filesystem::path &path, const std::string &extension, DWORD flags);
	
	void Stop(BOOL reset = TRUE);
	void StopExclusive(BOOL reset);
	
	inline void Unmute();

	inline void LoadBeats(HSTREAM streamHandle, std::filesystem::path path);
	void ResetBeatDetection();

	int windowWidth = 1920;
	int windowHeight = 1080;

	std::size_t bufferLength = 0;

	bool fileLoaded = false;
	std::filesystem::path loadedFile;
	std::string loadedFileExtension;
	int device = -1; // Default Sounddevice
	int freq = 48000; // Sample rate (Hz)
	HSTREAM streamHandle = NULL; // Handle for open stream
	BASS_CHANNELINFO channelInfo = { 0 };

	uint8_t *buffer = nullptr;
	float *floatBuffer = nullptr;
	short *shortBuffer = nullptr;

	LightPack lightPack;

	float hStep = 0.0f;
	SDL_Window *sdlWindow = nullptr;
	Colour<float> visColor{ 0.0f, 0.5f, 1.0f };

	std::wstring savedFile;

	IMMDevice *audioDevice = nullptr;
	MyAudioSink *audioSink = nullptr;

	double *in = nullptr;
	fftw_complex *out = nullptr;
    fftw_plan plan = nullptr;

	bool listening = false;
	float maxHeardSample = 0.0f;
	std::thread listenThread;

	float frameCount = 0.0f;
	bool rotating = false;

	std::size_t fftLength = 0;
	uint32_t fftFlag = BASS_DATA_FFT8192;

	AlbumArt albumArt;

	float gain = 20.0f;

	// In degrees per second
	float rotationSpeed = 5.0f;

	bool strobe = false;
	Duration<Microseconds> strobeAccum;
	Duration<Microseconds> strobeFrequency = 1s;

	std::atomic<bool> shuttingDown = false;

	bool blur = false;

	float blurIntensity = 0.88f;
	
	bool resetGain = false;

	// This config has much more aggressive normalization
//	DynamicGain<float> dynamicGain{ 
//		0.001f, 0.000001f, std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest(), true, true, true
//	};

	// This config gives the "original", less normalized look
	DynamicGain<float> dynamicGain { 
		0.001f, 0.0000001f, 0.0f, 0.0f, false, true, true
	};

	double timeSinceLastColorChange = 0.0;

	bool overrideColor = false;

	std::size_t maxLength = 0;
	bool attached = true;

	Renderer *renderer = nullptr;

	Controls controls;

	std::array<BeatDetect, 2> beatDetectors;
	BeatDetect *beatDetect = &beatDetectors[0];

	Metadata metadata;

	Polyline circleLine;

	float exclusiveBufferSize = 0.25f; // in seconds

	std::size_t presetIndex = 0;

	std::atomic<bool> advanceOnNextLoop = false;
	std::atomic<bool> stopWasapiOnNextLoop = false;

	BASS_WASAPI_INFO wasapiInfo{ 0 };

	float scale = 1.0f;
};
