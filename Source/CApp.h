#pragma once

#include <iostream>
#include <array>
#include <map>
#include <vector>
#include <optional>
#include <thread>
#include <random>

#include <string>
#include <bass.h>
#include <bassflac.h>
#include <basswasapi.h>

#include <glad/glad.h>
#include <SDL_net.h>

#include <Mmdeviceapi.h>

#include <SDL.h>

#include "FFtw3.h"

#include "MathCPP/Duration.hpp"

#include "OpenGL/Context.hpp"
#include "OpenGL/Polyline.hpp"

#include "Utils/Logger.hpp"

#include "AlbumArt.hpp"
#include "BeatDetect.hpp"
#include "Circle.hpp"
#include "ColorChangeListener.hpp"
#include "Controls.hpp"
#include "DynamicGain.hpp"
#include "FPSCounter.hpp"
#include "LightPack.hpp"
#include "Mappings.h"
#include "Menu.hpp"
#include "Metadata.hpp"
#include "Playlist.hpp"
#include "Preset.hpp"
#include "Renderer.hpp"
#include "Text.hpp"
#include "Volume.hpp"

#define GUI 1

using namespace MathsCPP;
using namespace Fetcko;

class MyAudioSink;

DWORD CALLBACK InWasapiProc(void*, DWORD, void*);

class CApp : public ColorChangeListener, public LoggableClass {
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

	void Listen(bool loopback = false);
	inline void StopListening();

	HSTREAM GetStreamHandle() const;
	HSTREAM GetNextStreamHandle() const;

	void SetBufferLength(std::size_t bufferLength);
	void SetFftLength(std::size_t fftLength);

	void SetRotating(bool rotating);
	void SetRotationSpeed(float speed);
	bool GetRotating() const { return rotating; }

	void SetDecayTime(Duration<Microseconds> time);
	void SetFadeTime(Duration<Microseconds> time);

	void SetGain(float gain) { this->gain = gain; }

	void SetStrobe(bool strobe);
	bool GetStrobe() const { return strobe; }

	AlbumArt &GetAlbumArt() { return albumArt; }

	void OnColorChanged(const MathsCPP::Colour<float> &color, bool silent = false) override;

	void SetBlur(bool blur);
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
	bool OnMouseDown(const Vector2i &mousePos);
	void OnMouseDragged(const Vector2i &mousePos);

	void LoadPreset(std::optional<std::size_t> index);
	void LoadPreset(const Preset &preset);

	void AdvanceToNextTrack();

	bool Open(const std::filesystem::path &path, const std::string &extension, bool exclusive, HSTREAM &target, bool force = false);

	void StopExclusive();

	void UpdateUi() { updateUi = 1; }

	void SyncToNearestBeat();

	std::mutex &GetStreamHandleMutex() { return streamHandleMutex; }

	void SaveBlurFBO();

private:
	void AddCommands();

	const Colour<float> &GetColor() const;
	void SetColor(float alpha = 1.0f) const;

	// We need to set our buffer to whichever
	// is larger out of bufferLength and fftLength
	void UpdateMaxBufferLength();

	void SeekTo(double seconds);

	inline void SwapBuffers(const Delta &time);

	HSTREAM OpenWithFlags(const std::filesystem::path &path, const std::string &extension, DWORD flags);
	
	void Stop(BOOL reset = TRUE);
	void StopExclusive(BOOL reset);
	
	inline void Unmute();

	inline void LoadBeats(HSTREAM streamHandle, std::filesystem::path path, bool pingPong = true);
	void ResetBeatDetection();

	inline bool SeekToMousePos(const Vector2i &mousePos, bool ignoreY = false);

	template<bool Output>
	int GetDeviceIndex(const std::string &device);

	inline void CacheBlurUniforms(Context::Shader &shader);

	inline void SetEffect(const std::string &effect);

	inline void LoadRandomPreset();
	inline void UpdateBeatCounter();

	inline void ToggleExclusive();

	inline void LoadRenderer(const std::string &rendererName);

	inline void SetVisualizerScale(float scale);

	inline void ClearBlurFbo();

	int windowWidth = 1920;
	int windowHeight = 1080;

	std::size_t bufferLength = 0;

	bool fileLoaded = false;
	std::filesystem::path loadedFile;
	std::string loadedFileExtension;
	int freq = 48000; // Sample rate (Hz)
	HSTREAM streamHandle = NULL; // Handle for open stream
	HSTREAM nextStreamHandle = NULL; // Handle for next track in playlist

	// We start assuming 2 channels
	BASS_CHANNELINFO channelInfo = { 0, 2, 0, 0, 0, 0, 0 };

	uint8_t *buffer = nullptr;
	float *floatBuffer = nullptr;
	short *shortBuffer = nullptr;

	LightPack lightPack;

	float hStep = 0.0f;
	SDL_Window *sdlWindow = nullptr;
	Colour<float> visColor{ 0.0f, 0.5f, 1.0f };
	Colour<float> brightColor{ 0.0f, 0.0f, 0.0f };
	Colour<float> darkColor{ 0.0f, 0.0f, 0.0f };
	float fadeTime = 0.0f;
	float currentFadeTime = 0.0f;

	std::wstring savedFile;

	IMMDevice *audioDevice = nullptr;
	MyAudioSink *audioSink = nullptr;

	float *in = nullptr;
	fftwf_complex *out = nullptr;
	fftwf_plan plan = nullptr;

	bool listening = false;
	float maxHeardSample = 0.0f;
	std::thread listenThread;

	float frameCount = 0.0f;
	bool rotating = Settings::settings.GetRotating();

	std::size_t fftLength = 0;
	uint32_t fftFlag = BASS_DATA_FFT8192;

	AlbumArt albumArt;

	float gain = 20.0f;

	// In degrees per second
	float rotationSpeed = Settings::settings.GetRotationSpeed();

	bool strobe = Settings::settings.GetStrobe();
	float strobeIntensity = Settings::settings.GetStrobeIntensity();

	std::atomic<bool> shuttingDown = false;

	bool blur = Settings::settings.GetBlur();

	// How many _seconds_ it takes for the blur to fade out
	float blurIntensity = Settings::settings.GetBlurIntensity();
	
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
	double beatDetectTime = 0.0;

	Metadata metadata;

	Polyline circleLine;

	float exclusiveBufferSize = 0.25f; // in seconds

	std::optional<std::size_t> presetIndex = Settings::settings.GetPresetIndex();

	std::atomic<bool> advanceOnNextLoop = false;
	std::atomic<bool> stopWasapiOnNextLoop = false;

	BASS_WASAPI_INFO wasapiInfo{ 0 };

	float scale = 1.0f;

	std::unique_ptr<Context> context;

	bool playing = false;

	Circle<Circles::Plain> spindle;
	constexpr static float SpindleSize = 20.0f;

	std::unique_ptr<MultisampledFramebufferObject> blurFbo;
	std::unique_ptr<MultisampledFramebufferObject> lastFrame;

	Menu menu;

	uint8_t updateUi = 0;

	std::unique_ptr<MultisampledFramebufferObject> uiFbo;
	double uiAccum = 0.0;

	Vector<int, 2> blurOffset;
	int maxDimension = 0;
	float blurOpacity = Settings::settings.GetBlurOpacity();

	std::mt19937 prng;

	std::chrono::steady_clock::time_point frameStart;
	double frameLimit = Settings::settings.GetLimitFramerate() ? Settings::settings.GetFrameLimit() : -1;

	std::optional<Duration<Microseconds>> randomizeTime = 
		Settings::settings.GetRandomize() ?
			static_cast<std::optional<Duration<Microseconds>>>(Settings::settings.GetRandomizeTime()) :
			std::nullopt;

	std::chrono::system_clock::time_point lastRandomize;

	std::vector<std::size_t> shuffledPresets;
	std::optional<Duration<Microseconds>> randomizePresetsTime =
		Settings::settings.GetRandomizePresets() ?
		static_cast<std::optional<Duration<Microseconds>>>(Settings::settings.GetRandomizePresetsTime()) :
		std::nullopt;

	std::chrono::system_clock::time_point lastPresetRandomize;

	std::optional<int> randomizePresetsBeats =
		Settings::settings.GetRandomizePresetsByBeats() ?
		static_cast<std::optional<int>>(Settings::settings.GetRandomizePresetsBeats()) :
		std::nullopt;

	int beatCounter = 0;
	bool resyncBeats = false;

	std::mutex streamHandleMutex;

	bool darkenPulseOnBrightColors = Settings::settings.GetDarkenPulseOnBrightColors();

	bool pulseBackground = Settings::settings.GetPulseBackground();
};
