#include "Settings.hpp"

#include <fstream>

#ifdef _WIN32
#include <Shlobj.h>
#endif

Settings Settings::settings = Settings::Load();

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
	{ GL_SRC1_COLOR, "GL_SRC1_COLOR"},
	{ GL_ONE_MINUS_SRC1_COLOR, "GL_ONE_MINUS_SRC1_COLOR"},
	{ GL_SRC1_ALPHA, "GL_SRC1_ALPHA"},
	{ GL_ONE_MINUS_SRC1_ALPHA, "GL_ONE_MINUS_SRC1_ALPHA"}
};

std::filesystem::path Settings::GetPath(const std::string &fileName) {
	std::filesystem::path ret;

#ifdef _WIN32
	PWSTR folder;
	if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &folder) == S_OK) {
		ret = std::filesystem::path(folder);

		CoTaskMemFree(folder);
	}
#elif defined(__linux__)
	ret = std::filesystem::path(getenv("HOME")) / ".config";
	if (!std::filesystem::exists(ret))
		std::filesystem::create_directory(ret);
#endif

	if (!ret.empty()) {
		ret /= "Fetcko";
		if (!std::filesystem::exists(ret))
			std::filesystem::create_directory(ret);

		ret /= "popRocks";
		if (!std::filesystem::exists(ret))
			std::filesystem::create_directory(ret);

		ret /= fileName;
	}

	return ret;
}

Settings Settings::Load() {
	Settings ret;

	LoggableClass errorLog(typeid(Settings).name());

	if (auto path = GetPath(); !path.empty()) {
		std::ifstream inFile(path, std::ios::in);

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
			errorLog.LogWarning("Could not load settings due to ", e.what());
		}
	}

	return ret;
}

void Settings::SetVolume(float volume) {
	if (volume != this->volume) {
		this->volume = volume;
		Save();
	}
}

void Settings::SetExclusive(bool exclusive) {
	if (exclusive != this->exclusive) {
		this->exclusive = exclusive;
		Save();
	}
}

void Settings::SetColorSelection(ColorSelection colorSelection) {
	if (colorSelection != this->colorSelection) {
		this->colorSelection = colorSelection;
		Save();
	}
}

void Settings::SetWindowWidth(int windowWidth) {
	if (windowWidth != this->windowWidth) {
		this->windowWidth = windowWidth;
		Save();
	}
}

void Settings::SetWindowHeight(int windowHeight) {
	if (windowHeight != this->windowHeight) {
		this->windowHeight = windowHeight;
		Save();
	}
}

void Settings::SetWindowX(int windowX) {
	if (windowX != this->windowX) {
		this->windowX = windowX;
		Save();
	}
}

void Settings::SetWindowY(int windowY) {
	if (windowY != this->windowY) {
		this->windowY = windowY;
		Save();
	}
}

void Settings::SetBufferLength(std::size_t bufferLength) {
	if (bufferLength != this->bufferLength) {
		this->bufferLength = bufferLength;
		Save();
	}
}

void Settings::SetDecayTime(Duration<Microseconds> decayTime) {
	if (decayTime != this->decayTime) {
		this->decayTime = decayTime;
		Save();
	}
}

void Settings::SetFadeTime(Duration<Microseconds> fadeTime) {
	if (fadeTime != this->fadeTime) {
		this->fadeTime = fadeTime;
		Save();
	}
}

void Settings::SetPulse(bool pulse) {
	if (pulse != this->pulse) {
		this->pulse = pulse;
		Save();
	}
}

void Settings::SetDarkenPulseOnBrightColors(bool darkenPulseOnBrightColors) {
	if (darkenPulseOnBrightColors != this->darkenPulseOnBrightColors) {
		this->darkenPulseOnBrightColors = darkenPulseOnBrightColors;
		Save();
	}
}

void Settings::SetPulseTime(Duration<Microseconds> pulseTime) {
	if (pulseTime != this->pulseTime) {
		this->pulseTime = pulseTime;
		Save();
	}
}

void Settings::SetStrobe(bool strobe) {
	if (strobe != this->strobe) {
		this->strobe = strobe;
		Save();
	}
}

void Settings::SetStrobeIntensity(float strobeIntensity) {
	if (strobeIntensity != this->strobeIntensity) {
		this->strobeIntensity = strobeIntensity;
		Save();
	}
}

void Settings::SetRotating(bool rotating) {
	if (rotating != this->rotating) {
		this->rotating = rotating;
		Save();
	}
}

void Settings::SetRotationSpeed(float rotationSpeed) {
	if (rotationSpeed != this->rotationSpeed) {
		this->rotationSpeed = rotationSpeed;
		Save();
	}
}

void Settings::SetRadius(float radius) {
	if (radius != this->radius) {
		this->radius = radius;
		Save();
	}
}

void Settings::SetDetectBpm(bool detectBpm) {
	if (detectBpm != this->detectBpm) {
		this->detectBpm = detectBpm;
		Save();
	}
}

void Settings::SetCacheDetectionResults(bool cacheDetectionResults) {
	if (cacheDetectionResults != this->cacheDetectionResults) {
		this->cacheDetectionResults = cacheDetectionResults;
		Save();
	}
}

void Settings::SetHalveBpm(bool halveBpm) {
	if (halveBpm != this->halveBpm) {
		this->halveBpm = halveBpm;
		Save();
	}
}

void Settings::SetWidth(float width) {
	if (width != this->width) {
		this->width = width;
		Save();
	}
}

void Settings::SetRenderer(const std::string &renderer) {
	if (renderer != this->renderer) {
		this->renderer = renderer;
		Save();
	}
}

void Settings::SetLightPackVisualizationType(const std::string &lightPackVisualizationType) {
	if (lightPackVisualizationType != this->lightPackVisualizationType) {
		this->lightPackVisualizationType = lightPackVisualizationType;
		Save();
	}
}

void Settings::SetLightPackMapping(const std::string &lightPackMapping) {
	if (lightPackMapping != this->lightPackMapping) {
		this->lightPackMapping = lightPackMapping;
		Save();
	}
}

void Settings::SetLightPackFocusArea(const std::string &lightPackFocusArea) {
	if (lightPackFocusArea != this->lightPackFocusArea) {
		this->lightPackFocusArea = lightPackFocusArea;
		Save();
	}
}

void Settings::SetPresetIndex(std::optional<std::size_t> presetIndex) {
	if (presetIndex != this->presetIndex) {
		this->presetIndex = presetIndex;
		Save();
	}
}

void Settings::SetSmooth(uint8_t smooth) {
	if (smooth != this->smooth) {
		this->smooth = smooth;
		Save();
	}
}

void Settings::SetGamma(float gamma) {
	if (gamma != this->gamma) {
		this->gamma = gamma;
		Save();
	}
}

void Settings::SetBlur(bool blur) {
	if (blur != this->blur) {
		this->blur = blur;
		Save();
	}
}

void Settings::SetBlurIntensity(float blurIntensity) {
	if (blurIntensity != this->blurIntensity) {
		this->blurIntensity = blurIntensity;
		Save();
	}
}

void Settings::SetBlurOpacity(float blurOpacity) {
	if (blurOpacity != this->blurOpacity) {
		this->blurOpacity = blurOpacity;
		Save();
	}
}

void Settings::SetCurrentSongVisible(bool currentSongVisible) {
	if (currentSongVisible != this->currentSongVisible) {
		this->currentSongVisible = currentSongVisible;
		Save();
	}
}

void Settings::SetFftSize(int fftSize) {
	if (fftSize != this->fftSize) {
		this->fftSize = fftSize;
		Save();
	}
}

void Settings::SetListening(bool listening) {
	if (listening != this->listening) {
		this->listening = listening;
		if (listening) loopback = false;
		Save();
	}
}

void Settings::SetLoopback(bool loopback) {
	if (loopback != this->loopback) {
		this->loopback = loopback;
		if (loopback) listening = false;
		Save();
	}
}

void Settings::SetOutputDevice(const std::string &outputDevice) {
	if (outputDevice != this->outputDevice) {
		this->outputDevice = outputDevice;
		Save();
	}
}

void Settings::SetInputDevice(const std::string &inputDevice) {
	if (inputDevice != this->inputDevice) {
		this->inputDevice = inputDevice;
		Save();
	}
}

void Settings::SetEffect(const std::string &effect) {
	if (effect != this->effect) {
		this->effect = effect;
		Save();
	}
}

void Settings::SetEffectIntensity(float effectIntensity) {
	if (effectIntensity != this->effectIntensity) {
		this->effectIntensity = effectIntensity;
		Save();
	}
}

void Settings::SetEffectXOffset(float effectXOffset) {
	if (effectXOffset != this->effectXOffset) {
		this->effectXOffset = effectXOffset;
		Save();
	}
}

void Settings::SetEffectYOffset(float effectYOffset) {
	if (effectYOffset != this->effectYOffset) {
		this->effectYOffset = effectYOffset;
		Save();
	}
}

void Settings::SetEffectRadiation(float effectRadiation) {
	if (effectRadiation != this->effectRadiation) {
		this->effectRadiation = effectRadiation;
		Save();
	}
}

void Settings::SetEffectHorizontalSpread(float effectHorizontalSpread) {
	if (effectHorizontalSpread != this->effectHorizontalSpread) {
		this->effectHorizontalSpread = effectHorizontalSpread;
		Save();
	}
}

void Settings::SetEffectVerticalSpread(float effectVerticalSpread) {
	if (effectVerticalSpread != this->effectVerticalSpread) {
		this->effectVerticalSpread = effectVerticalSpread;
		Save();
	}
}

void Settings::SetEffectRotation(float effectRotation) {
	if (effectRotation != this->effectRotation) {
		this->effectRotation = effectRotation;
		Save();
	}
}

void Settings::SetLimitFramerate(bool limitFramerate) {
	if (limitFramerate != this->limitFramerate) {
		this->limitFramerate = limitFramerate;
		Save();
	}
}

void Settings::SetFrameLimit(int frameLimit) {
	if (frameLimit != this->frameLimit) {
		this->frameLimit = frameLimit;
		Save();
	}
}

void Settings::SetRandomize(bool randomize) {
	if (randomize != this->randomize) {
		this->randomize = randomize;
		Save();
	}
}

void Settings::SetRandomizeTime(Duration<Microseconds> randomizeTime) {
	if (randomizeTime != this->randomizeTime) {
		this->randomizeTime = randomizeTime;
		Save();
	}
}

void Settings::SetScale(float scale) {
	if (scale != this->scale) {
		this->scale = scale;
		Save();
	}
}

void Settings::SetSelectedPresets(const std::set<std::size_t> &selectedPresets) {
	if (selectedPresets != this->selectedPresets) {
		this->selectedPresets = selectedPresets;
		Save();
	}
}

void Settings::SetRandomizePresets(bool randomizePresets) {
	if (randomizePresets != this->randomizePresets) {
		this->randomizePresets = randomizePresets;
		Save();
	}
}

void Settings::SetRandomizePresetsTime(Duration<Microseconds> randomizePresetsTime) {
	if (randomizePresetsTime != this->randomizePresetsTime) {
		this->randomizePresetsTime = randomizePresetsTime;
		Save();
	}
}

void Settings::SetRandomizePresetsByBeats(bool randomizePresetsByBeats) {
	if (randomizePresetsByBeats != this->randomizePresetsByBeats) {
		this->randomizePresetsByBeats = randomizePresetsByBeats;
		Save();
	}
}

void Settings::SetRandomizePresetsBeats(int randomizePresetsBeats) {
	if (randomizePresetsBeats != this->randomizePresetsBeats) {
		this->randomizePresetsBeats = randomizePresetsBeats;
		Save();
	}
}

void Settings::SetAutoFade(bool autoFade) {
	if (autoFade != this->autoFade) {
		this->autoFade = autoFade;
		Save();
	}
}

void Settings::SetWaitTime(Duration<Microseconds> waitTime) {
	if (waitTime != this->waitTime) {
		this->waitTime = waitTime;
		Save();
	}
}

void Settings::SetAutoFadeSpeed(float autoFadeSpeed) {
	if (autoFadeSpeed != this->autoFadeSpeed) {
		this->autoFadeSpeed = autoFadeSpeed;
		Save();
	}
}

void Settings::SetSaveRenderer(bool saveRenderer) {
	if (saveRenderer != this->saveRenderer) {
		this->saveRenderer = saveRenderer;
		Save();
	}
}

void Settings::SetSaveScale(bool saveScale) {
	if (saveScale != this->saveScale) {
		this->saveScale = saveScale;
		Save();
	}
}

void Settings::SetRendererOffset(int rendererOffset) {
	if (rendererOffset != this->rendererOffset) {
		this->rendererOffset = rendererOffset;
		Save();
	}
}

void Settings::SetPulseBackground(bool pulseBackground) {
	if (pulseBackground != this->pulseBackground) {
		this->pulseBackground = pulseBackground;
		Save();
	}
}

void Settings::SetSourceFactor(GLenum sourceFactor) {
	if (this->sourceFactor != sourceFactor) {
		this->sourceFactor = sourceFactor;
		Save();
	}
}

void Settings::SetDestFactor(GLenum destFactor) {
	if (this->destFactor != destFactor) {
		this->destFactor = destFactor;
		Save();
	}
}

void Settings::SetLut(const std::string &lut) {
	if (this->lut != lut) {
		this->lut = lut;
		Save();
	}
}

void Settings::SetAlbumArtGamma(float albumArtGamma) {
	if (this->albumArtGamma != albumArtGamma) {
		this->albumArtGamma = albumArtGamma;
		Save();
	}
}

void Settings::SetAlbumArtContrast(float albumArtContrast) {
	if (this->albumArtContrast != albumArtContrast) {
		this->albumArtContrast = albumArtContrast;
		Save();
	}
}

void Settings::SetAlbumArtBrightness(float albumArtBrightness) {
	if (this->albumArtBrightness != albumArtBrightness) {
		this->albumArtBrightness = albumArtBrightness;
		Save();
	}
}

void Settings::SetHdrWhitePoint(std::optional<float> hdrWhitePoint) {
	if (this->hdrWhitePoint != hdrWhitePoint) {
		this->hdrWhitePoint = hdrWhitePoint;
		Save();
	}
}

void Settings::Save() {
	if (auto path = GetPath(); !path.empty()) {
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

	return node;
}

Node &operator<<(Node &node, const Settings::ColorSelection &colorSelection) {
	node["minPercentage"]->set(colorSelection.minPercentage);
	node["minHueSeparation"]->set(colorSelection.minHueSeparation);
	node["minValueSeparation"]->set(colorSelection.minValueSeparation);
	node["minRgbSeparation"]->set(colorSelection.minRgbSeparation);
	node["minSaturation"]->set(colorSelection.minSaturation);
	node["minValue"]->set(colorSelection.minValue);

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
	if (node.has("lightPackVisualizationType"))
		node["lightPackVisualizationType"]->get(settings.lightPackVisualizationType);
	if (node.has("lightPackMapping"))
		node["lightPackMapping"]->get(settings.lightPackMapping);
	if (node.has("lightPackFocusArea"))
		node["lightPackFocusArea"]->get(settings.lightPackFocusArea);

	if (node.has("presetIndex"))
		node["presetIndex"]->get(settings.presetIndex);

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
	node["rotationSpeed"]->set(settings.rotationSpeed);
	node["radius"]->set(settings.radius);
	node["detectBpm"]->set(settings.detectBpm);
	node["cacheDetectionResults"]->set(settings.cacheDetectionResults);
	node["halveBpm"]->set(settings.halveBpm);
	node["width"]->set(settings.width);
	node["renderer"]->set(settings.renderer);
	node["presetIndex"]->set(settings.presetIndex);
	node["smooth"]->set(settings.smooth);
	node["gamma"]->set(settings.gamma);
	node["blur"]->set(settings.blur);
	node["blurIntensity"]->set(settings.blurIntensity);
	node["lightPackVisualizationType"]->set(settings.lightPackVisualizationType);
	node["lightPackMapping"]->set(settings.lightPackMapping);
	node["lightPackFocusArea"]->set(settings.lightPackFocusArea);
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
	node["autoFadeSpeed"]->set(settings.autoFadeSpeed);
	node["saveRenderer"]->set(settings.saveRenderer);
	node["saveScale"]->set(settings.saveScale);
	node["rendererOffset"]->set(settings.rendererOffset);
	node["sourceFactor"]->set(settings.sourceFactor);
	node["destFactor"]->set(settings.destFactor);
	node["lut"]->set(settings.lut);
	node["albumArtGamma"]->set(settings.albumArtGamma);
	node["albumArtContrast"]->set(settings.albumArtContrast);
	node["albumArtBrightness"]->set(settings.albumArtBrightness);
	node["hdrWhitePoint"]->set(settings.hdrWhitePoint);

	return node;
}