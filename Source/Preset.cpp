#include "Preset.hpp"

#include <fstream>
#include <random>

#include "Utils/Filesystem.hpp"

#include "Settings.hpp"
#include "Utils.hpp"

std::vector<Preset> Preset::Presets;
std::set<Preset::ChangeListener *> Preset::ChangeListeners;

std::vector<Preset> Preset::Load() {
	std::vector<Preset> ret;

	std::ifstream inFile;
	if (std::filesystem::exists(Filesystem::GetPath("Presets.json")))
		inFile.open(Filesystem::GetPath("Presets.json"));
	else
		inFile.open(Utils::GetResource("Presets.json"));

	LoggableClass errorLog(typeid(Preset).name());

	if (inFile) {
		try {
			Node json;
			json.parseStream<Json>(inFile);

			for (auto &&node : json.get<std::map<std::size_t, Preset>>()) {
				ret.emplace_back(std::move(node.second));
			}
		} catch (std::exception &e) {
			errorLog.LogError("Could not parse presets: ", e.what());
		}
	} else errorLog.LogError("Presets file not found!");

	return ret;
}

Preset Preset::Random(const DynamicGain<float> &dynamicGain) {
	std::mt19937 prng(time(nullptr));

	auto files = Utils::GetFiles(Utils::GetResourceFolder() / "Effects");
	std::vector<std::string> effectNames;
	for (const auto &file : files) {
		auto effectName = file.stem().u8string();
		effectName = effectName.substr(effectName.find_first_of('-') + 1);

		// Make sure "No Effect" is at index 0
		if (effectName == "noeffect")
			effectNames.emplace(effectNames.begin(), std::move(effectName));
		else
			effectNames.emplace_back(std::move(effectName));
	}

	Preset ret(
		"Random",
		1 + prng() % 512, // limit to 512
		8192, // use default FFT size
		Duration<Microseconds>(
			std::chrono::duration<double>(
				(prng() % 10000) / 5000.0f // 0 - 2
			)
		),
		Duration<Microseconds>(
			std::chrono::duration<double>(
				(prng() % 10000) / 5000.0f // 0 - 2
			)
		),
		prng() % 2,
		false,
		Duration<Microseconds>(
			std::chrono::duration<double>(
				(prng() % 10000) / 5000.0f // 0 - 2
			)
		),
		prng() % 2,
		0.25f + (prng() % 10000) / 13333.0f, // 0.25 - 1.0
		true, // always rotate
		(1 + prng() % 25) * 6, // limit to 25 RPM max
		true, // always blur,
		GL_ONE,
		GL_ZERO,
		(prng() % 40000) / 10000.0f,
		0.25f + (prng() % 10000) / 13333.0f, // 0.25 - 1.0
		effectNames[1 + (prng() % (effectNames.size() - 1))], // skip "noeffect"
		(prng() % 10000) / 400.0f,
		(prng() % 10000) / 1000.0f * ((prng() % 1) ? -1 : 1),
		(prng() % 10000) / 1000.0f * ((prng() % 1) ? -1 : 1),
		(prng() % 10000) / 1000.0f * ((prng() % 1) ? -1 : 1),
		(prng() % 10000) / 1000.0f * ((prng() % 1) ? -1 : 1),
		(prng() % 10000) / 1000.0f * ((prng() % 1) ? -1 : 1),
		(prng() % 10000) / 2000.0f * ((prng() % 1) ? -1 : 1),
		dynamicGain
	);

	return ret;
}

void Preset::Save() {
	std::ofstream outFile(Filesystem::GetPath("Presets.json"));

	std::map<std::size_t, const Preset *> mapped;
	for (const auto &[i, existing] : Utils::Enumerate(Presets)) {
		mapped.emplace(std::make_pair(i, &existing));
	}

	Node json;
	json << mapped;
	json.writeStream<Json>(outFile, NodeFormat::Beautified);
}

void Preset::AddPreset(Preset &&preset) {
	Presets.emplace_back(std::move(preset));

	Save();

	for (auto listener : ChangeListeners)
		listener->OnPresetsChanged(Presets);
}

void Preset::RemovePreset(std::size_t index) {
	Presets.erase(Presets.begin() + index);

	Save();

	for (auto listener : ChangeListeners)
		listener->OnPresetsChanged(Presets);
}

void Preset::AddChangeListener(ChangeListener *listener) {
	ChangeListeners.emplace(listener);
}

void Preset::RemoveChangeListener(ChangeListener *listener) {
	ChangeListeners.erase(listener);
}

const Node &operator>>(const Node &node, Preset &preset) {
	node["name"]->get(preset.name);

	node["bufferSize"]->get(preset.bufferSize);
	if (node.has("fftSize"))
		node["fftSize"]->get(preset.fftSize);

	preset.decayTime = Duration<Microseconds>(
		std::chrono::duration<double>(
			node["decayTime"]->get<double>()
		)
	);
	preset.fadeDecayTime = Duration<Microseconds>(
		std::chrono::duration<double>(
			node["fadeTime"]->get<double>()
		)
	);

	node["pulse"]->get(preset.pulse);

	if (node.has("pulseBackground"))
		node["pulseBackground"]->get(preset.pulseBackground);

	preset.pulseTime = Duration<Microseconds>(
		std::chrono::duration<double>(
			node["pulseTime"]->get<double>()
		)
	);

	if (node.has("strobe"))
		node["strobe"]->get(preset.strobe);
	if (node.has("strobeIntensity"))
		node["strobeIntensity"]->get(preset.strobeIntensity);

	if (node.has("rotating"))
		node["rotating"]->get(preset.rotating);
	if (node.has("rotationSpeed"))
		node["rotationSpeed"]->get(preset.rotationSpeed);

	if (node.has("blur"))
		node["blur"]->get(preset.blur);
	if (node.has("sourceFactor"))
		node["sourceFactor"]->get(preset.sourceFactor);
	if (node.has("destFactor"))
		node["destFactor"]->get(preset.destFactor);
	if (node.has("sourceAlphaFactor"))
		node["sourceAlphaFactor"]->get(preset.sourceAlphaFactor);
	else
		preset.sourceAlphaFactor = preset.sourceFactor;
	if (node.has("destAlphaFactor"))
		node["destAlphaFactor"]->get(preset.destAlphaFactor);
	else
		preset.destAlphaFactor = preset.destFactor;

	if (node.has("blurIntensity"))
		node["blurIntensity"]->get(preset.blurIntensity);
	if (node.has("blurOpacity"))
		node["blurOpacity"]->get(preset.blurOpacity);

	if (node.has("effect"))
		node["effect"]->get(preset.effect);
	if (node.has("effectIntensity"))
		node["effectIntensity"]->get(preset.effectIntensity);
	if (node.has("effectXOffset"))
		node["effectXOffset"]->get(preset.effectXOffset);
	if (node.has("effectYOffset"))
		node["effectYOffset"]->get(preset.effectYOffset);
	if (node.has("effectRadiation"))
		node["effectRadiation"]->get(preset.effectRadiation);
	if (node.has("effectHorizontalSpread"))
		node["effectHorizontalSpread"]->get(preset.effectHorizontalSpread);
	if (node.has("effectVerticalSpread"))
		node["effectVerticalSpread"]->get(preset.effectVerticalSpread);
	if (node.has("effectRotation"))
		node["effectRotation"]->get(preset.effectRotation);

	if (node.has("renderer"))
		node["renderer"]->get(preset.renderer);
	
	if (node.has("scale"))
		node["scale"]->get(preset.scale);

	if (node.has("rendererOffset"))
		node["rendererOffset"]->get(preset.rendererOffset);

	if (node.has("availableInMiniPlayer"))
		node["availableInMiniPlayer"]->get(preset.availableInMiniPlayer);

	if (node.has("dynamicGain"))
		node["dynamicGain"]->get(preset.dynamicGain);
	else 
		preset.dynamicGain = Settings::GetBaseDynamicGain();

	return node;
}

Node &operator<<(Node &node, const Preset &preset) {
	node["name"]->set(preset.name);

	node["bufferSize"]->set(preset.bufferSize);
	node["fftSize"]->set(preset.fftSize);
	node["decayTime"]->set(preset.decayTime.AsSeconds());
	node["fadeTime"]->set(preset.fadeDecayTime.AsSeconds());
	node["pulse"]->set(preset.pulse);
	node["pulseBackground"]->set(preset.pulseBackground);
	node["pulseTime"]->set(preset.pulseTime.AsSeconds());
	node["strobe"]->set(preset.strobe);
	node["strobeIntensity"]->set(preset.strobeIntensity);
	node["rotating"]->set(preset.rotating);
	node["rotationSpeed"]->set(preset.rotationSpeed);
	node["blur"]->set(preset.blur);
	node["sourceFactor"]->set(preset.sourceFactor);
	node["destFactor"]->set(preset.destFactor);
	node["blurIntensity"]->set(preset.blurIntensity);
	node["blurOpacity"]->set(preset.blurOpacity);
	node["effect"]->set(preset.effect);
	node["effectIntensity"]->set(preset.effectIntensity);
	node["effectXOffset"]->set(preset.effectXOffset);
	node["effectYOffset"]->set(preset.effectYOffset);
	node["effectRadiation"]->set(preset.effectRadiation);
	node["effectHorizontalSpread"]->set(preset.effectHorizontalSpread);
	node["effectVerticalSpread"]->set(preset.effectVerticalSpread);
	node["effectRotation"]->set(preset.effectRotation);
	node["availableInMiniPlayer"]->set(preset.availableInMiniPlayer);
	node["dynamicGain"]->set(preset.dynamicGain);
	if (preset.renderer)
		node["renderer"]->set(preset.renderer);
	if (preset.scale)
		node["scale"]->set(preset.scale);
	if (preset.rendererOffset)
		node["rendererOffset"]->set(preset.rendererOffset);

	return node;
}