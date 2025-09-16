#include "Settings.hpp"

#include <fstream>

#ifdef _WIN32
#include <Shlobj.h>
#endif

Settings Settings::settings = Settings::Load();

std::filesystem::path Settings::GetPath(const std::string &fileName) {
	std::filesystem::path ret;

#ifdef _WIN32
	PWSTR folder;
	if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &folder) == S_OK) {
		ret = std::filesystem::path(folder);

		CoTaskMemFree(folder);
	}
#elif defined(__linux__)
	ret = "~/.config";
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
	this->volume = volume;
	Save();
}

void Settings::SetExclusive(bool exclusive) {
	this->exclusive = exclusive;
	Save();
}

void Settings::SetColorSelection(ColorSelection colorSelection) {
	this->colorSelection = colorSelection;
	Save();
}

void Settings::SetWindowWidth(int windowWidth) {
	this->windowWidth = windowWidth;
	Save();
}

void Settings::SetWindowHeight(int windowHeight) {
	this->windowHeight = windowHeight;
	Save();
}

void Settings::SetWindowX(int windowX) {
	this->windowX = windowX;
	Save();
}

void Settings::SetWindowY(int windowY) {
	this->windowY = windowY;
	Save();
}

void Settings::SetBufferLength(std::size_t bufferLength) {
	this->bufferLength = bufferLength;
	Save();
}

void Settings::SetDecayTime(Duration<Microseconds> decayTime) {
	this->decayTime = decayTime;
	Save();
}

void Settings::SetFadeTime(Duration<Microseconds> fadeTime) {
	this->fadeTime = fadeTime;
	Save();
}

void Settings::SetPulse(bool pulse) {
	this->pulse = pulse;
	Save();
}

void Settings::SetPulseTime(Duration<Microseconds> pulseTime) {
	this->pulseTime = pulseTime;
	Save();
}

void Settings::SetRotating(bool rotating) {
	this->rotating = rotating;
	Save();
}

void Settings::SetRotationSpeed(float rotationSpeed) {
	this->rotationSpeed = rotationSpeed;
	Save();
}

void Settings::SetRadius(float radius) {
	this->radius = radius;
	Save();
}

void Settings::SetDetectBpm(bool detectBpm) {
	this->detectBpm = detectBpm;
	Save();
}

void Settings::SetWidth(float width) {
	this->width = width;
	Save();
}

void Settings::SetRenderer(const std::string &renderer) {
	this->renderer = renderer;
	Save();
}

void Settings::SetLightPackVisualizationType(const std::string &lightPackVisualizationType) {
	this->lightPackVisualizationType = lightPackVisualizationType;
	Save();
}

void Settings::SetLightPackMapping(const std::string &lightPackMapping) {
	this->lightPackMapping = lightPackMapping;
	Save();
}

void Settings::SetLightPackFocusArea(const std::string &lightPackFocusArea) {
	this->lightPackFocusArea = lightPackFocusArea;
	Save();
}

void Settings::SetPresetIndex(std::optional<std::size_t> presetIndex) {
	this->presetIndex = presetIndex;
	Save();
}

void Settings::SetSmooth(uint8_t smooth) {
	this->smooth = smooth;
	Save();
}

void Settings::SetGamma(float gamma) {
	this->gamma = gamma;
	Save();
}

void Settings::SetBlur(bool blur) {
	this->blur = blur;
	Save();
}

void Settings::SetBlurIntensity(float blurIntensity) {
	this->blurIntensity = blurIntensity;
	Save();
}

void Settings::SetCurrentSongVisible(bool currentSongVisible) {
	this->currentSongVisible = currentSongVisible;
	Save();
}

void Settings::SetFftSize(int fftSize) {
	this->fftSize = fftSize;
	Save();
}

void Settings::SetListening(bool listening) {
	this->listening = listening;
	if (listening) loopback = false;
	Save();
}

void Settings::SetLoopback(bool loopback) {
	this->loopback = loopback;
	if (loopback) listening = false;
	Save();
}

void Settings::SetOutputDevice(const std::string &outputDevice) {
	this->outputDevice = outputDevice;
	Save();
}

void Settings::SetInputDevice(const std::string &inputDevice) {
	this->inputDevice = inputDevice;
	Save();
}

void Settings::SetEffect(const std::string &effect) {
	this->effect = effect;
	Save();
}

void Settings::SetEffectIntensity(float effectIntensity) {
	this->effectIntensity = effectIntensity;
	Save();
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
	if (node.has("pulseTime")) {
		settings.pulseTime = Duration<Microseconds>(
			std::chrono::duration<double>(
				node["pulseTime"]->get<double>()
			)
		);
	}

	if (node.has("rotating"))
		node["rotating"]->get(settings.rotating);
	if (node.has("rotationSpeed"))
		node["rotationSpeed"]->get(settings.rotationSpeed);

	if (node.has("radius"))
		node["radius"]->get(settings.radius);

	if (node.has("detectBpm"))
		node["detectBpm"]->get(settings.detectBpm);

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
	node["pulseTime"]->set(settings.pulseTime.AsSeconds());
	node["rotating"]->set(settings.rotating);
	node["rotationSpeed"]->set(settings.rotationSpeed);
	node["radius"]->set(settings.radius);
	node["detectBpm"]->set(settings.detectBpm);
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

	return node;
}