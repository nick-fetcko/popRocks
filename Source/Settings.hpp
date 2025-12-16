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

class Settings {
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

		bool operator !=(const ColorSelection &right) {
			return right.minPercentage != minPercentage ||
				right.minSaturation != minSaturation ||
				right.minValue != minValue ||
				right.minHueSeparation != minHueSeparation ||
				right.minValueSeparation != minValueSeparation ||
				right.minRgbSeparation != minRgbSeparation;
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
	static std::filesystem::path GetPath(const std::string &fileName = "Settings.json");

	const float &GetVolume() const { return volume; }
	void SetVolume(float volume);

	const bool &GetExclsuive() const { return exclusive; }
	void SetExclusive(bool exclusive);

	const ColorSelection &GetColorSelection() const { return colorSelection; }
	void SetColorSelection(ColorSelection colorSelection);

	const int &GetWindowWidth() const { return windowWidth; }
	void SetWindowWidth(int windowWidth);

	const int &GetWindowHeight() const { return windowHeight; }
	void SetWindowHeight(int windowHeight);

	const int &GetWindowX() const { return windowX; }
	void SetWindowX(int windowX);

	const int &GetWindowY() const { return windowY; }
	void SetWindowY(int windowY);

	const std::size_t &GetBufferLength() { return bufferLength; }
	void SetBufferLength(std::size_t bufferLength);

	const Duration<Microseconds> &GetDecayTime() { return decayTime; }
	void SetDecayTime(Duration<Microseconds> decayTime);

	const Duration<Microseconds> &GetFadeTime() { return fadeTime; }
	void SetFadeTime(Duration<Microseconds> fadeTime);

	const bool &GetPulse() const { return pulse; }
	void SetPulse(bool pulse);

	const bool &GetPulseBackground() const { return pulseBackground; }
	void SetPulseBackground(bool pulseBackground);

	const bool &GetDarkenPulseOnBrightColors() const { return darkenPulseOnBrightColors; }
	void SetDarkenPulseOnBrightColors(bool darkenPulseOnBrightColors);

	const Duration<Microseconds> &GetPulseTime() { return pulseTime; }
	void SetPulseTime(Duration<Microseconds> pulseTime);

	const bool &GetStrobe() const { return strobe; }
	void SetStrobe(bool strobe);

	const float &GetStrobeIntensity() const { return strobeIntensity; }
	void SetStrobeIntensity(float strobeIntensity);

	const bool &GetRotating() const { return rotating; }
	void SetRotating(bool rotating);

	const float &GetRotationSpeed() const { return rotationSpeed; }
	void SetRotationSpeed(float rotationSpeed);

	const float &GetRadius() const { return radius; }
	void SetRadius(float radius);

	const bool &GetDetectBpm() const { return detectBpm; }
	void SetDetectBpm(bool detectBpm);

	const bool &GetCacheDetectionResults() const { return cacheDetectionResults; }
	void SetCacheDetectionResults(bool cacheDetectionResults);

	const bool &GetHalveBpm() const { return halveBpm; }
	void SetHalveBpm(bool halveBpm);

	const float &GetWidth() const { return width; }
	void SetWidth(float width);

	const std::string &GetRenderer() const { return renderer; }
	void SetRenderer(const std::string &renderer);

	const std::string &GetLightPackVisualizationType() const { return lightPackVisualizationType; }
	void SetLightPackVisualizationType(const std::string &lightPackVisualizationType);

	const std::string &GetLightPackMapping() const { return lightPackMapping; }
	void SetLightPackMapping(const std::string &lightPackMapping);

	const std::string &GetLightPackFocusArea() const { return lightPackFocusArea; }
	void SetLightPackFocusArea(const std::string &lightPackFocusArea);

	const std::optional<std::size_t> &GetPresetIndex() const { return presetIndex; }
	void SetPresetIndex(std::optional<std::size_t> presetIndex);

	const uint8_t &GetSmooth() const { return smooth; }
	void SetSmooth(uint8_t smooth);

	const float &GetGamma() const { return gamma; }
	void SetGamma(float gamma);

	const bool &GetBlur() const { return blur; }
	void SetBlur(bool blur);

	const float &GetBlurIntensity() const { return blurIntensity; }
	void SetBlurIntensity(float blurIntensity);

	const float &GetBlurOpacity() const { return blurOpacity; }
	void SetBlurOpacity(float blurOpacity);

	const bool &GetCurrentSongVisible() const { return currentSongVisible; }
	void SetCurrentSongVisible(bool currentSongVisible);

	const int &GetFftSize() const { return fftSize; }
	void SetFftSize(int fftSize);

	const bool &GetListening() const { return listening; };
	void SetListening(bool listening);

	const bool &GetLoopback() const { return loopback; }
	void SetLoopback(bool loopback);

	const std::string &GetOutputDevice() const { return outputDevice; }
	void SetOutputDevice(const std::string &outputDevice);

	const std::string &GetInputDevice() const { return inputDevice; }
	void SetInputDevice(const std::string &inputDevice);

	const std::string &GetEffect() const { return effect; }
	void SetEffect(const std::string &effect);

	const float &GetEffectIntensity() const { return effectIntensity; }
	void SetEffectIntensity(float effectIntensity);

	const float &GetEffectXOffset() const { return effectXOffset; }
	void SetEffectXOffset(float effectXOffset);

	const float &GetEffectYOffset() const { return effectYOffset; }
	void SetEffectYOffset(float effectYOffset);

	const float &GetEffectRadiation() const { return effectRadiation; }
	void SetEffectRadiation(float effectRadiation);

	const float &GetEffectHorizontalSpread() const { return effectHorizontalSpread; }
	void SetEffectHorizontalSpread(float effectHorizontalSpread);

	const float &GetEffectVerticalSpread() const { return effectVerticalSpread; }
	void SetEffectVerticalSpread(float effectVerticalSpread);

	const float &GetEffectRotation() const { return effectRotation; }
	void SetEffectRotation(float effectRotation);

	const bool &GetLimitFramerate() const { return limitFramerate; }
	void SetLimitFramerate(bool limitFramerate);

	const int &GetFrameLimit() const { return frameLimit; }
	void SetFrameLimit(int frameLimit);

	const bool &GetRandomize() const { return randomize; }
	void SetRandomize(bool randomize);

	const Duration<Microseconds> &GetRandomizeTime() const { return randomizeTime; }
	void SetRandomizeTime(Duration<Microseconds> randomizeTime);

	const float &GetScale() const { return scale; }
	void SetScale(float scale);

	const std::set<std::size_t> &GetSelectedPresets() const { return selectedPresets; }
	void SetSelectedPresets(const std::set<std::size_t> &selectedPresets);

	const bool &GetRandomizePresets() const { return randomizePresets; }
	void SetRandomizePresets(bool randomizePresets);

	const Duration<Microseconds> &GetRandomizePresetsTime() const { return randomizePresetsTime; }
	void SetRandomizePresetsTime(Duration<Microseconds> randomizePresetsTime);

	const bool &GetRandomizePresetsByBeats() const { return randomizePresetsByBeats; }
	void SetRandomizePresetsByBeats(bool randomizePresetsByBeats);

	const int &GetRandomizePresetsBeats() const { return randomizePresetsBeats; }
	void SetRandomizePresetsBeats(int randomizePresetsBeats);

	const bool &GetAutoFade() const { return autoFade; }
	void SetAutoFade(bool autoFade);

	const Duration<Microseconds> &GetWaitTime() { return waitTime; }
	void SetWaitTime(Duration<Microseconds> waitTime);

	const float &GetAutoFadeSpeed() const { return autoFadeSpeed; }
	void SetAutoFadeSpeed(float autoFadeSpeed);

	const bool &GetSaveRenderer() const { return saveRenderer; }
	void SetSaveRenderer(bool saveRenderer);

	const bool &GetSaveScale() const { return saveScale; }
	void SetSaveScale(bool saveScale);

	const int &GetRendererOffset() const { return rendererOffset; }
	void SetRendererOffset(int rendererOffset);

	const GLenum &GetSourceFactor() const { return sourceFactor; }
	void SetSourceFactor(GLenum sourceFactor);

	const GLenum &GetDestFactor() const { return destFactor; }
	void SetDestFactor(GLenum destFactor);

	const std::string &GetLut() const { return lut; }
	void SetLut(const std::string &lut);

	const float &GetAlbumArtGamma() const { return albumArtGamma; }
	void SetAlbumArtGamma(float albumArtGamma);

	const float &GetAlbumArtContrast() const { return albumArtContrast; }
	void SetAlbumArtContrast(float albumArtContrast);

	const float &GetAlbumArtBrightness() const { return albumArtBrightness; }
	void SetAlbumArtBrightness(float albumArtBrightness);

	const std::optional<float> &GetHdrWhitePoint() const { return hdrWhitePoint; }
	void SetHdrWhitePoint(std::optional<float> hdrWhitePoint);

	const bool &GetPulseUi() const { return pulseUi; }
	void SetPulseUi(bool pulseUi);

	const float &GetUiGamma() const { return uiGamma; }
	void SetUiGamma(float uiGamma);

	const float &GetUiContrast() const { return uiContrast; }
	void SetUiContrast(float uiContrast);

	const float &GetUiBrightness() const { return uiBrightness; }
	void SetUiBrightness(float uiBrightness);

	const bool &GetHdr() const { return hdr; }
	void SetHdr(bool hdr);

	const std::string &GetColorspace() const { return colorspace; }
	void SetColorspace(const std::string &colorspace);

	friend const Node &operator>>(const Node &node, Settings &settings);
	friend Node &operator<<(Node &node, const Settings &settings);

	friend const Node &operator>>(const Node &node, ColorSelection &colorSelection);
	friend Node &operator<<(Node &node, const ColorSelection &colorSelection);
private:
	static std::string path;
	static Settings Load();

	void Save();

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

	uint8_t smooth = 0;
	float gamma = 1.0f;

	bool blur = false;
	float blurIntensity = 0.5f;
	float blurOpacity = 1.0f;

	std::string lightPackVisualizationType = "intensity";
	std::string lightPackMapping = "default";
	std::string lightPackFocusArea = "bassandmid";

	bool currentSongVisible = false;

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
	float autoFadeSpeed = 2.0f;

	bool saveRenderer = false;
	bool saveScale = false;

	int rendererOffset = 0.0f;

	bool pulseBackground = false;

	GLenum sourceFactor = GL_ONE;
	GLenum destFactor = GL_ZERO;

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

	bool hdr = true;
	std::string colorspace = "EGL_EXT_gl_colorspace_bt2020_pq";
};