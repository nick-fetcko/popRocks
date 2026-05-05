#pragma once

#include <filesystem>

#include <SDL3/SDL_video.h>

#include <glad/glad.h>

#include "MathCPP/Duration.hpp"

#include "Serial/Json.hpp"

#include "Utils/Logger.hpp"

using namespace std::chrono_literals;

using namespace Fetcko;
using namespace MathsCPP;
using namespace serial;

class Settings : public LoggableClass {
public:
	static std::map<GLenum, std::string> BlendModes;
	static std::map<std::string, GLenum> Colorspaces;

	struct ColorSelection {
		double minPercentage = 0.02;
		double minSaturation = 0.1;
		double minValue = 0.25;
		double minHueSeparation = 25.0;
		double minValueSeparation = 0.1;
		double minRgbSeparation = 0.70;
		double maxAverageColorVariance = 2.34;
		double maxPerPixelColorVariance = 1.0;

		bool operator !=(const ColorSelection &right) {
			return right.minPercentage != minPercentage ||
				right.minSaturation != minSaturation ||
				right.minValue != minValue ||
				right.minHueSeparation != minHueSeparation ||
				right.minValueSeparation != minValueSeparation ||
				right.minRgbSeparation != minRgbSeparation ||
				right.maxAverageColorVariance != maxAverageColorVariance ||
				right.maxPerPixelColorVariance != maxPerPixelColorVariance;
		}
	};

	static Settings settings;

	static constexpr bool IsColorBlend(GLenum blend) {
		return
			blend == GL_SRC_COLOR ||
			blend == GL_ONE_MINUS_SRC_COLOR ||
			blend == GL_DST_COLOR ||
			blend == GL_ONE_MINUS_DST_COLOR ||
			blend == GL_CONSTANT_COLOR ||
			blend == GL_ONE_MINUS_CONSTANT_COLOR
#ifndef __ANDROID__
			||
			blend == GL_SRC1_COLOR ||
			blend == GL_ONE_MINUS_SRC1_COLOR
#endif
			;
	}

	static std::optional<GLenum> GetEnumForColorspace(const std::string &string) {
		if (auto iter = Colorspaces.find(string); iter != Colorspaces.end())
			return iter->second;

		return std::nullopt;
	}

	static void SetPath(const std::string &path);

	Settings();

	Settings(const Settings &) = default;
	Settings &operator=(const Settings &) = default;

	Settings(Settings &&other) noexcept : LoggableClass(std::move(other)) {
		// Hack to avoid having to manually
		// move every variable
		auto temp = logger;

		*this = std::move(other);

		logger = temp;
		logger->SetObject(this);
	}

	Settings &operator=(Settings &&) = default;

	void Save();

	const float &GetVolume() const { return volume; }
	void SetVolume(float volume, bool delayed = false);

	const bool &GetExclsuive() const { return exclusive; }
	void SetExclusive(bool exclusive, bool delayed = false);

	const ColorSelection &GetColorSelection() const { return colorSelection; }
	void SetColorSelection(ColorSelection colorSelection, bool delayed = false);

	const int &GetWindowWidth() const { return windowWidth; }
	void SetWindowWidth(int windowWidth, bool delayed = false);

	const int &GetWindowHeight() const { return windowHeight; }
	void SetWindowHeight(int windowHeight, bool delayed = false);

	const int &GetWindowX() const { return windowX; }
	void SetWindowX(int windowX, bool delayed = false);

	const int &GetWindowY() const { return windowY; }
	void SetWindowY(int windowY, bool delayed = false);

	const std::size_t &GetBufferLength() { return bufferLength; }
	void SetBufferLength(std::size_t bufferLength, bool delayed = false);

	const Duration<Microseconds> &GetDecayTime() { return decayTime; }
	void SetDecayTime(Duration<Microseconds> decayTime, bool delayed = false);

	const Duration<Microseconds> &GetFadeTime() { return fadeTime; }
	void SetFadeTime(Duration<Microseconds> fadeTime, bool delayed = false);

	const bool &GetPulse() const { return pulse; }
	void SetPulse(bool pulse, bool delayed = false);

	const bool &GetPulseBackground() const { return pulseBackground; }
	void SetPulseBackground(bool pulseBackground, bool delayed = false);

	const bool &GetDarkenPulseOnBrightColors() const { return darkenPulseOnBrightColors; }
	void SetDarkenPulseOnBrightColors(bool darkenPulseOnBrightColors, bool delayed = false);

	const Duration<Microseconds> &GetPulseTime() { return pulseTime; }
	void SetPulseTime(Duration<Microseconds> pulseTime, bool delayed = false);

	const bool &GetStrobe() const { return strobe; }
	void SetStrobe(bool strobe, bool delayed = false);

	const float &GetStrobeIntensity() const { return strobeIntensity; }
	void SetStrobeIntensity(float strobeIntensity, bool delayed = false);

	const bool &GetRotating() const { return rotating; }
	void SetRotating(bool rotating, bool delayed = false);

	const float &GetRotationSpeed() const { return rotationSpeed; }
	void SetRotationSpeed(float rotationSpeed, bool delayed = false);

	const float &GetRadius() const { return radius; }
	void SetRadius(float radius, bool delayed = false);

	const bool &GetDetectBpm() const { return detectBpm; }
	void SetDetectBpm(bool detectBpm, bool delayed = false);

	const bool &GetCacheDetectionResults() const { return cacheDetectionResults; }
	void SetCacheDetectionResults(bool cacheDetectionResults, bool delayed = false);

	const bool &GetHalveBpm() const { return halveBpm; }
	void SetHalveBpm(bool halveBpm, bool delayed = false);

	const float &GetWidth() const { return width; }
	void SetWidth(float width, bool delayed = false);

	const std::string &GetRenderer() const { return renderer; }
	void SetRenderer(const std::string &renderer, bool delayed = false);

	const std::string &GetLightPackVisualizationType() const { return lightPackVisualizationType; }
	void SetLightPackVisualizationType(const std::string &lightPackVisualizationType, bool delayed = false);

	const std::string &GetLightPackMapping() const { return lightPackMapping; }
	void SetLightPackMapping(const std::string &lightPackMapping, bool delayed = false);

	const std::string &GetLightPackFocusArea() const { return lightPackFocusArea; }
	void SetLightPackFocusArea(const std::string &lightPackFocusArea, bool delayed = false);

	const std::optional<std::size_t> &GetPresetIndex() const { return presetIndex; }
	void SetPresetIndex(std::optional<std::size_t> presetIndex, bool delayed = false);

	const std::optional<std::size_t> &GetMiniPlayerPresetIndex() const { return miniPlayerPresetIndex; }
	void SetMiniPlayerPresetIndex(std::optional<std::size_t> miniPlayerPresetIndex, bool delayed = false);

	const uint8_t &GetSmooth() const { return smooth; }
	void SetSmooth(uint8_t smooth, bool delayed = false);

	const float &GetGamma() const { return gamma; }
	void SetGamma(float gamma, bool delayed = false);

	const bool &GetBlur() const { return blur; }
	void SetBlur(bool blur, bool delayed = false);

	const float &GetBlurIntensity() const { return blurIntensity; }
	void SetBlurIntensity(float blurIntensity, bool delayed = false);

	const float &GetBlurOpacity() const { return blurOpacity; }
	void SetBlurOpacity(float blurOpacity, bool delayed = false);

	const bool &GetCurrentSongVisible() const { return currentSongVisible; }
	void SetCurrentSongVisible(bool currentSongVisible, bool delayed = false);

	const bool &GetPlaylistFade() const { return playlistFade; }
	void SetPlaylistFade(bool playlistFade, bool delayed = false);

	const bool &GetPlaylistOnScreen() const { return playlistOnScreen; }
	void SetPlaylistOnScreen(bool playlistOnScreen, bool delayed = false);

	const int &GetFftSize() const { return fftSize; }
	void SetFftSize(int fftSize, bool delayed = false);

	const bool &GetListening() const { return listening; };
	void SetListening(bool listening, bool delayed = false);

	const bool &GetLoopback() const { return loopback; }
	void SetLoopback(bool loopback, bool delayed = false);

	const std::string &GetOutputDevice() const { return outputDevice; }
	void SetOutputDevice(const std::string &outputDevice, bool delayed = false);

	const std::string &GetInputDevice() const { return inputDevice; }
	void SetInputDevice(const std::string &inputDevice, bool delayed = false);

	const std::string &GetEffect() const { return effect; }
	void SetEffect(const std::string &effect, bool delayed = false);

	const float &GetEffectIntensity() const { return effectIntensity; }
	void SetEffectIntensity(float effectIntensity, bool delayed = false);

	const float &GetEffectXOffset() const { return effectXOffset; }
	void SetEffectXOffset(float effectXOffset, bool delayed = false);

	const float &GetEffectYOffset() const { return effectYOffset; }
	void SetEffectYOffset(float effectYOffset, bool delayed = false);

	const float &GetEffectRadiation() const { return effectRadiation; }
	void SetEffectRadiation(float effectRadiation, bool delayed = false);

	const float &GetEffectHorizontalSpread() const { return effectHorizontalSpread; }
	void SetEffectHorizontalSpread(float effectHorizontalSpread, bool delayed = false);

	const float &GetEffectVerticalSpread() const { return effectVerticalSpread; }
	void SetEffectVerticalSpread(float effectVerticalSpread, bool delayed = false);

	const float &GetEffectRotation() const { return effectRotation; }
	void SetEffectRotation(float effectRotation, bool delayed = false);

	const bool &GetLimitFramerate() const { return limitFramerate; }
	void SetLimitFramerate(bool limitFramerate, bool delayed = false);

	const int &GetFrameLimit() const { return frameLimit; }
	void SetFrameLimit(int frameLimit, bool delayed = false);

	const bool &GetRandomize() const { return randomize; }
	void SetRandomize(bool randomize, bool delayed = false);

	const Duration<Microseconds> &GetRandomizeTime() const { return randomizeTime; }
	void SetRandomizeTime(Duration<Microseconds> randomizeTime, bool delayed = false);

	const float &GetScale() const { return scale; }
	void SetScale(float scale, bool delayed = false);

	const std::set<std::size_t> &GetSelectedPresets() const { return selectedPresets; }
	void SetSelectedPresets(const std::set<std::size_t> &selectedPresets, bool delayed = false);

	const bool &GetRandomizePresets() const { return randomizePresets; }
	void SetRandomizePresets(bool randomizePresets, bool delayed = false);

	const Duration<Microseconds> &GetRandomizePresetsTime() const { return randomizePresetsTime; }
	void SetRandomizePresetsTime(Duration<Microseconds> randomizePresetsTime, bool delayed = false);

	const bool &GetRandomizePresetsByBeats() const { return randomizePresetsByBeats; }
	void SetRandomizePresetsByBeats(bool randomizePresetsByBeats, bool delayed = false);

	const int &GetRandomizePresetsBeats() const { return randomizePresetsBeats; }
	void SetRandomizePresetsBeats(int randomizePresetsBeats, bool delayed = false);

	const bool &GetAutoFade() const { return autoFade; }
	void SetAutoFade(bool autoFade, bool delayed = false);

	const Duration<Microseconds> &GetWaitTime() { return waitTime; }
	void SetWaitTime(Duration<Microseconds> waitTime, bool delayed = false);

	const Duration<Microseconds> &GetMiniPlayerWaitTime() { return miniPlayerWaitTime; }
	void SetMiniPlayerWaitTime(Duration<Microseconds> miniPlayerWaitTime, bool delayed = false);

	const float &GetAutoFadeSpeed() const { return autoFadeSpeed; }
	void SetAutoFadeSpeed(float autoFadeSpeed, bool delayed = false);

	const bool &GetSaveRenderer() const { return saveRenderer; }
	void SetSaveRenderer(bool saveRenderer, bool delayed = false);

	const bool &GetSaveScale() const { return saveScale; }
	void SetSaveScale(bool saveScale, bool delayed = false);

	const int &GetRendererOffset() const { return rendererOffset; }
	void SetRendererOffset(int rendererOffset, bool delayed = false);

	const GLenum &GetSourceFactor() const { return sourceFactor; }
	void SetSourceFactor(GLenum sourceFactor, bool delayed = false);

	const GLenum &GetDestFactor() const { return destFactor; }
	void SetDestFactor(GLenum destFactor, bool delayed = false);

	const GLenum &GetSourceAlphaFactor() const { return sourceFactor; }
	void SetSourceAlphaFactor(GLenum sourceAlphaFactor, bool delayed = false);

	const GLenum &GetDestAlphaFactor() const { return destAlphaFactor; }
	void SetDestAlphaFactor(GLenum destAlphaFactor, bool delayed = false);

	const std::string &GetLut() const { return lut; }
	void SetLut(const std::string &lut, bool delayed = false);

	const float &GetAlbumArtGamma() const { return albumArtGamma; }
	void SetAlbumArtGamma(float albumArtGamma, bool delayed = false);

	const float &GetAlbumArtContrast() const { return albumArtContrast; }
	void SetAlbumArtContrast(float albumArtContrast, bool delayed = false);

	const float &GetAlbumArtBrightness() const { return albumArtBrightness; }
	void SetAlbumArtBrightness(float albumArtBrightness, bool delayed = false);

	const std::optional<float> &GetHdrWhitePoint() const { return hdrWhitePoint; }
	void SetHdrWhitePoint(std::optional<float> hdrWhitePoint, bool delayed = false);

	const bool &GetPulseUi() const { return pulseUi; }
	void SetPulseUi(bool pulseUi, bool delayed = false);

	const float &GetUiGamma() const { return uiGamma; }
	void SetUiGamma(float uiGamma, bool delayed = false);

	const float &GetUiContrast() const { return uiContrast; }
	void SetUiContrast(float uiContrast, bool delayed = false);

	const float &GetUiBrightness() const { return uiBrightness; }
	void SetUiBrightness(float uiBrightness, bool delayed = false);

	const bool &GetHdr() const { return hdr; }
	void SetHdr(bool hdr, bool delayed = false);

	const std::string &GetColorspace() const { return colorspace; }
	void SetColorspace(const std::string &colorspace, bool delayed = false);

	const bool &GetPulseMaxBrightness() const { return pulseMaxBrightness; }
	void SetPulseMaxBrightness(bool pulseMaxBrightness, bool delayed = false);

	const bool &GetVsync() const { return vsync; }
	void SetVsync(bool vsync, bool delayed = false);

	const std::string &GetRngSource() const { return rngSource; }
	void SetRngSource(const std::string &rngSource, bool delayed = false);

	const bool &GetMiniPlayer() const { return miniPlayer; }
	void SetMiniPlayer(bool miniPlayer, bool delayed = false);

	const int &GetMiniPlayerX() const { return miniPlayerX; }
	void SetMiniPlayerX(int miniPlayerX, bool delayed = false);

	const int &GetMiniPlayerY() const { return miniPlayerY; }
	void SetMiniPlayerY(int miniPlayerY, bool delayed = false);

	const int &GetMiniPlayerWidth() const { return miniPlayerWidth; }
	void SetMiniPlayerWidth(int miniPlayerWidth, bool delayed = false);

	const int &GetMiniPlayerHeight() const { return miniPlayerHeight; }
	void SetMiniPlayerHeight(int miniPlayerHeight, bool delayed = false);

	const float &GetMiniPlayerRadius() const { return miniPlayerRadius; }
	void SetMiniPlayerRadius(float miniPlayerRadius, bool delayed = false);

	const int &GetMiniPlayerFontSize() const { return miniPlayerFontSize; }
	void SetMiniPlayerFontSize(int miniPlayerFontSize, bool delayed = false);

	const float &GetMiniPlayerVisualizerRatio() const { return miniPlayerVisualizerRatio; }
	void SetMiniPlayerVisualizerRatio(float miniPlayerVisualizerRatio, bool delayed = false);

	const Duration<Microseconds> &GetHoverTime() const { return hoverTime; }

	const bool &GetCaptureKeyboardMediaKeys() const { return captureKeyboardMediaKeys; }
	void SetCaptureKeyboardMediaKeys(bool captureKeyboardMediaKeys, bool delayed = false);

	const bool &GetVulkan() const { return vulkan; }
	void SetVulkan(bool vulkan, bool delayed = false);

	friend const Node &operator>>(const Node &node, Settings &settings);
	friend Node &operator<<(Node &node, const Settings &settings);

	friend const Node &operator>>(const Node &node, ColorSelection &colorSelection);
	friend Node &operator<<(Node &node, const ColorSelection &colorSelection);
private:
	static Settings Load();

	// Actual settings
	float volume = 1.0f;
	bool exclusive = true;
	ColorSelection colorSelection;

	int windowWidth = 1920;
	int windowHeight = 1080;

	int windowX = SDL_WINDOWPOS_CENTERED;
	int windowY = SDL_WINDOWPOS_CENTERED;

	std::size_t bufferLength = 2048;
	
	Duration<Microseconds> decayTime = 0.5s;
	Duration<Microseconds> fadeTime = 0.5s;

	bool pulse = false;
	bool darkenPulseOnBrightColors = true;
	Duration<Microseconds> pulseTime = 0.1s;

	bool rotating = false;
	float rotationSpeed = 6.0f;

	float radius = 200;

	bool detectBpm = true;
	bool cacheDetectionResults = true;
	bool halveBpm = false;

	float width = 4.0f;

	std::string renderer = "fft";

	std::optional<std::size_t> presetIndex = std::nullopt;

	// FIXME: Find better way to reference 
	//        existing presets than by index
	std::optional<std::size_t> miniPlayerPresetIndex = 7; // Black Hole

	uint8_t smooth = 0;
	float gamma = 1.0f;

	bool blur = false;
	float blurIntensity = 0.5f;
	float blurOpacity = 1.0f;

	std::string lightPackVisualizationType = "intensity";
	std::string lightPackMapping = "default";
	std::string lightPackFocusArea = "bassandmid";

	bool playlistOnScreen = true;
	bool currentSongVisible = false;
	bool playlistFade = true;

	int fftSize = 8192;

	bool listening = false;
	bool loopback = false;

	std::string outputDevice;
	std::string inputDevice;

	std::string effect = "noeffect";

	float effectIntensity = 1.0f;
	float effectXOffset = 0.0f;
	float effectYOffset = 0.0f;
	float effectRadiation = 0.0f;
	float effectHorizontalSpread = 0.0f;
	float effectVerticalSpread = 0.0f;
	float effectRotation = 0.0f;

	bool strobe = false;
	float strobeIntensity = 0.66f;

	bool limitFramerate = false;
	int frameLimit = 60;

	bool randomize = false;
	Duration<Microseconds> randomizeTime = 2.5s;

	float scale = 1.0f;

	// 8: Ripples
	// 9: Conway
	// 11: Eye of the Storm
	// 12: Tunnel
	// 13: Sinkhole
	// 14: Horizon
	// 15: Galaxy
	// 16: Tubular
	std::set<std::size_t> selectedPresets = { 8, 9, 11, 12, 13, 14, 15, 16 };
	bool randomizePresets = false;
	Duration<Microseconds> randomizePresetsTime = 2.5s;
	bool randomizePresetsByBeats = false;
	int randomizePresetsBeats = 16;

	bool autoFade = true;
	Duration<Microseconds> waitTime = 2s;
	Duration<Microseconds> miniPlayerWaitTime = 1s;
	float autoFadeSpeed = 2.0f;

	bool saveRenderer = false;
	bool saveScale = false;

	int rendererOffset = 0.0f;

	bool pulseBackground = false;

	GLenum sourceFactor = GL_ONE;
	GLenum destFactor = GL_ZERO;
	GLenum sourceAlphaFactor = GL_ONE;
	GLenum destAlphaFactor = GL_ZERO;

	std::string lut = "BT709_to_HLG.cube";

#ifdef WIN32
	float albumArtGamma = 0.5f;
	float albumArtContrast = 1.25f;
	float albumArtBrightness = 1.50f;
#elif defined (__ANDROID__)
	float albumArtGamma = 1.0f;
	float albumArtContrast = 1.0f;
	float albumArtBrightness = 0.0f;
#elif defined (__linux__)
	float albumArtGamma = 0.5f;
	float albumArtContrast = 2.0f;
	float albumArtBrightness = 1.45f;
#endif

	std::optional<float> hdrWhitePoint = std::nullopt;

	bool pulseUi = true;

#ifdef WIN32
	float uiGamma = 0.33f;
	float uiContrast = 1.1f;
	float uiBrightness = 1.0f;
#elif defined (__ANDROID__)
	float uiGamma = 1.0f;
	float uiContrast = 1.0f;
	float uiBrightness = 0.0f;
#elif defined(__linux__)
	float uiGamma = 0.33f;
	float uiContrast = 1.62f;
	float uiBrightness = 1.0f;
#endif

	bool miniPlayer = false;

	bool hdr = true;
	std::string colorspace = "EGL_EXT_gl_colorspace_bt2020_hlg";

	bool pulseMaxBrightness = true;

	bool vsync = true;

	std::string rngSource = "MT19937";

	int miniPlayerX = SDL_WINDOWPOS_CENTERED;
	int miniPlayerY = SDL_WINDOWPOS_CENTERED;
	float miniPlayerVisualizerRatio = 5.4f;
	int miniPlayerWidth = 200 * miniPlayerVisualizerRatio;
	int miniPlayerHeight = 200 * miniPlayerVisualizerRatio;
	float miniPlayerRadius = 200;
	int miniPlayerFontSize = 20;

	Duration<Microseconds> hoverTime = 200ms;

	bool captureKeyboardMediaKeys = false;

	bool vulkan = false;
};