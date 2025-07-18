#include "Preset.hpp"

#include <fstream>

#include "CConsole.h"

const std::vector<Preset> Preset::Presets = Preset::Load();

std::vector<Preset> Preset::Load() {
	std::vector<Preset> ret;

	std::ifstream inFile("../../Data/Presets.json");

	try {
		Node json;
		json.parseStream<Json>(inFile);

		for (auto &&node : json.get<std::map<std::string, Preset>>()) {
			node.second.name = node.first;

			ret.emplace_back(std::move(node.second));
		}
	} catch (std::exception &e) {
		CConsole::Console.Print(std::string("Could not parse presets: ") + e.what(), MSG_ERROR);
	}

	return ret;
}

const Node &operator>>(const Node &node, Preset &preset) {
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

	return node;
}