#include "Preset.hpp"

#include <fstream>

#include "Settings.hpp"
#include "Utils.hpp"

std::vector<Preset> Preset::Presets = Preset::Load();

std::vector<Preset> Preset::Load() {
	std::vector<Preset> ret;

	std::ifstream inFile;
	if (std::filesystem::exists(Settings::GetPath("Presets.json")))
		inFile.open(Settings::GetPath("Presets.json"));
	else
		inFile.open(Utils::GetResource("Presets.json"));

	LoggableClass errorLog(typeid(Preset).name());

	try {
		Node json;
		json.parseStream<Json>(inFile);

		for (auto &&node : json.get<std::map<std::size_t, Preset>>()) {
			ret.emplace_back(std::move(node.second));
		}
	} catch (std::exception &e) {
		errorLog.LogError("Could not parse presets: ", e.what());
	}

	return ret;
}

void Preset::Save() {
	std::ofstream outFile(Settings::GetPath("Presets.json"));

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
}

void Preset::RemovePreset(std::size_t index) {
	Presets.erase(Presets.begin() + index);

	Save();
}

const Node &operator>>(const Node &node, Preset &preset) {
	node["name"]->get(preset.name);

	node["bufferSize"]->get(preset.bufferSize);

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

	preset.pulse = node["pulse"]->get<bool>();

	preset.pulseTime = Duration<Microseconds>(
		std::chrono::duration<double>(
			node["pulseTime"]->get<double>()
		)
	);

	if (node.has("rotating"))
		node["rotating"]->get(preset.rotating);
	if (node.has("rotationSpeed"))
		node["rotationSpeed"]->get(preset.rotationSpeed);

	if (node.has("blur"))
		node["blur"]->get(preset.blur);
	if (node.has("blurIntensity"))
		node["blurIntensity"]->get(preset.blurIntensity);

	return node;
}

Node &operator<<(Node &node, const Preset &preset) {
	node["name"]->set(preset.name);

	node["bufferSize"]->set(preset.bufferSize);
	node["decayTime"]->set(preset.decayTime.AsSeconds());
	node["fadeTime"]->set(preset.fadeDecayTime.AsSeconds());
	node["pulse"]->set(preset.pulse);
	node["pulseTime"]->set(preset.pulseTime.AsSeconds());
	node["rotating"]->set(preset.rotating);
	node["rotationSpeed"]->set(preset.rotationSpeed);
	node["blur"]->set(preset.blur);
	node["blurIntensity"]->set(preset.blurIntensity);

	return node;
}