#pragma once

#include "Integration.hpp"
#include "../Settings.hpp"
#include "Utils/Logger.hpp"

#include <discordpp.h>

class CApp;
class Discord : public Integration, public Fetcko::LoggableClass {
public:
	Discord(CApp *app);

	bool IsEnabled() const override;

	void Enable() override;
	void Disable() override;

	void OnSongChanged(
		const uint64_t &hash,
		const std::string &title,
		const std::string &artist,
		const std::string &album,
		const std::filesystem::path &externalArt,
		const int64_t length, // in microseconds
		bool hasArt = true
	) override;

	void SetShowVisualizerName(bool showVisualizerName);
	void OnVisualizerChanged(const std::string &name) override;

	void SetPosition(int64_t position) override;

	void OnPlay() override;
	void OnPause() override;
private:
	inline void UpdateRichPresence();
	inline void UpdateVisualizerName();
	inline void UpdateAlbumName();
	inline void SetVisualizerName(const std::string &name);

	std::unique_ptr<discordpp::Client> client;

	discordpp::Activity activity;
	discordpp::ActivityTimestamps timestamps;
	discordpp::ActivityAssets assets;

	int64_t length = 0;

	std::chrono::system_clock::time_point updateTimer = std::chrono::system_clock::now();

	bool showVisualizerName = Settings::settings.GetShowVisualizerNameOnDiscord();

	bool firstChange = true;

	std::string title;
	std::string artist;
	std::string album;
};