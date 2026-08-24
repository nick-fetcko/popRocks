#include "Discord.hpp"

#include "../CApp.h"

#include "Utils/Italics.hpp"

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

	activity.SetName("popRocks");
	activity.SetType(discordpp::ActivityTypes::Listening);
	activity.SetDetails("Selecting an album...");

	UpdateRichPresence();
}
void Discord::Disable() {
	client->Disconnect();
	client.reset();
}

void Discord::SetShowVisualizerName(bool showVisualizerName) {
	this->showVisualizerName = showVisualizerName;

	if (showVisualizerName)
		UpdateVisualizerName(true);
	else {
		activity.SetState("on the album " + Italics::ToItalics(album));
		UpdateRichPresence();
	}
}

inline void Discord::UpdateVisualizerName(bool andSet) {
	auto index =
		Settings::settings.GetMiniPlayer() ?
		Settings::settings.GetMiniPlayerPresetIndex() :
		Settings::settings.GetPresetIndex();

	SetVisualizerName(index ? Preset::GetPresets().at(*index).GetName() : "", andSet);
}

inline void Discord::SetVisualizerName(const std::string &name, bool andSet) {
	if (!showVisualizerName) return;

	if (name.empty())
		activity.SetState("Creating new visualizer...");
	else
		activity.SetState("Visualizing with " + Italics::ToItalics(name));

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
	this->album = album;

	activity = discordpp::Activity();

	activity.SetName("popRocks");
	activity.SetType(discordpp::ActivityTypes::Listening);
	activity.SetDetails("\"" + title + "\" by " + artist);

	if (!showVisualizerName)
		activity.SetState("on the album " + Italics::ToItalics(album));
	else UpdateVisualizerName(false);

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
	SetVisualizerName(name);
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
