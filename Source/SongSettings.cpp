#include "SongSettings.hpp"

#include <fstream>

#include "Utils/Filesystem.hpp"

using namespace Fetcko;

void SongSettings::Load() {
	if (const auto path = Filesystem::GetPath("SongSettings.json"); !path.empty() && std::filesystem::exists(path)) {
		std::ifstream inFile(path);

		Node json;
		json.parseStream<Json>(inFile);

		json >> settings;
	}
}

void SongSettings::SetPreferredAlbumArt(const std::filesystem::path &path, const std::string &art) {
	settings[path.generic_u8string()] = SongSetting{art};

	Save();
}

const bool SongSettings::HasSettings(const std::filesystem::path &path) const {
	return settings.find(path.generic_u8string()) != settings.end();
}

SongSettings::SongSetting *SongSettings::GetSettings(const std::filesystem::path &path) {
	const auto iter = settings.find(path.generic_u8string());

	if (iter == settings.end()) return nullptr;
	else return &(iter->second);
}

void SongSettings::RemoveSetting(const std::filesystem::path &path) {
	const auto iter = settings.find(path.generic_u8string());

	if (iter != settings.end())
		settings.erase(iter);

	Save();
}

void SongSettings::Save() {
	if (const auto path = Filesystem::GetPath("SongSettings.json"); !path.empty()) {
		std::ofstream outFile(path);

		Node json;
		json << settings;

		json.writeStream<Json>(outFile, NodeFormat::Beautified);
	}
}

const Node &operator>>(const Node &node, SongSettings::SongSetting &setting) {
	node["albumArt"]->get(setting.albumArt);

	return node;
}
Node &operator<<(Node &node, const SongSettings::SongSetting &settings) {
	node["albumArt"]->set(settings.albumArt);

	return node;
}
