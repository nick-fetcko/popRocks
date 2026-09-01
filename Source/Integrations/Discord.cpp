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

	if (!_app->IsFileLoaded() && !_app->IsPreLoaded()) {
		activity.SetDetails("Selecting an album...");

		UpdateRichPresence();
	} else {
		// Queue up an immediate update
		updateTimer = std::chrono::system_clock::now() - 15s;
	}
}
void Discord::Disable() {
	client->Disconnect();
	client.reset();
}

void Discord::SetShowVisualizerName(bool showVisualizerName) {
	this->showVisualizerName = showVisualizerName;

	if (showVisualizerName)
		UpdateVisualizerName();
	else
		UpdateAlbumName();
}

inline void Discord::UpdateVisualizerName() {
	auto index =
		Settings::settings.GetMiniPlayer() ?
		Settings::settings.GetMiniPlayerPresetIndex() :
		Settings::settings.GetPresetIndex();

	SetVisualizerName(index ? Preset::GetPresets().at(*index).GetName() : "");
}

inline void Discord::UpdateAlbumName() {
	if (album.size()) {
		const auto state = "on the album " + Italics::ToItalics(album);

		activity.SetState(Utils::Truncate(state, 128, true));
	} else {
		activity.SetDetails(Utils::Truncate(title, 128, true));
		activity.SetState(Utils::Truncate(artist, 128, true));
	}
}

inline void Discord::SetVisualizerName(const std::string &name) {
	if (!showVisualizerName) return;

	if (name.empty())
		activity.SetState("Creating new visualizer...");
	else
		activity.SetState("Visualizing with " + Italics::ToItalics(name));
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
	this->title = title;
	this->artist = artist;
	this->album = album;

	activity = discordpp::Activity();

	activity.SetName("popRocks");
	activity.SetType(discordpp::ActivityTypes::Listening);

	const auto details = "\"" + title + "\" by " + artist;

	activity.SetDetails(Utils::Truncate(details, 128, true));

	if (!showVisualizerName)
		UpdateAlbumName();
	else
		UpdateVisualizerName();

	this->length = length / 1000000 /* Discord wants the time in SECONDS */;

	if (firstChange)
		firstChange = false;
	else
		position = 0;

	const auto start = std::time(nullptr) - position / 1000000;

	timestamps.SetStart(start);
	timestamps.SetEnd(start + this->length);

	activity.SetTimestamps(timestamps);

	discordpp::ActivityButton buyButton;
	buyButton.SetLabel("Get popRocks");
	buyButton.SetUrl("https://popRocks.app");

	activity.AddButton(buyButton);

	assets.SetLargeImage("poprocks-icon-main");
	activity.SetAssets(assets);
}

void Discord::OnVisualizerChanged(const std::string &name) {
	SetVisualizerName(name);
}

void Discord::SetPosition(int64_t position) {
	// Discord throttles to one update per ~15 seconds
	if (const auto now = std::chrono::system_clock::now(); now - updateTimer >= 15s) {
		const auto start = std::time(nullptr) - position / 1000000;

		timestamps.SetStart(start);
		timestamps.SetEnd(start + this->length);

		activity.SetTimestamps(timestamps);

		UpdateRichPresence();

		updateTimer = now;
	}

	discordpp::RunCallbacks();
	
	Integration::SetPosition(position);
}

void Discord::OnPlay() {
	assets.SetSmallImage("play");
	activity.SetAssets(assets);
}
void Discord::OnPause() {
	assets.SetSmallImage("pause");
	activity.SetAssets(assets);
}
