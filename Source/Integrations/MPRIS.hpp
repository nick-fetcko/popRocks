#pragma once

#include <any>
#include <map>
#include <string>
#include <vector>

#include "Integration.hpp"

#include "Utils/Logger.hpp"

#include <dbus/dbus.h>

class CApp;
class MPRIS : public Integration, public Fetcko::LoggableClass {
public:
	MPRIS(CApp *app);
	~MPRIS() override;

	void OnDestroy() override;

	void OnLoop();

	bool IsEnabled() const override;

	void Enable() override;
	void Disable() override;

	void OnSongChanged(
		const uint64_t &hash,
		const std::string &title,
		const std::string &artist,
		const std::string &album,
		const std::filesystem::path &externalArt,
		const int64_t length, // in microseconds,
		bool hasArt = true
	) override;

	void OnVisualizerChanged(const std::string &name) override;

	void OnPlay() override;
	void OnPause() override;

	void SetPosition(int64_t position) override;

private:
	std::map<std::string, std::any> GetMetadata();

	void AddPathProp(
		DBusMessageIter &dictIter,
		DBusMessageIter &entryIter,
		DBusMessageIter &variantIter,
		const char *name, const char *value
	);
	void AddInt64Prop(
		DBusMessageIter &dictIter,
		DBusMessageIter &entryIter,
		DBusMessageIter &variantIter,
		const char *name, int64_t value
	);
	void AddDoubleProp(
		DBusMessageIter &dictIter,
		DBusMessageIter &entryIter,
		DBusMessageIter &variantIter,
		const char *name, double value
	);
	void AddBooleanProp(
		DBusMessageIter &dictIter,
		DBusMessageIter &entryIter,
		DBusMessageIter &variantIter,
		const char *name, int value
	);
	void AddStringProp(
		DBusMessageIter &dictIter,
		DBusMessageIter &entryIter,
		DBusMessageIter &variantIter,
		const char *name, const char *value
	);
	void AddStringArrayProp(
		DBusMessageIter &dictIter,
		DBusMessageIter &entryIter,
		DBusMessageIter &variantIter,
		const char *name, const std::vector<std::string> &values
	);
	void AddDictArrayProp(
		DBusMessageIter &dictIter,
		DBusMessageIter &entryIter,
		DBusMessageIter &variantIter,
		const char *name, const std::map<std::string, std::any> &values
	);
	void PropertyChanged(
		const std::string &name,
		std::any property
	);

	DBusError dbusError;
	DBusConnection *dbusConnection = nullptr;

	int64_t lastPosition = 0;

	// Cache
	uint64_t hash = 0;
	std::string title;
	std::string artist;
	std::string album;
	std::filesystem::path externalArt;
	int64_t length = 0; // in microseconds
};
