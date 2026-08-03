#include "Settings.hpp"

#include <fstream>

#ifdef _WIN32
#include <Shlobj.h>
#endif

#include "Utils/Filesystem.hpp"

#include "AlbumArt.hpp"
#include "CApp.h"
#include "Preset.hpp"

#ifndef __ANDROID__
Settings Settings::settings = Settings::Load();
#else
#include <EGL/egl.h>
#include <EGL/eglext.h>
Settings Settings::settings;
#endif

std::map<GLenum, std::string> Settings::BlendModes = {
	{ GL_ZERO, "GL_ZERO" },
	{ GL_ONE, "GL_ONE" },
	{ GL_SRC_COLOR, "GL_SRC_COLOR" },
	{ GL_ONE_MINUS_SRC_COLOR, "GL_ONE_MINUS_SRC_COLOR"},
	{ GL_DST_COLOR, "GL_DST_COLOR"},
	{ GL_ONE_MINUS_DST_COLOR, "GL_ONE_MINUS_DST_COLOR"},
	{ GL_SRC_ALPHA, "GL_SRC_ALPHA"},
	{ GL_ONE_MINUS_SRC_ALPHA, "GL_ONE_MINUS_SRC_ALPHA"},
	{ GL_DST_ALPHA, "GL_DST_ALPHA"},
	{ GL_ONE_MINUS_DST_ALPHA, "GL_ONE_MINUS_DST_ALPHA"},
	{ GL_CONSTANT_COLOR, "GL_CONSTANT_COLOR"},
	{ GL_ONE_MINUS_CONSTANT_COLOR, "GL_ONE_MINUS_CONSTANT_COLOR"},
	{ GL_CONSTANT_ALPHA, "GL_CONSTANT_ALPHA"},
	{ GL_ONE_MINUS_CONSTANT_ALPHA, "GL_ONE_MINUS_CONSTANT_ALPHA"},
	{ GL_SRC_ALPHA_SATURATE, "GL_SRC_ALPHA_SATURATE"},
#ifndef __ANDROID__
	{ GL_SRC1_COLOR, "GL_SRC1_COLOR"},
	{ GL_ONE_MINUS_SRC1_COLOR, "GL_ONE_MINUS_SRC1_COLOR"},
	{ GL_SRC1_ALPHA, "GL_SRC1_ALPHA"},
	{ GL_ONE_MINUS_SRC1_ALPHA, "GL_ONE_MINUS_SRC1_ALPHA"}
#endif
};

std::map<std::string, GLenum> Settings::Colorspaces = {
#ifdef __ANDROID__
	{ "EGL_EXT_gl_colorspace_scrgb", EGL_GL_COLORSPACE_SCRGB_EXT},
	{ "EGL_EXT_gl_colorspace_scrgb_linear", EGL_GL_COLORSPACE_SCRGB_LINEAR_EXT },
	{ "EGL_EXT_gl_colorspace_display_p3_linear", EGL_GL_COLORSPACE_DISPLAY_P3_LINEAR_EXT },
	{ "EGL_EXT_gl_colorspace_display_p3", EGL_GL_COLORSPACE_DISPLAY_P3_EXT },
	{ "EGL_EXT_gl_colorspace_display_p3_passthrough", EGL_GL_COLORSPACE_DISPLAY_P3_PASSTHROUGH_EXT },
	{ "EGL_EXT_gl_colorspace_bt2020_hlg", EGL_GL_COLORSPACE_BT2020_HLG_EXT },
	{ "EGL_EXT_gl_colorspace_bt2020_linear", EGL_GL_COLORSPACE_BT2020_LINEAR_EXT },
	{ "EGL_EXT_gl_colorspace_bt2020_pq", EGL_GL_COLORSPACE_BT2020_PQ_EXT }
#endif
};

// This config has much more aggressive normalization
//	const DynamicGain<float> Settings::BaseDynamicGain = { 
//		0.001f, 0.000001f, std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest(), true, true, true
//	};

// This config gives the "original", less normalized look
const DynamicGain<float> Settings::BaseDynamicGain = {
	0.001f, 0.0000001f, 0.0f, 0.00f, false, true, true
};

Settings::Settings() {
	radius = AlbumArt::BaseRadius;
	miniPlayerRadius = AlbumArt::BaseRadius;

	miniPlayerWidth = radius * miniPlayerVisualizerRatio;
	miniPlayerHeight = radius * miniPlayerVisualizerRatio;
}

void Settings::SetPath(const std::string &path) {
	Filesystem::SetPath(path);

	Settings::settings = Settings::Load();
}

Settings Settings::Load() {
	// Make sure we actually have a folder
	// to load settings from.
#ifndef __ANDROID__
	Filesystem::SetAppName("popRocks");
#endif

	Settings ret;

	if (auto path = Filesystem::GetPath(); !path.empty()) {
		std::ifstream inFile(path, std::ios::in);

		if (inFile) {
			try {
				Node json;
				json.parseStream<Json>(inFile);

				json >> ret;

				// Any setting not loaded will be set
				// to its default value. Drop those
				// default values down right away
				// so the user can edit them if
				// they'd like to.
				ret.Save();
			} catch (std::exception &e) {
				std::cerr << "Could not load settings due to " << e.what() << std::endl;
			}
		} else std::cerr << "Settings file does not yet exist!" << std::endl;
	}

#ifdef __ANDROID__
	LoadPresets();
#endif

	return ret;
}

float Settings::LoadDefaults(CApp *app) {
	if (miniPlayerWidth == -1 ||
		miniPlayerHeight == -1 ||
		windowWidth == -1 ||
		windowHeight == -1) {
		int screenWidth = 0, screenHeight = 0;
		const auto *displayMode = SDL_GetDesktopDisplayMode(
#ifdef __linux__
			app ? 
				SDL_GetDisplayForWindow(app->GetSdlWindow())
				:
#endif
			SDL_GetPrimaryDisplay()
		);
		
		if (displayMode) {
			
			const auto dpi = 
#ifdef __linux__
				// Round to 2 decimal places
				std::round(displayMode->pixel_density * 100.0f) / 100.0f;
#else

				// On Windows, displayMode->pixel_density is always 1.0,
				// so use SDL_GetDisplayContentScale() here, instead
				SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
#endif

			const auto min = std::min(displayMode->w, displayMode->h);
			
			constexpr auto DefaultPercentage = 0.75f;

			if (windowWidth == -1 || windowHeight == -1) {
				SetWindowWidth(1920 / dpi, true);
				SetWindowHeight(1080 / dpi, true);

				LogDebug(
					"Setting (full view) defaults to:\n\twindowWidth = ", windowWidth,
					"\n\tminiPlayerHeight = ", windowHeight
				);
			}

			if (miniPlayerWidth == -1 || miniPlayerHeight == -1) {
				SetMiniPlayerWidth(
					min * DefaultPercentage
					// Save as DPI agnostic pixels on Windows
#ifdef WIN32
					/ dpi
#endif
					,
					true
				);
				SetMiniPlayerHeight(
					min * DefaultPercentage
					// Save as DPI agnostic pixels on Windows
#ifdef WIN32
					/ dpi
#endif
					,
					true
				);
				SetMiniPlayerRadius(min * DefaultPercentage / 5.4f, true);
				SetMiniPlayerVisualizerRatio(min * DefaultPercentage / miniPlayerRadius, true);
				SetMiniPlayerFontSize(std::lround(20 / (AlbumArt::BaseRadius / miniPlayerRadius)), true);

				LogDebug(
					"Setting (mini-player) defaults to:\n\tminiPlayerWidth = ", miniPlayerWidth,
					"\n\tminiPlayerHeight = ", miniPlayerHeight,
					"\n\tminiPlayerRadius = ", miniPlayerRadius,
					"\n\tminiPlayerVisualizerRatio = ", miniPlayerVisualizerRatio,
					"\n\tminiPlayerFontSize = ", miniPlayerFontSize
				);
			}

			Save();

			return dpi;
		} else LogError("Display mode was null! ", SDL_GetError());
	}

	return 1.0f;
}

void Settings::LoadPresets() {
	Preset::Presets = Preset::Load();
}

void Settings::SetVolume(float volume, bool delayed) {
	if (volume != this->volume) {
		this->volume = volume;
		if (!delayed) Save();
	}
}

void Settings::SetExclusive(bool exclusive, bool delayed) {
	if (exclusive != this->exclusive) {
		this->exclusive = exclusive;
		if (!delayed) Save();
	}
}

void Settings::SetColorSelection(ColorSelection colorSelection, bool delayed) {
	if (colorSelection != this->colorSelection) {
		this->colorSelection = colorSelection;
		if (!delayed) Save();
	}
}

void Settings::SetWindowWidth(int windowWidth, bool delayed) {
	if (windowWidth != this->windowWidth) {
		this->windowWidth = windowWidth;
		if (!delayed) Save();
	}
}

void Settings::SetWindowHeight(int windowHeight, bool delayed) {
	if (windowHeight != this->windowHeight) {
		this->windowHeight = windowHeight;
		if (!delayed) Save();
	}
}

void Settings::SetWindowX(int windowX, bool delayed) {
	if (windowX != this->windowX) {
		this->windowX = windowX;
		if (!delayed) Save();
	}
}

void Settings::SetWindowY(int windowY, bool delayed) {
	if (windowY != this->windowY) {
		this->windowY = windowY;
		if (!delayed) Save();
	}
}

void Settings::SetBufferLength(std::size_t bufferLength, bool delayed) {
	if (bufferLength != this->bufferLength) {
		this->bufferLength = bufferLength;
		if (!delayed) Save();
	}
}

void Settings::SetDecayTime(Duration<Microseconds> decayTime, bool delayed) {
	if (decayTime != this->decayTime) {
		this->decayTime = decayTime;
		if (!delayed) Save();
	}
}

void Settings::SetFadeTime(Duration<Microseconds> fadeTime, bool delayed) {
	if (fadeTime != this->fadeTime) {
		this->fadeTime = fadeTime;
		if (!delayed) Save();
	}
}

void Settings::SetPulse(bool pulse, bool delayed) {
	if (pulse != this->pulse) {
		this->pulse = pulse;
		if (!delayed) Save();
	}
}

void Settings::SetDarkenPulseOnBrightColors(bool darkenPulseOnBrightColors, bool delayed) {
	if (darkenPulseOnBrightColors != this->darkenPulseOnBrightColors) {
		this->darkenPulseOnBrightColors = darkenPulseOnBrightColors;
		if (!delayed) Save();
	}
}

void Settings::SetPulseTime(Duration<Microseconds> pulseTime, bool delayed) {
	if (pulseTime != this->pulseTime) {
		this->pulseTime = pulseTime;
		if (!delayed) Save();
	}
}

void Settings::SetStrobe(bool strobe, bool delayed) {
	if (strobe != this->strobe) {
		this->strobe = strobe;
		if (!delayed) Save();
	}
}

void Settings::SetStrobeIntensity(float strobeIntensity, bool delayed) {
	if (strobeIntensity != this->strobeIntensity) {
		this->strobeIntensity = strobeIntensity;
		if (!delayed) Save();
	}
}

void Settings::SetRotating(bool rotating, bool delayed) {
	if (rotating != this->rotating) {
		this->rotating = rotating;
		if (!delayed) Save();
	}
}

void Settings::SetMiniPlayerRotating(bool miniPlayerRotating, bool delayed) {
	if (miniPlayerRotating != this->miniPlayerRotating) {
		this->miniPlayerRotating = miniPlayerRotating;
		if (!delayed) Save();
	}
}

void Settings::SetRotationSpeed(float rotationSpeed, bool delayed) {
	if (rotationSpeed != this->rotationSpeed) {
		this->rotationSpeed = rotationSpeed;
		if (!delayed) Save();
	}
}

void Settings::SetRadius(float radius, bool delayed) {
	if (radius != this->radius) {
		this->radius = radius;
		if (!delayed) Save();
	}
}

void Settings::SetDetectBpm(bool detectBpm, bool delayed) {
	if (detectBpm != this->detectBpm) {
		this->detectBpm = detectBpm;
		if (!delayed) Save();
	}
}

void Settings::SetCacheDetectionResults(bool cacheDetectionResults, bool delayed) {
	if (cacheDetectionResults != this->cacheDetectionResults) {
		this->cacheDetectionResults = cacheDetectionResults;
		if (!delayed) Save();
	}
}

void Settings::SetHalveBpm(bool halveBpm, bool delayed) {
	if (halveBpm != this->halveBpm) {
		this->halveBpm = halveBpm;
		if (!delayed) Save();
	}
}

void Settings::SetWidth(float width, bool delayed) {
	if (width != this->width) {
		this->width = width;
		if (!delayed) Save();
	}
}

void Settings::SetRenderer(const std::string &renderer, bool delayed) {
	if (renderer != this->renderer) {
		this->renderer = renderer;
		if (!delayed) Save();
	}
}

void Settings::SetLineRendererStyle(LineRenderer::Style lineRendererStyle, bool delayed) {
	if (lineRendererStyle != this->lineRendererStyle) {
		this->lineRendererStyle = lineRendererStyle;
		if (!delayed) Save();
	}
}

void Settings::SetLightPackVisualizationType(const std::string &lightPackVisualizationType, bool delayed) {
	if (lightPackVisualizationType != this->lightPackVisualizationType) {
		this->lightPackVisualizationType = lightPackVisualizationType;
		if (!delayed) Save();
	}
}

void Settings::SetLightPackMapping(const std::string &lightPackMapping, bool delayed) {
	if (lightPackMapping != this->lightPackMapping) {
		this->lightPackMapping = lightPackMapping;
		if (!delayed) Save();
	}
}

void Settings::SetLightPackFocusArea(const std::string &lightPackFocusArea, bool delayed) {
	if (lightPackFocusArea != this->lightPackFocusArea) {
		this->lightPackFocusArea = lightPackFocusArea;
		if (!delayed) Save();
	}
}

void Settings::SetPresetIndex(std::optional<std::size_t> presetIndex, bool delayed) {
	if (presetIndex != this->presetIndex) {
		this->presetIndex = presetIndex;
		if (!delayed) Save();
	}
}

void Settings::SetMiniPlayerPresetIndex(std::optional<std::size_t> miniPlayerPresetIndex, bool delayed) {
	if (miniPlayerPresetIndex != this->miniPlayerPresetIndex) {
		this->miniPlayerPresetIndex = miniPlayerPresetIndex;
		if (!delayed) Save();
	}
}

void Settings::SetSmooth(uint8_t smooth, bool delayed) {
	if (smooth != this->smooth) {
		this->smooth = smooth;
		if (!delayed) Save();
	}
}

void Settings::SetGamma(float gamma, bool delayed) {
	if (gamma != this->gamma) {
		this->gamma = gamma;
		if (!delayed) Save();
	}
}

void Settings::SetBlur(bool blur, bool delayed) {
	if (blur != this->blur) {
		this->blur = blur;
		if (!delayed) Save();
	}
}

void Settings::SetBlurIntensity(float blurIntensity, bool delayed) {
	if (blurIntensity != this->blurIntensity) {
		this->blurIntensity = blurIntensity;
		if (!delayed) Save();
	}
}

void Settings::SetBlurOpacity(float blurOpacity, bool delayed) {
	if (blurOpacity != this->blurOpacity) {
		this->blurOpacity = blurOpacity;
		if (!delayed) Save();
	}
}

void Settings::SetCurrentSongVisible(bool currentSongVisible, bool delayed) {
	if (currentSongVisible != this->currentSongVisible) {
		this->currentSongVisible = currentSongVisible;
		if (!delayed) Save();
	}
}

void Settings::SetPlaylistFade(bool playlistFade, bool delayed) {
	if (playlistFade != this->playlistFade) {
		this->playlistFade = playlistFade;
		if (!delayed) Save();
	}
}

void Settings::SetPlaylistOnScreen(bool playlistOnScreen, bool delayed) {
	if (playlistOnScreen != this->playlistOnScreen) {
		this->playlistOnScreen = playlistOnScreen;
		if (!delayed) Save();
	}
}

void Settings::SetFftSize(int fftSize, bool delayed) {
	if (fftSize != this->fftSize) {
		this->fftSize = fftSize;
		if (!delayed) Save();
	}
}

void Settings::SetListening(bool listening, bool delayed) {
	if (listening != this->listening) {
		this->listening = listening;
		if (listening) loopback = false;
		if (!delayed) Save();
	}
}

void Settings::SetLoopback(bool loopback, bool delayed) {
	if (loopback != this->loopback) {
		this->loopback = loopback;
		if (loopback) listening = false;
		if (!delayed) Save();
	}
}

void Settings::SetOutputDevice(const std::string &outputDevice, bool delayed) {
	if (outputDevice != this->outputDevice) {
		this->outputDevice = outputDevice;
		if (!delayed) Save();
	}
}

void Settings::SetInputDevice(const std::string &inputDevice, bool delayed) {
	if (inputDevice != this->inputDevice) {
		this->inputDevice = inputDevice;
		if (!delayed) Save();
	}
}

void Settings::SetEffect(const std::string &effect, bool delayed) {
	if (effect != this->effect) {
		this->effect = effect;
		if (!delayed) Save();
	}
}

void Settings::SetEffectIntensity(float effectIntensity, bool delayed) {
	if (effectIntensity != this->effectIntensity) {
		this->effectIntensity = effectIntensity;
		if (!delayed) Save();
	}
}

void Settings::SetEffectXOffset(float effectXOffset, bool delayed) {
	if (effectXOffset != this->effectXOffset) {
		this->effectXOffset = effectXOffset;
		if (!delayed) Save();
	}
}

void Settings::SetEffectYOffset(float effectYOffset, bool delayed) {
	if (effectYOffset != this->effectYOffset) {
		this->effectYOffset = effectYOffset;
		if (!delayed) Save();
	}
}

void Settings::SetEffectRadiation(float effectRadiation, bool delayed) {
	if (effectRadiation != this->effectRadiation) {
		this->effectRadiation = effectRadiation;
		if (!delayed) Save();
	}
}

void Settings::SetEffectHorizontalSpread(float effectHorizontalSpread, bool delayed) {
	if (effectHorizontalSpread != this->effectHorizontalSpread) {
		this->effectHorizontalSpread = effectHorizontalSpread;
		if (!delayed) Save();
	}
}

void Settings::SetEffectVerticalSpread(float effectVerticalSpread, bool delayed) {
	if (effectVerticalSpread != this->effectVerticalSpread) {
		this->effectVerticalSpread = effectVerticalSpread;
		if (!delayed) Save();
	}
}

void Settings::SetEffectRotation(float effectRotation, bool delayed) {
	if (effectRotation != this->effectRotation) {
		this->effectRotation = effectRotation;
		if (!delayed) Save();
	}
}

void Settings::SetLimitFramerate(bool limitFramerate, bool delayed) {
	if (limitFramerate != this->limitFramerate) {
		this->limitFramerate = limitFramerate;
		if (!delayed) Save();
	}
}

void Settings::SetFrameLimit(int frameLimit, bool delayed) {
	if (frameLimit != this->frameLimit) {
		this->frameLimit = frameLimit;
		if (!delayed) Save();
	}
}

void Settings::SetRandomize(bool randomize, bool delayed) {
	if (randomize != this->randomize) {
		this->randomize = randomize;
		if (!delayed) Save();
	}
}

void Settings::SetRandomizeTime(Duration<Microseconds> randomizeTime, bool delayed) {
	if (randomizeTime != this->randomizeTime) {
		this->randomizeTime = randomizeTime;
		if (!delayed) Save();
	}
}

void Settings::SetScale(float scale, bool delayed) {
	if (scale != this->scale) {
		this->scale = scale;
		if (!delayed) Save();
	}
}

void Settings::SetSelectedPresets(const std::set<std::size_t> &selectedPresets, bool delayed) {
	if (selectedPresets != this->selectedPresets) {
		this->selectedPresets = selectedPresets;
		if (!delayed) Save();
	}
}

void Settings::SetRandomizePresets(bool randomizePresets, bool delayed) {
	if (randomizePresets != this->randomizePresets) {
		this->randomizePresets = randomizePresets;
		if (!delayed) Save();
	}
}

void Settings::SetRandomizePresetsTime(Duration<Microseconds> randomizePresetsTime, bool delayed) {
	if (randomizePresetsTime != this->randomizePresetsTime) {
		this->randomizePresetsTime = randomizePresetsTime;
		if (!delayed) Save();
	}
}

void Settings::SetRandomizePresetsByBeats(bool randomizePresetsByBeats, bool delayed) {
	if (randomizePresetsByBeats != this->randomizePresetsByBeats) {
		this->randomizePresetsByBeats = randomizePresetsByBeats;
		if (!delayed) Save();
	}
}

void Settings::SetRandomizePresetsBeats(int randomizePresetsBeats, bool delayed) {
	if (randomizePresetsBeats != this->randomizePresetsBeats) {
		this->randomizePresetsBeats = randomizePresetsBeats;
		if (!delayed) Save();
	}
}

void Settings::SetAutoFade(bool autoFade, bool delayed) {
	if (autoFade != this->autoFade) {
		this->autoFade = autoFade;
		if (!delayed) Save();
	}
}

void Settings::SetWaitTime(Duration<Microseconds> waitTime, bool delayed) {
	if (waitTime != this->waitTime) {
		this->waitTime = waitTime;
		if (!delayed) Save();
	}
}

void Settings::SetMiniPlayerWaitTime(Duration<Microseconds> miniPlayerWaitTime, bool delayed) {
	if (miniPlayerWaitTime != this->miniPlayerWaitTime) {
		this->waitTime = miniPlayerWaitTime;
		if (!delayed) Save();
	}
}

void Settings::SetAutoFadeSpeed(float autoFadeSpeed, bool delayed) {
	if (autoFadeSpeed != this->autoFadeSpeed) {
		this->autoFadeSpeed = autoFadeSpeed;
		if (!delayed) Save();
	}
}

void Settings::SetSaveRenderer(bool saveRenderer, bool delayed) {
	if (saveRenderer != this->saveRenderer) {
		this->saveRenderer = saveRenderer;
		if (!delayed) Save();
	}
}

void Settings::SetSaveScale(bool saveScale, bool delayed) {
	if (saveScale != this->saveScale) {
		this->saveScale = saveScale;
		if (!delayed) Save();
	}
}

void Settings::SetRendererOffset(int rendererOffset, bool delayed) {
	if (rendererOffset != this->rendererOffset) {
		this->rendererOffset = rendererOffset;
		if (!delayed) Save();
	}
}

void Settings::SetPulseBackground(bool pulseBackground, bool delayed) {
	if (pulseBackground != this->pulseBackground) {
		this->pulseBackground = pulseBackground;
		if (!delayed) Save();
	}
}

void Settings::SetSourceFactor(GLenum sourceFactor, bool delayed) {
	if (this->sourceFactor != sourceFactor) {
		this->sourceFactor = sourceFactor;
		if (!delayed) Save();
	}
}

void Settings::SetDestFactor(GLenum destFactor, bool delayed) {
	if (this->destFactor != destFactor) {
		this->destFactor = destFactor;
		if (!delayed) Save();
	}
}

void Settings::SetSourceAlphaFactor(GLenum sourceAlphaFactor, bool delayed) {
	if (this->sourceAlphaFactor != sourceAlphaFactor) {
		this->sourceAlphaFactor = sourceAlphaFactor;
		if (!delayed) Save();
	}
}

void Settings::SetDestAlphaFactor(GLenum destAlphaFactor, bool delayed) {
	if (this->destAlphaFactor != destAlphaFactor) {
		this->destAlphaFactor = destAlphaFactor;
		if (!delayed) Save();
	}
}

void Settings::SetLut(const std::string &lut, bool delayed) {
	if (this->lut != lut) {
		this->lut = lut;
		if (!delayed) Save();
	}
}

void Settings::SetAlbumArtGamma(float albumArtGamma, bool delayed) {
	if (this->albumArtGamma != albumArtGamma) {
		this->albumArtGamma = albumArtGamma;
		if (!delayed) Save();
	}
}

void Settings::SetAlbumArtContrast(float albumArtContrast, bool delayed) {
	if (this->albumArtContrast != albumArtContrast) {
		this->albumArtContrast = albumArtContrast;
		if (!delayed) Save();
	}
}

void Settings::SetAlbumArtBrightness(float albumArtBrightness, bool delayed) {
	if (this->albumArtBrightness != albumArtBrightness) {
		this->albumArtBrightness = albumArtBrightness;
		if (!delayed) Save();
	}
}

void Settings::SetHdrWhitePoint(std::optional<float> hdrWhitePoint, bool delayed) {
	if (this->hdrWhitePoint != hdrWhitePoint) {
		this->hdrWhitePoint = hdrWhitePoint;
		if (!delayed) Save();
	}
}

void Settings::SetPulseUi(bool pulseUi, bool delayed) {
	if (this->pulseUi != pulseUi) {
		this->pulseUi = pulseUi;
		if (!delayed) Save();
	}
}

void Settings::SetUiGamma(float uiGamma, bool delayed) {
	if (this->uiGamma != uiGamma) {
		this->uiGamma = uiGamma;
		if (!delayed) Save();
	}
}

void Settings::SetUiContrast(float uiContrast, bool delayed) {
	if (this->uiContrast != uiContrast) {
		this->uiContrast = uiContrast;
		if (!delayed) Save();
	}
}

void Settings::SetUiBrightness(float uiBrightness, bool delayed) {
	if (this->uiBrightness != uiBrightness) {
		this->uiBrightness = uiBrightness;
		if (!delayed) Save();
	}
}

void Settings::SetHdr(bool hdr, bool delayed) {
	if (this->hdr != hdr) {
		this->hdr = hdr;
		if (!delayed) Save();
	}
}

void Settings::SetColorspace(const std::string &colorspace, bool delayed) {
	if (this->colorspace != colorspace) {
		this->colorspace = colorspace;
		if (!delayed) Save();
	}
}

void Settings::SetPulseMaxBrightness(bool pulseMaxBrightness, bool delayed) {
	if (this->pulseMaxBrightness != pulseMaxBrightness) {
		this->pulseMaxBrightness = pulseMaxBrightness;
		if (!delayed) Save();
	}
}

void Settings::SetVsync(bool vsync, bool delayed) {
	if (this->vsync != vsync) {
		this->vsync = vsync;
		if (!delayed) Save();
	}
}

void Settings::SetRngSource(const std::string &rngSource, bool delayed) {
	if (this->rngSource != rngSource) {
		this->rngSource = rngSource;
		if (!delayed) Save();
	}
}

void Settings::SetMiniPlayer(bool miniPlayer, bool delayed) {
	if (this->miniPlayer != miniPlayer) {
		this->miniPlayer = miniPlayer;
		if (!delayed) Save();
	}
}

void Settings::SetMiniPlayerX(int miniPlayerX, bool delayed) {
	if (this->miniPlayerX != miniPlayerX) {
		this->miniPlayerX = miniPlayerX;
		if (!delayed) Save();
	}
}

void Settings::SetMiniPlayerY(int miniPlayerY, bool delayed) {
	if (this->miniPlayerY != miniPlayerY) {
		this->miniPlayerY = miniPlayerY;
		if (!delayed) Save();
	}
}

void Settings::SetMiniPlayerWidth(int miniPlayerWidth, bool delayed) {
	if (this->miniPlayerWidth != miniPlayerWidth) {
		this->miniPlayerWidth = miniPlayerWidth;
		if (!delayed) Save();
	}
}

void Settings::SetMiniPlayerHeight(int miniPlayerHeight, bool delayed) {
	if (this->miniPlayerHeight != miniPlayerHeight) {
		this->miniPlayerHeight = miniPlayerHeight;
		if (!delayed) Save();
	}
}

void Settings::SetMiniPlayerRadius(float miniPlayerRadius, bool delayed) {
	if (this->miniPlayerRadius != miniPlayerRadius) {
		this->miniPlayerRadius = miniPlayerRadius;
		if (!delayed) Save();
	}
}

void Settings::SetMiniPlayerFontSize(int miniPlayerFontSize, bool delayed) {
	if (this->miniPlayerFontSize != miniPlayerFontSize) {
		this->miniPlayerFontSize = miniPlayerFontSize;
		if (!delayed) Save();
	}
}

void Settings::SetMiniPlayerVisualizerRatio(float miniPlayerVisualizerRatio, bool delayed) {
	if (this->miniPlayerVisualizerRatio != miniPlayerVisualizerRatio) {
		this->miniPlayerVisualizerRatio = miniPlayerVisualizerRatio;
		if (!delayed) Save();
	}
}

void Settings::SetCaptureKeyboardMediaKeys(bool captureKeyboardMediaKeys, bool delayed) {
	if (this->captureKeyboardMediaKeys != captureKeyboardMediaKeys) {
		this->captureKeyboardMediaKeys = captureKeyboardMediaKeys;
		if (!delayed) Save();
	}
}

void Settings::SetVulkan(bool vulkan, bool delayed) {
	if (this->vulkan != vulkan) {
		this->vulkan = vulkan;
		if (!delayed) Save();
	}
}

void Settings::SetHelpDismissed(bool helpDismissed, bool delayed) {
	if (this->helpDismissed != helpDismissed) {
		this->helpDismissed = helpDismissed;
		if (!delayed) Save();
	}
}

void Settings::SetDynamicGain(const DynamicGain<float> &dynamicGain, bool delayed) {
	if (this->dynamicGain != dynamicGain) {
		this->dynamicGain = dynamicGain;
		if (!delayed) Save();
	}
}

void Settings::SetAudioOffset(std::optional<float> audioOffset, bool delayed) {
	if (this->audioOffset != audioOffset) {
		this->audioOffset = audioOffset;
		if (!delayed) Save();
	}
}

void Settings::Save() {
	if (auto path = Filesystem::GetPath(); !path.empty()) {
		LogInfo("Saving settings...");

		std::ofstream outFile(path, std::ios::out);

		Node json;
		json << *this;

		json.writeStream<Json>(outFile, NodeFormat::Beautified);
	}
}

const Node &operator>>(const Node &node, Settings::ColorSelection &colorSelection) {
	if (node.has("minPercentage"))
		node["minPercentage"]->get(colorSelection.minPercentage);

	if (node.has("minHueSeparation"))
		node["minHueSeparation"]->get(colorSelection.minHueSeparation);

	if (node.has("minValueSeparation"))
		node["minValueSeparation"]->get(colorSelection.minValueSeparation);

	if (node.has("minRgbSeparation"))
		node["minRgbSeparation"]->get(colorSelection.minRgbSeparation);

	if (node.has("minSaturation"))
		node["minSaturation"]->get(colorSelection.minSaturation);

	if (node.has("minValue"))
		node["minValue"]->get(colorSelection.minValue);

	if (node.has("maxAverageColorVariance"))
		node["maxAverageColorVariance"]->get(colorSelection.maxAverageColorVariance);
	if (node.has("maxPerPixelColorVariance"))
		node["maxPerPixelColorVariance"]->get(colorSelection.maxPerPixelColorVariance);

	return node;
}

Node &operator<<(Node &node, const Settings::ColorSelection &colorSelection) {
	node["minPercentage"]->set(colorSelection.minPercentage);
	node["minHueSeparation"]->set(colorSelection.minHueSeparation);
	node["minValueSeparation"]->set(colorSelection.minValueSeparation);
	node["minRgbSeparation"]->set(colorSelection.minRgbSeparation);
	node["minSaturation"]->set(colorSelection.minSaturation);
	node["minValue"]->set(colorSelection.minValue);
	node["maxAverageColorVariance"]->set(colorSelection.maxAverageColorVariance);
	node["maxPerPixelColorVariance"]->set(colorSelection.maxPerPixelColorVariance);

	return node;
}

const Node &operator>>(const Node &node, Settings &settings) {
	if (node.has("volume"))
		node["volume"]->get(settings.volume);

	if (node.has("exclusive"))
		node["exclusive"]->get(settings.exclusive);

	if (node.has("colorSelection"))
		node["colorSelection"]->get(settings.colorSelection);

	if (node.has("windowWidth"))
		node["windowWidth"]->get(settings.windowWidth);
	if (node.has("windowHeight"))
		node["windowHeight"]->get(settings.windowHeight);

	if (node.has("windowX"))
		node["windowX"]->get(settings.windowX);
	if (node.has("windowY"))
		node["windowY"]->get(settings.windowY);

	if (node.has("bufferSize"))
		node["bufferSize"]->get(settings.bufferLength);
	if (node.has("decayTime")) {
		settings.decayTime = Duration<Microseconds>(
			std::chrono::duration<double>(
				node["decayTime"]->get<double>()
			)
		);
	}
	if (node.has("fadeTime")) {
		settings.fadeTime = Duration<Microseconds>(
			std::chrono::duration<double>(
				node["fadeTime"]->get<double>()
			)
		);
	}

	if (node.has("pulse"))
		node["pulse"]->get(settings.pulse);
	if (node.has("pulseBackground"))
		node["pulseBackground"]->get(settings.pulseBackground);
	if (node.has("darkenPulseOnBrightColors"))
		node["darkenPulseOnBrightColors"]->get(settings.darkenPulseOnBrightColors);
	if (node.has("pulseTime")) {
		settings.pulseTime = Duration<Microseconds>(
			std::chrono::duration<double>(
				node["pulseTime"]->get<double>()
			)
		);
	}

	if (node.has("strobe"))
		node["strobe"]->get(settings.strobe);
	if (node.has("strobeIntensity"))
		node["strobeIntensity"]->get(settings.strobeIntensity);

	if (node.has("rotating"))
		node["rotating"]->get(settings.rotating);
	if (node.has("miniPlayerRotating"))
		node["miniPlayerRotating"]->get(settings.miniPlayerRotating);
	if (node.has("rotationSpeed"))
		node["rotationSpeed"]->get(settings.rotationSpeed);

	if (node.has("radius"))
		node["radius"]->get(settings.radius);

	if (node.has("detectBpm"))
		node["detectBpm"]->get(settings.detectBpm);
	if (node.has("cacheDetectionResults"))
		node["cacheDetectionResults"]->get(settings.cacheDetectionResults);
	if (node.has("halveBpm"))
		node["halveBpm"]->get(settings.halveBpm);

	if (node.has("width"))
		node["width"]->get(settings.width);

	if (node.has("renderer"))
		node["renderer"]->get(settings.renderer);
	if (node.has("lineRendererStyle"))
		node["lineRendererStyle"]->get(settings.lineRendererStyle);
	if (node.has("lightPackVisualizationType"))
		node["lightPackVisualizationType"]->get(settings.lightPackVisualizationType);
	if (node.has("lightPackMapping"))
		node["lightPackMapping"]->get(settings.lightPackMapping);
	if (node.has("lightPackFocusArea"))
		node["lightPackFocusArea"]->get(settings.lightPackFocusArea);

	if (node.has("presetIndex"))
		node["presetIndex"]->get(settings.presetIndex);
	if (node.has("miniPlayerPresetIndex"))
		node["miniPlayerPresetIndex"]->get(settings.miniPlayerPresetIndex);

	if (node.has("smooth"))
		node["smooth"]->get(settings.smooth);
	if (node.has("gamma"))
		node["gamma"]->get(settings.gamma);

	if (node.has("blur"))
		node["blur"]->get(settings.blur);
	if (node.has("blurIntensity"))
		node["blurIntensity"]->get(settings.blurIntensity);
	if (node.has("blurOpacity"))
		node["blurOpacity"]->get(settings.blurOpacity);

	if (node.has("playlistOnScreen"))
		node["playlistOnScreen"]->get(settings.playlistOnScreen);
	if (node.has("playlistFade"))
		node["playlistFade"]->get(settings.playlistFade);
	if (node.has("currentSongVisible"))
		node["currentSongVisible"]->get(settings.currentSongVisible);

	if (node.has("fftSize"))
		node["fftSize"]->get(settings.fftSize);

	if (node.has("listening"))
		node["listening"]->get(settings.listening);
	if (node.has("loopback"))
		node["loopback"]->get(settings.loopback);

	if (node.has("outputDevice") && node["outputDevice"]->type() == NodeType::String)
		node["outputDevice"]->get(settings.outputDevice);
	if (node.has("inputDevice") && node["inputDevice"]->type() == NodeType::String)
		node["inputDevice"]->get(settings.inputDevice);

	if (node.has("effect"))
		node["effect"]->get(settings.effect);
	if (node.has("effectIntensity"))
		node["effectIntensity"]->get(settings.effectIntensity);
	if (node.has("effectXOffset"))
		node["effectXOffset"]->get(settings.effectXOffset);
	if (node.has("effectYOffset"))
		node["effectYOffset"]->get(settings.effectYOffset);
	if (node.has("effectRadiation"))
		node["effectRadiation"]->get(settings.effectRadiation);
	if (node.has("effectHorizontalSpread"))
		node["effectHorizontalSpread"]->get(settings.effectHorizontalSpread);
	if (node.has("effectVerticalSpread"))
		node["effectVerticalSpread"]->get(settings.effectVerticalSpread);
	if (node.has("effectRotation"))
		node["effectRotation"]->get(settings.effectRotation);

	if (node.has("limitFramerate"))
		node["limitFramerate"]->get(settings.limitFramerate);
	if (node.has("frameLimit"))
		node["frameLimit"]->get(settings.frameLimit);

	if (node.has("randomize"))
		node["randomize"]->get(settings.randomize);
	if (node.has("randomizeTime")) {
		settings.randomizeTime = Duration<Microseconds>(
			std::chrono::duration<double>(
				node["randomizeTime"]->get<double>()
			)
		);
	}

	if (node.has("scale"))
		node["scale"]->get(settings.scale);

	if (node.has("selectedPresets"))
		node["selectedPresets"]->get(settings.selectedPresets);
	if (node.has("randomizePresets"))
		node["randomizePresets"]->get(settings.randomizePresets);
	if (node.has("randomizePresetsTime")) {
		settings.randomizePresetsTime = Duration<Microseconds>(
			std::chrono::duration<double>(
				node["randomizePresetsTime"]->get<double>()
			)
		);
	}
	if (node.has("randomizePresetsByBeats"))
		node["randomizePresetsByBeats"]->get(settings.randomizePresetsByBeats);
	if (node.has("randomizePresetsBeats"))
		node["randomizePresetsBeats"]->get(settings.randomizePresetsBeats);

	if (node.has("autoFade"))
		node["autoFade"]->get(settings.autoFade);
	if (node.has("waitTime")) {
		settings.waitTime = Duration<Microseconds>(
			std::chrono::duration<double>(
				node["waitTime"]->get<double>()
			)
		);
	}
	if (node.has("miniPlayerWaitTime")) {
		settings.miniPlayerWaitTime = Duration<Microseconds>(
			std::chrono::duration<double>(
				node["miniPlayerWaitTime"]->get<double>()
			)
		);
	}
	if (node.has("autoFadeSpeed"))
		node["autoFadeSpeed"]->get(settings.autoFadeSpeed);

	if (node.has("saveRenderer"))
		node["saveRenderer"]->get(settings.saveRenderer);
	if (node.has("saveScale"))
		node["saveScale"]->get(settings.saveScale);

	if (node.has("rendererOffset"))
		node["rendererOffset"]->get(settings.rendererOffset);

	if (node.has("sourceFactor"))
		node["sourceFactor"]->get(settings.sourceFactor);
	if (node.has("destFactor"))
		node["destFactor"]->get(settings.destFactor);
	if (node.has("sourceAlphaFactor"))
		node["sourceAlphaFactor"]->get(settings.sourceAlphaFactor);
	if (node.has("destAlphaFactor"))
		node["destAlphaFactor"]->get(settings.destAlphaFactor);

	if (node.has("lut"))
		node["lut"]->get(settings.lut);
	if (node.has("albumArtGamma"))
		node["albumArtGamma"]->get(settings.albumArtGamma);
	if (node.has("albumArtContrast"))
		node["albumArtContrast"]->get(settings.albumArtContrast);
	if (node.has("albumArtBrightness"))
		node["albumArtBrightness"]->get(settings.albumArtBrightness);

	if (node.has("hdrWhitePoint"))
		node["hdrWhitePoint"]->get(settings.hdrWhitePoint);

	if (node.has("pulseUi"))
		node["pulseUi"]->get(settings.pulseUi);
	if (node.has("uiGamma"))
		node["uiGamma"]->get(settings.uiGamma);
	if (node.has("uiContrast"))
		node["uiContrast"]->get(settings.uiContrast);
	if (node.has("uiBrightness"))
		node["uiBrightness"]->get(settings.uiBrightness);

	if (node.has("hdr"))
		node["hdr"]->get(settings.hdr);
	if (node.has("colorspace"))
		node["colorspace"]->get(settings.colorspace);

	if (node.has("pulseMaxBrightness"))
		node["pulseMaxBrightness"]->get(settings.pulseMaxBrightness);

	if (node.has("vsync"))
		node["vsync"]->get(settings.vsync);

	if (node.has("rngSource"))
		node["rngSource"]->get(settings.rngSource);

	if (node.has("miniPlayer"))
		node["miniPlayer"]->get(settings.miniPlayer);
	if (node.has("miniPlayerX"))
		node["miniPlayerX"]->get(settings.miniPlayerX);
	if (node.has("miniPlayerY"))
		node["miniPlayerY"]->get(settings.miniPlayerY);
	if (node.has("miniPlayerWidth"))
		node["miniPlayerWidth"]->get(settings.miniPlayerWidth);
	if (node.has("miniPlayerHeight"))
		node["miniPlayerHeight"]->get(settings.miniPlayerHeight);
	if (node.has("miniPlayerRadius"))
		node["miniPlayerRadius"]->get(settings.miniPlayerRadius);
	if (node.has("miniPlayerFontSize"))
		node["miniPlayerFontSize"]->get(settings.miniPlayerFontSize);
	if (node.has("miniPlayerVisualizerRatio"))
		node["miniPlayerVisualizerRatio"]->get(settings.miniPlayerVisualizerRatio);

	if (node.has("captureKeyboardMediaKeys"))
		node["captureKeyboardMediaKeys"]->get(settings.captureKeyboardMediaKeys);

	if (node.has("vulkan"))
		node["vulkan"]->get(settings.vulkan);

	if (node.has("helpDismissed"))
		node["helpDismissed"]->get(settings.helpDismissed);

	if (node.has("dynamicGain"))
		node["dynamicGain"]->get(settings.dynamicGain);

	if (node.has("audioOffset"))
		node["audioOffset"]->get(settings.audioOffset);
	else
		settings.audioOffset = std::nullopt;
		
	return node;
}

Node &operator<<(Node &node, const Settings &settings) {
	node["volume"]->set(settings.volume);
	node["exclusive"]->set(settings.exclusive);
	node["colorSelection"]->set(settings.colorSelection);
	node["windowWidth"]->set(settings.windowWidth);
	node["windowHeight"]->set(settings.windowHeight);
	node["windowX"]->set(settings.windowX);
	node["windowY"]->set(settings.windowY);
	node["bufferSize"]->set(settings.bufferLength);
	node["decayTime"]->set(settings.decayTime.AsSeconds());
	node["fadeTime"]->set(settings.fadeTime.AsSeconds());
	node["pulse"]->set(settings.pulse);
	node["pulseBackground"]->set(settings.pulseBackground);
	node["darkenPulseOnBrightColors"]->set(settings.darkenPulseOnBrightColors);
	node["pulseTime"]->set(settings.pulseTime.AsSeconds());
	node["strobe"]->set(settings.strobe);
	node["strobeIntensity"]->set(settings.strobeIntensity);
	node["rotating"]->set(settings.rotating);
	node["miniPlayerRotating"]->set(settings.miniPlayerRotating);
	node["rotationSpeed"]->set(settings.rotationSpeed);
	node["radius"]->set(settings.radius);
	node["detectBpm"]->set(settings.detectBpm);
	node["cacheDetectionResults"]->set(settings.cacheDetectionResults);
	node["halveBpm"]->set(settings.halveBpm);
	node["width"]->set(settings.width);
	node["renderer"]->set(settings.renderer);
	node["lineRendererStyle"]->set(settings.lineRendererStyle);
	node["presetIndex"]->set(settings.presetIndex);
	node["miniPlayerPresetIndex"]->set(settings.miniPlayerPresetIndex);
	node["smooth"]->set(settings.smooth);
	node["gamma"]->set(settings.gamma);
	node["blur"]->set(settings.blur);
	node["blurIntensity"]->set(settings.blurIntensity);
	node["lightPackVisualizationType"]->set(settings.lightPackVisualizationType);
	node["lightPackMapping"]->set(settings.lightPackMapping);
	node["lightPackFocusArea"]->set(settings.lightPackFocusArea);
	node["playlistOnScreen"]->set(settings.playlistOnScreen);
	node["playlistFade"]->set(settings.playlistFade);
	node["currentSongVisible"]->set(settings.currentSongVisible);
	node["fftSize"]->set(settings.fftSize);
	node["listening"]->set(settings.listening);
	node["loopback"]->set(settings.loopback);
	node["outputDevice"]->set(settings.outputDevice);
	node["inputDevice"]->set(settings.inputDevice);
	node["effect"]->set(settings.effect);
	node["effectIntensity"]->set(settings.effectIntensity);
	node["effectXOffset"]->set(settings.effectXOffset);
	node["effectYOffset"]->set(settings.effectYOffset);
	node["effectRadiation"]->set(settings.effectRadiation);
	node["effectHorizontalSpread"]->set(settings.effectHorizontalSpread);
	node["effectVerticalSpread"]->set(settings.effectVerticalSpread);
	node["blurOpacity"]->set(settings.blurOpacity);
	node["limitFramerate"]->set(settings.limitFramerate);
	node["frameLimit"]->set(settings.frameLimit);
	node["randomize"]->set(settings.randomize);
	node["randomizeTime"]->set(settings.randomizeTime.AsSeconds());
	node["scale"]->set(settings.scale);
	node["selectedPresets"]->set(settings.selectedPresets);
	node["randomizePresets"]->set(settings.randomizePresets);
	node["randomizePresetsTime"]->set(settings.randomizePresetsTime.AsSeconds());
	node["randomizePresetsByBeats"]->set(settings.randomizePresetsByBeats);
	node["randomizePresetsBeats"]->set(settings.randomizePresetsBeats);
	node["effectRotation"]->set(settings.effectRotation);
	node["autoFade"]->set(settings.autoFade);
	node["waitTime"]->set(settings.waitTime.AsSeconds());
	node["miniPlayerWaitTime"]->set(settings.miniPlayerWaitTime.AsSeconds());
	node["autoFadeSpeed"]->set(settings.autoFadeSpeed);
	node["saveRenderer"]->set(settings.saveRenderer);
	node["saveScale"]->set(settings.saveScale);
	node["rendererOffset"]->set(settings.rendererOffset);
	node["sourceFactor"]->set(settings.sourceFactor);
	node["destFactor"]->set(settings.destFactor);
	node["sourceAlphaFactor"]->set(settings.sourceAlphaFactor);
	node["destAlphaFactor"]->set(settings.destAlphaFactor);
	node["lut"]->set(settings.lut);
	node["albumArtGamma"]->set(settings.albumArtGamma);
	node["albumArtContrast"]->set(settings.albumArtContrast);
	node["albumArtBrightness"]->set(settings.albumArtBrightness);
	node["hdrWhitePoint"]->set(settings.hdrWhitePoint);
	node["pulseUi"]->set(settings.pulseUi);
	node["uiGamma"]->set(settings.uiGamma);
	node["uiContrast"]->set(settings.uiContrast);
	node["uiBrightness"]->set(settings.uiBrightness);
	node["hdr"]->set(settings.hdr);
	node["colorspace"]->set(settings.colorspace);
	node["pulseMaxBrightness"]->set(settings.pulseMaxBrightness);
	node["vsync"]->set(settings.vsync);
	node["rngSource"]->set(settings.rngSource);
	node["miniPlayer"]->set(settings.miniPlayer);
	node["miniPlayerX"]->set(settings.miniPlayerX);
	node["miniPlayerY"]->set(settings.miniPlayerY);
	node["miniPlayerWidth"]->set(settings.miniPlayerWidth);
	node["miniPlayerHeight"]->set(settings.miniPlayerHeight);
	node["miniPlayerRadius"]->set(settings.miniPlayerRadius);
	node["miniPlayerFontSize"]->set(settings.miniPlayerFontSize);
	node["miniPlayerVisualizerRatio"]->set(settings.miniPlayerVisualizerRatio);
	node["captureKeyboardMediaKeys"]->set(settings.captureKeyboardMediaKeys);
	node["vulkan"]->set(settings.vulkan);
	node["helpDismissed"]->set(settings.helpDismissed);
	node["dynamicGain"]->set(settings.dynamicGain);
	if (settings.audioOffset)
		node["audioOffset"]->set(settings.audioOffset);
	else
		node.remove("audioOffset");

	return node;
}