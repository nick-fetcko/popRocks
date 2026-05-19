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
#ifdef WIN32
#endif
#include <glad/glad.h>
#include <SDL3_net/SDL_net.h>

#ifdef WIN32
#endif
#include <SDL3/SDL.h>

#ifndef __ANDROID__

#else
#include <EGL/egl.h>
#endif

#include "MathCPP/Duration.hpp"

#include "OpenGL/Context.hpp"
#include "OpenGL/Interops/Vulkan.hpp"
#include "OpenGL/Polyline.hpp"

#include "Utils/Logger.hpp"

#include "AlbumArt.hpp"
#include "BeatDetect.hpp"
#include "Circle.hpp"
#include "Close.hpp"
#include "ColorChangeListener.hpp"
#include "Controls.hpp"
#include "DoubleClick.hpp"
#include "DynamicGain.hpp"
#include "FPSCounter.hpp"
#include "LightPack.hpp"
#include "Mappings.h"
#include "Menu.hpp"
#include "Metadata.hpp"
#include "MT19937.hpp"
#include "Playlist.hpp"
#include "Preset.hpp"
#include "Renderer.hpp"
#include "SampleRNG.hpp"
#include "TestRNG.hpp"
#include "Text.hpp"
#include "Volume.hpp"

#ifdef __ANDROID__
#define GUI 1
#define VULKAN 0
#else
#define GUI 1
#define VULKAN 1
#endif

// Platform-specific code
// 
// This has to be _after_ the VULKAN
// declaration.
#if defined(WIN32)
#include "Platforms/Windows.hpp"
#elif defined (__ANDROID__)
#include "Platforms/Android.hpp"
#elif defined (USING_FLATPAK)
#include "Platforms/Flatpak.hpp"
#else
#include "Platforms/Linux.hpp"
#endif

using namespace MathsCPP;
using namespace Fetcko;

class MyAudioSink;

class CApp : public ColorChangeListener, public AlbumArt::BlackChangedListener, public LoggableClass {
public:
	CApp();
	~CApp();

	float GetScale(SDL_Window *window, int *w = nullptr, int *h = nullptr);
	const float &GetScale() const { return scale; }

	void OnInit();
	void OnResize(int width, int height, float scale = 1.0f, bool force = false);
	void OnLoop(const Delta &time); 
	void OnDestroy(bool includingLog = true);

	void LoadFile(std::filesystem::path path, bool fromPlaylist = false);
	void PrepareFile(std::wstring fileName);
	void PlayPreparedFile();

	void SetColor(int r, int g, int b);

	HSTREAM GetStreamHandle() const;
	HSTREAM GetNextStreamHandle() const;

	const std::size_t GetBufferLength() const { return bufferLength; }
	void SetBufferLength(std::size_t bufferLength);
	void SetFftLength(std::size_t fftLength);

	const uint32_t GetFftFlag() const { return fftFlag; }

	void SetRotating(bool rotating);
	void SetRotationSpeed(float speed);
	bool GetRotating() const { return rotating; }

	void SetDecayTime(Duration<Microseconds> time);
	void SetFadeTime(Duration<Microseconds> time);

	void SetStrobe(bool strobe);
	bool GetStrobe() const { return strobe; }

	AlbumArt &GetAlbumArt() { return albumArt; }

	void OnColorChanged(const MathsCPP::Colour<float> &color, bool silent = false) override;

	const bool GetBlur() const { return blur; }
	void SetBlur(bool blur);
	void ToggleBlur();
	void SetBlurIntensity(float intensity);

	void Seek(double seconds);

	std::pair<int, int> GetWindowSize() { return { windowWidth, windowHeight }; }

	Controls &GetControls() { return controls; }
	void FadeControls(bool in);
	void TogglePlaying();
	void SetPlaying(bool playing) { this->playing = playing; }

	void ToggleFullscreen();

	const Renderer *GetRenderer() const { return renderer; }

	void NextTrack();
	void PreviousTrack();

	bool OnMouseClicked(const Vector2i &mousePos);
	bool OnMouseDown(const Vector2i &mousePos);
	void OnMouseUp(const Vector2i &mousePos);
	void OnMouseMoved(const Vector2i &mousePos);
	bool OnMouseDragged(const Vector2i &mousePos);
	void OnMouseLeave();

	void LoadPreset(std::optional<std::size_t> index);
	void LoadPreset(const Preset &preset);

	void AdvanceToNextTrack();

	bool Open(const std::filesystem::path &path, const std::string &extension, bool exclusive, HSTREAM &target, bool force = false);

	void StopExclusive();

	void UpdateUi() { if (updateUi == 0) updateUi = 1; }

	void SyncToNearestBeat();

	std::mutex &GetStreamHandleMutex() { return streamHandleMutex; }

	void SaveBlurFBO();

	void UpdateHdrProperties(bool force = false);

	void UpdateVsync();

	Menu &GetMenu() { return menu; }

	std::unique_ptr<MultisampledFramebufferObject> &GetBlurFbo() { return blurFbo; }
	void SetBlurFbo(std::unique_ptr<MultisampledFramebufferObject> &&blurFbo) { this->blurFbo = std::move(blurFbo); }
	std::unique_ptr<MultisampledFramebufferObject> &GetLastFrame() { return lastFrame; }
	void SetLastFrame(std::unique_ptr<MultisampledFramebufferObject> &&lastFrame) { this->lastFrame = std::move(lastFrame); }
	std::unique_ptr<MultisampledFramebufferObject> &GetUiFbo() { return uiFbo; }
	void SetUiFbo(std::unique_ptr<MultisampledFramebufferObject> &&uiFbo) { this->uiFbo = std::move(uiFbo); }

	SDL_GLContext GetOpenGlContext() { return openGlContext; }
	void SetOpenGlContext(SDL_GLContext openGlContext) { this->openGlContext = openGlContext; }
	std::unique_ptr<Context> &GetContext() { return context; }

	SDL_Window *GetSdlWindow() { return sdlWindow; }
	void SetSdlWindow(SDL_Window *sdlWindow) { this->sdlWindow = sdlWindow; }

	SDL_Window *GetOpenGlWindow() { return openGlWindow; }
	void SetOpenGlWindow(SDL_Window *openGlWindow) { this->openGlWindow = openGlWindow; }

	const int GetMaxDimension() const { return maxDimension; }

	const bool GetPulseBackground() const { return pulseBackground; }

	void SeekTo(double seconds);

	const std::filesystem::path &GetLoadedFile() const { return loadedFile; }
	const std::string &GetLoadedFileExtension() const { return loadedFileExtension; }

	HSTREAM &GetStreamHandle() { return streamHandle; }

	const BASS_CHANNELINFO &GetChannelInfo() const { return channelInfo; }

	void ToggleExclusive();

	std::unique_ptr<Platform> &GetPlatform() { return platform; }

	const bool &GetMiniPlayer() const { return miniPlayer; }
	void SetMiniPlayer(bool miniPlayer, bool inLoop = false);

	const bool IsFileLoaded() const { return fileLoaded || platform->IsListening(); }

	const float &GetBackgroundAlpha() const { return backgroundAlpha; }

	void UpdateDisplayBoundingBox();

	void ResetWindow();

	void OnBlackChanged(const float &black) override;

	void SetVulkan(bool vulkan);
	const bool &GetVulkan() const { return vulkan; }

	void SetSourceFactor(GLenum factor) { this->sourceFactor = factor; }
	void SetDestFactor(GLenum factor) { this->destFactor = factor; }
	void SetSourceAlphaFactor(GLenum factor) { this->sourceAlphaFactor = factor; }
	void SetDestAlphaFactor(GLenum factor) { this->destAlphaFactor = factor; }

private:
	void AddCommands();

	const Colour<float> &GetColor() const;
	void SetColor(float alpha = 1.0f) const;

	// We need to set our buffer to whichever
	// is larger out of bufferLength and fftLength
	void UpdateMaxBufferLength();

	inline void SwapBuffers(const Delta &time);

	void Stop(BOOL reset = TRUE);
	void StopExclusive(BOOL reset);

	void LoadBeats(HSTREAM streamHandle, std::filesystem::path path, bool pingPong = true);
	void ResetBeatDetection();

	inline bool SeekToMousePos(const Vector2i &mousePos, bool ignoreY = false);

	inline void CacheBlurUniforms(Context::Shader &shader);

	inline void SetEffect(const std::string &effect);

	inline void LoadRandomPreset();
	inline void UpdateBeatCounter();

	inline void LoadRenderer(const std::string &rendererName);

	inline void SetVisualizerScale(float scale);

	inline void ClearBlurFbo();

	inline void LoadShaders();

	inline void SetHdr(bool enabled);

	inline SDL_PropertiesID CreateSdlWindow();

	Interop::InitArgs GetInteropArgs();

	inline void SetRadius(float radius);
	inline void UpdateBleedEdge(float radius);

	inline void UpdateMiniPlayer();

	inline void DrawCloseButton(const Delta &time);
	inline bool IsOnCloseButton(const Vector2i &mousePos);

	int windowWidth = 1920;
	int windowHeight = 1080;

	std::size_t bufferLength = 0;

	bool fileLoaded = false;
	std::filesystem::path loadedFile;
	std::string loadedFileExtension;
	int freq = 48000; // Sample rate (Hz)
	HSTREAM streamHandle = 0; // Handle for open stream
	HSTREAM nextStreamHandle = 0; // Handle for next track in playlist

	// We start assuming 2 channels
	BASS_CHANNELINFO channelInfo = { 0, 2, 0, 0, 0, 0, 0 };

	uint8_t *buffer = nullptr;
	float *floatBuffer = nullptr;
	short *shortBuffer = nullptr;

	LightPack lightPack;

	float hStep = 0.0f;
	SDL_Window *sdlWindow = nullptr;
	SDL_Window *openGlWindow = nullptr;
	Colour<float> visColor{ 0.0f, 0.5f, 1.0f };
	Colour<float> brightColor{ 0.0f, 0.0f, 0.0f };
	Colour<float> darkColor{ 0.0f, 0.0f, 0.0f };
	float fadeTime = 0.0f;
	float currentFadeTime = 0.0f;

	std::wstring savedFile;

	float frameCount = 0.0f;
	bool rotating = Settings::settings.GetRotating();

	std::size_t fftLength = 0;
	uint32_t fftFlag = BASS_DATA_FFT8192;

	AlbumArt albumArt;

	// In degrees per second
	float rotationSpeed = Settings::settings.GetRotationSpeed();

	bool strobe = Settings::settings.GetStrobe();
	float strobeIntensity = Settings::settings.GetStrobeIntensity();

	std::atomic<bool> shuttingDown = false;
	bool destroyed = false;

	bool blur = Settings::settings.GetBlur();

	// How many _seconds_ it takes for the blur to fade out
	float blurIntensity = Settings::settings.GetBlurIntensity();
	
	bool resetGain = false;

	DynamicGain<float> dynamicGain = Settings::settings.GetDynamicGain();

	double timeSinceLastColorChange = 0.0;

	bool overrideColor = false;

	bool attached = true;

	Renderer *renderer = nullptr;

	Controls controls;

	std::array<BeatDetect, 2> beatDetectors;
	BeatDetect *beatDetect = &beatDetectors[0];
	double beatDetectTime = 0.0;

	Metadata metadata;

	Fetcko::Polyline circleLine;

	std::optional<std::size_t> presetIndex = Settings::settings.GetPresetIndex();

	std::atomic<bool> advanceOnNextLoop = false;
	std::atomic<bool> stopWasapiOnNextLoop = false;

	float originalScale = 1.0f;
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

	std::unique_ptr<PRNG<unsigned int>> prng;

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

	GLenum sourceFactor = Settings::settings.GetSourceFactor();
	GLenum destFactor = Settings::settings.GetDestFactor();
	GLenum sourceAlphaFactor = Settings::settings.GetSourceAlphaFactor();
	GLenum destAlphaFactor = Settings::settings.GetDestAlphaFactor();

	float uiGamma = Settings::settings.GetUiGamma();
	float uiContrast = Settings::settings.GetUiContrast();
	float uiBrightness = Settings::settings.GetUiBrightness();

	SDL_GLContext openGlContext;

	// Is this running on a Steam Deck?
	bool steamDeck = false;

	float safeAreaPadding = 0.0f;

	std::unique_ptr<Platform> platform;

	DoubleClick doubleClick;

	bool pulseMaxBrightness = HDR::Enabled && Settings::settings.GetPulseMaxBrightness();

	bool miniPlayer = Settings::settings.GetMiniPlayer();
	std::optional<Vector2i> lastMousePos = std::nullopt;

	float backgroundAlpha = Settings::settings.GetMiniPlayer() ? 0.0f : 1.0f;

	int windowX = 0, windowY = 0;

	float miniPlayerVisualizerRatio = Settings::settings.GetMiniPlayerVisualizerRatio();

	Close close;

	std::optional<float> scaleDelta = std::nullopt;

	Rectanglei displayBoundingBox = { 0, 0, 0, 0 };

	double mouseCaptureAccum = 0.0;

	bool updateRenderer = false;

	bool vulkan = Settings::settings.GetVulkan();

	std::optional<std::chrono::system_clock::time_point> scaleTimer = std::nullopt;
};
