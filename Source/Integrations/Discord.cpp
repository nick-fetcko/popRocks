#include "Discord.hpp"

#include "../CApp.h"

#define DISCORDPP_IMPLEMENTATION
#include "discordpp.h"

Discord::Discord(CApp *app) : Integration(app) {

}

bool Discord::IsEnabled() const {
	return true;
}

void Discord::Enable() {
	firstChange = true;

	client = std::make_unique<discordpp::Client>();
	client->SetApplicationId(1540473579617915010);

	client->AddLogCallback([this](auto message, auto severity) {
		switch (severity) {
		case discordpp::LoggingSeverity::Verbose:
			LogInfo(message);
			break;
		case discordpp::LoggingSeverity::Info:
			LogDebug(message);
			break;
		case discordpp::LoggingSeverity::Warning:
			LogWarning(message);
			break;
		default:
			LogError(message);
			break;
		}
	}, discordpp::LoggingSeverity::Info);

	client->SetStatusChangedCallback([this](discordpp::Client::Status status, discordpp::Client::Error error, int32_t errorDetail) {
		LogDebug(discordpp::Client::StatusToString(status));
	});
}
void Discord::Disable() {
	client->Disconnect();
	client.reset();
}

inline void Discord::UpdateVisualizerName(const std::string &name, bool andSet) {
	if (name.empty())
		activity.SetState("Creating new visualizer...");
	else
		activity.SetState(u8"Visualizing with \uFF62" + name + u8"\uFF63");

	if (andSet) UpdateRichPresence();
}

inline void Discord::UpdateRichPresence() {
	client->UpdateRichPresence(activity, [this](discordpp::ClientResult result) {
		if (result.Successful())
			LogInfo("Rich presence updated!");
		else LogError("Rich presence not updated! ", result.Error());
	});
}

void Discord::OnSongChanged(
	const uint64_t &hash,
	const std::string &title,
	const std::string &artist,
	const std::string &album,
	const std::filesystem::path &externalArt,
	const int64_t length, // in microseconds
	bool hasArt
) {
	activity = discordpp::Activity();

	activity.SetName("popRocks");
	activity.SetType(discordpp::ActivityTypes::Listening);
	activity.SetDetails("\"" + title + "\" by " + artist);

	auto index = 
		Settings::settings.GetMiniPlayer() ?
			Settings::settings.GetMiniPlayerPresetIndex() :
			Settings::settings.GetPresetIndex();
	
	UpdateVisualizerName(index ? Preset::GetPresets().at(*index).GetName() : "", false);

	this->length = length / 1000000 /* Discord wants the time in SECONDS */;

	if (firstChange)
		firstChange = false;
	else
		position = 0;

	const auto start = std::time(nullptr) - position / 1000000;

	timestamps.SetStart(start);
	timestamps.SetEnd(start + this->length);

	activity.SetTimestamps(timestamps);

	positionTimer = std::chrono::system_clock::now();

	discordpp::ActivityButton buyButton;
	buyButton.SetLabel("Get popRocks");
	buyButton.SetUrl("https://popRocks.app");

	activity.AddButton(buyButton);

	assets.SetLargeImage("poprocks-icon-main");
	activity.SetAssets(assets);

	UpdateRichPresence();
}

void Discord::OnVisualizerChanged(const std::string &name) {
	UpdateVisualizerName(name);
}

void Discord::SetPosition(int64_t position) {
	// Discord appears to double-check these values once every ~20 seconds
	if (const auto now = std::chrono::system_clock::now(); now - positionTimer >= 20s) {
		const auto start = std::time(nullptr) - position / 1000000;

		timestamps.SetStart(start);
		timestamps.SetEnd(start + this->length);

		activity.SetTimestamps(timestamps);

		UpdateRichPresence();

		positionTimer = now;
	}

	discordpp::RunCallbacks();
	
	Integration::SetPosition(position);
}

void Discord::OnPlay() {
	assets.SetSmallImage("play");
	activity.SetAssets(assets);
	UpdateRichPresence();
}
void Discord::OnPause() {
	assets.SetSmallImage("pause");
	activity.SetAssets(assets);
	UpdateRichPresence();
}
