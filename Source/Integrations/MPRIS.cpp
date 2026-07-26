#include "MPRIS.hpp"

#include <cstring>

#include "../CApp.h"

MPRIS::MPRIS(CApp *app) : Integration(app) {
	// Initialize D-Bus error
	::dbus_error_init(&dbusError);
}

MPRIS::~MPRIS() {
}

void MPRIS::OnDestroy() {
	Disable();

	Integration::OnDestroy();
}

void MPRIS::OnLoop() {
	if (dbusConnection) {
		dbus_connection_read_write(dbusConnection, 0);

		if (auto *message = dbus_connection_pop_message(dbusConnection)) {
			if (dbus_message_is_method_call(message, "org.mpris.MediaPlayer2.Player", "PlayPause") ||
				dbus_message_is_method_call(message, "org.mpris.MediaPlayer2.Player", "Play") ||
				dbus_message_is_method_call(message, "org.mpris.MediaPlayer2.Player", "Pause")) {
				
				_app->TogglePlaying();

				auto *reply = dbus_message_new_method_return(message);
				dbus_connection_send(dbusConnection, reply, nullptr);
				dbus_message_unref(reply);
			} else if (dbus_message_is_method_call(message, "org.mpris.MediaPlayer2.Player", "Next")) {
				_app->NextTrack();

				auto *reply = dbus_message_new_method_return(message);
				dbus_connection_send(dbusConnection, reply, nullptr);
				dbus_message_unref(reply);
			} else if (dbus_message_is_method_call(message, "org.mpris.MediaPlayer2.Player", "Previous")) {
				_app->PreviousTrack();

				auto *reply = dbus_message_new_method_return(message);
				dbus_connection_send(dbusConnection, reply, nullptr);
				dbus_message_unref(reply);
			} else if (dbus_message_is_method_call(message, "org.mpris.MediaPlayer2.Player", "SetPosition")) {
				DBusMessageIter messageArgs{0};

				char *trackId = nullptr;
				int64_t position = 0;

				dbus_message_iter_init(message, &messageArgs);
				dbus_message_iter_get_arg_type(&messageArgs);
				dbus_message_iter_get_basic(&messageArgs, &trackId);
				dbus_message_iter_next(&messageArgs);
				dbus_message_iter_get_arg_type(&messageArgs);
				dbus_message_iter_get_basic(&messageArgs, &position);

				_app->SeekTo(Duration<Microseconds>(std::chrono::microseconds(position)).AsSeconds());

				auto *reply = dbus_message_new_method_return(message);
				dbus_connection_send(dbusConnection, reply, nullptr);
				dbus_message_unref(reply);
			} else {
				const auto *member = dbus_message_get_member(message);
				const auto *interface = dbus_message_get_interface(message);

				if (dbus_message_is_method_call(message, "org.freedesktop.DBus.Properties", "GetAll")) {
					DBusMessageIter messageArgs{0};

					char *interfaceName = nullptr;
					char *propertyName = nullptr;

					dbus_message_iter_init(message, &messageArgs);
					dbus_message_iter_get_arg_type(&messageArgs);
					dbus_message_iter_get_basic(&messageArgs, &interfaceName);

					DBusMessageIter args{0}, dictIter{0}, entryIter{0}, variantIter{0};

					auto *reply = dbus_message_new_method_return(message);
					dbus_message_iter_init_append(reply, &args);

					dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dictIter);

					if (!strcmp(interfaceName, "org.mpris.MediaPlayer2")) {
						AddBooleanProp(dictIter, entryIter, variantIter, "CanQuit", 0);
						AddBooleanProp(dictIter, entryIter, variantIter, "CanRaise", 0);
						AddBooleanProp(dictIter, entryIter, variantIter, "HasTrackList", 1);
						AddStringProp(dictIter, entryIter, variantIter, "Identity", "popRocks Visualizer");
						AddStringProp(dictIter, entryIter, variantIter, "DesktopEntry", "popRocks");
						std::vector<std::string> supportedUriSchemes = {"file"};
						AddStringArrayProp(dictIter, entryIter, variantIter, "SupportedUriSchemes", supportedUriSchemes);
						std::vector<std::string> supportedMimeTypes = {"audio/mpeg"};
						AddStringArrayProp(dictIter, entryIter, variantIter, "SupportedMimeTypes", supportedMimeTypes);
					} else if (!strcmp(interfaceName, "org.mpris.MediaPlayer2.Player")) {
						AddStringProp(dictIter, entryIter, variantIter, "PlaybackStatus", (hash ? (_app->IsPlaying() ? "Playing" : "Paused") : "Stopped"));
						AddStringProp(dictIter, entryIter, variantIter, "LoopStatus", "Playlist");
						AddDoubleProp(dictIter, entryIter, variantIter, "Rate", 1.0);
						AddBooleanProp(dictIter, entryIter, variantIter, "Shuffle", 0);
						AddDoubleProp(dictIter, entryIter, variantIter, "Volume", 1.0f);
						AddInt64Prop(dictIter, entryIter, variantIter, "Position", position);
						AddDoubleProp(dictIter, entryIter, variantIter, "MinimumRate", 1.0f);
						AddDoubleProp(dictIter, entryIter, variantIter, "MaximumRate", 1.0f);
						AddBooleanProp(dictIter, entryIter, variantIter, "CanGoNext", 1);
						AddBooleanProp(dictIter, entryIter, variantIter, "CanGoPrevious", 1);
						AddBooleanProp(dictIter, entryIter, variantIter, "CanPlay", 1);
						AddBooleanProp(dictIter, entryIter, variantIter, "CanPause", 1);
						AddBooleanProp(dictIter, entryIter, variantIter, "CanSeek", 1);
						AddBooleanProp(dictIter, entryIter, variantIter, "CanControl", 1);

						if (!hash) {
							std::map<std::string, std::any> metadata = {
								{"mpris:trackid", "/org/mpris/MediaPlayer2/TrackList/NoTrack"}
							};

							AddDictArrayProp(dictIter, entryIter, variantIter, "Metadata", metadata);
						} else {
							AddDictArrayProp(dictIter, entryIter, variantIter, "Metadata", GetMetadata());
						}
						
					}
					dbus_message_iter_close_container(&args, &dictIter);

					dbus_connection_send(dbusConnection, reply, nullptr);
					dbus_message_unref(reply);
				} else if (dbus_message_is_method_call(message, "org.freedesktop.DBus.Properties", "Get")) {
					DBusMessageIter messageArgs{0};

					char *interfaceName = nullptr;
					char *propertyName = nullptr;

					dbus_message_iter_init(message, &messageArgs);
					dbus_message_iter_get_arg_type(&messageArgs);
					dbus_message_iter_get_basic(&messageArgs, &interfaceName);
					dbus_message_iter_next(&messageArgs);
					dbus_message_iter_get_basic(&messageArgs, &propertyName);

					if (!strcmp(propertyName, "DesktopEntry")) {
						DBusMessageIter replyIter{0}, variantIter{0};

						const char *value = "popRocks";
						auto *reply = dbus_message_new_method_return(message);
						dbus_message_iter_init_append(reply, &replyIter);

						dbus_message_iter_open_container(&replyIter, DBUS_TYPE_VARIANT, "s", &variantIter);
						dbus_message_iter_append_basic(&variantIter, DBUS_TYPE_STRING, &value);
						dbus_message_iter_close_container(&replyIter, &variantIter);

						dbus_connection_send(dbusConnection, reply, nullptr);
						dbus_message_unref(reply);

						//AddStringProp(dictIter, entryIter, variantIter, "DesktopEntry", "popRocks");
					} else if (!strcmp(propertyName, "Position")) {
						DBusMessageIter replyIter{0}, variantIter{0};

						auto *reply = dbus_message_new_method_return(message);
						dbus_message_iter_init_append(reply, &replyIter);

						dbus_message_iter_open_container(&replyIter, DBUS_TYPE_VARIANT, "x", &variantIter);
						dbus_message_iter_append_basic(&variantIter, DBUS_TYPE_INT64, &position);
						dbus_message_iter_close_container(&replyIter, &variantIter);

						dbus_connection_send(dbusConnection, reply, nullptr);
						dbus_message_unref(reply);
					} else {
						LogWarning("requesting property ", propertyName, " on interface ", interfaceName);
					}
				} else if (dbus_message_is_signal(message, "org.freedesktop.DBus", "NameAcquired")) {
					// If we have a song playing, make sure to set it
					if (hash) {
						OnSongChanged(hash, title, artist, album, externalArt, length);
						SetPosition(position);
					}
				} else {
					LogWarning(
						"Got DBus message we don't understand!\n\tInterface = ",
						interface,
						"\n\tMember = ",
						member
					);
				}
			}

			dbus_message_unref(message);
		}
	}
}

bool MPRIS::IsEnabled() const {
	return dbusConnection;
}

void MPRIS::Enable() {
	// Connect to D-Bus
	if (!(dbusConnection = ::dbus_bus_get(DBUS_BUS_SESSION, &dbusError))) {
		LogError(dbusError.name);
		LogError(dbusError.message);
	} else if (dbus_bus_request_name(
		dbusConnection,
		"org.mpris.MediaPlayer2.popRocks",
		DBUS_NAME_FLAG_REPLACE_EXISTING,
		&dbusError
	) == -1) {
		LogError(dbusError.name);
		LogError(dbusError.message);
	} else {
		LogDebug("DBus connection established!");
	}
}

void MPRIS::Disable() {
	if (dbusConnection) {
		std::map<std::string, std::any> metadata = {
			{"mpris:trackid", "/org/mpris/MediaPlayer2/TrackList/NoTrack"}
		};

		PropertyChanged("Metadata", metadata);
		dbus_connection_flush(dbusConnection);

		dbus_bus_release_name(dbusConnection, "org.mpris.MediaPlayer2.popRocks", &dbusError);

		dbus_connection_flush(dbusConnection);
		dbus_connection_unref(dbusConnection);

		dbusConnection = nullptr;
	}
}

void MPRIS::OnSongChanged(
	const uint64_t &hash,
	const std::string &title,
	const std::string &artist,
	const std::string &album,
	const std::filesystem::path &externalArt,
	const int64_t length, // in microseconds
	bool hasArt
) {
	this->hash = hash;
	this->artist = artist;
	this->title = title;
	this->album = album;
	if (hasArt)
		this->externalArt = externalArt;
	this->length = length;

	PropertyChanged("Metadata", GetMetadata());
}

void MPRIS::OnPlay() {
	PropertyChanged("PlaybackStatus", "Playing");
}

void MPRIS::OnPause() {
	PropertyChanged("PlaybackStatus", "Paused");
}

void MPRIS::SetPosition(int64_t position) {
	Integration::SetPosition(position);

	// Only emit a signal if we're going to
	// or coming from 0
	if (lastPosition == 0 || position == 0) {
		PropertyChanged("Position", position);

		lastPosition = position;
	}
}

std::map<std::string, std::any> MPRIS::GetMetadata() {
	std::map<std::string, std::any> metadata = {
		{"mpris:trackid", "/com/fetcko/popRocks/track/" + std::to_string(hash)},
		{"mpris:length", length},
		{"xesam:title", title},
		{"xesam:artist", std::vector<std::string>{artist}},
		{"xesam:album", album}
	};

	if (!externalArt.empty()) {
		metadata.emplace(
			std::make_pair("mpris:artUrl", "file://" + externalArt.u8string())
		);
	}

	return metadata;
}

void MPRIS::AddPathProp(
	DBusMessageIter &dictIter,
	DBusMessageIter &entryIter,
	DBusMessageIter &variantIter,
	const char *name, const char *value
) {
	dbus_message_iter_open_container(&dictIter, DBUS_TYPE_DICT_ENTRY, nullptr, &entryIter);
	dbus_message_iter_append_basic(&entryIter, DBUS_TYPE_STRING, &name);

	dbus_message_iter_open_container(&entryIter, DBUS_TYPE_VARIANT, "o", &variantIter);
	dbus_message_iter_append_basic(&variantIter, DBUS_TYPE_OBJECT_PATH, &value);

	dbus_message_iter_close_container(&entryIter, &variantIter);
	dbus_message_iter_close_container(&dictIter, &entryIter);
}

void MPRIS::AddInt64Prop(
	DBusMessageIter &dictIter,
	DBusMessageIter &entryIter,
	DBusMessageIter &variantIter,
	const char *name, int64_t value
) {
	dbus_message_iter_open_container(&dictIter, DBUS_TYPE_DICT_ENTRY, nullptr, &entryIter);
	dbus_message_iter_append_basic(&entryIter, DBUS_TYPE_STRING, &name);

	dbus_message_iter_open_container(&entryIter, DBUS_TYPE_VARIANT, "x", &variantIter);
	dbus_message_iter_append_basic(&variantIter, DBUS_TYPE_INT64, &value);

	dbus_message_iter_close_container(&entryIter, &variantIter);
	dbus_message_iter_close_container(&dictIter, &entryIter);
}

void MPRIS::AddDoubleProp(
	DBusMessageIter &dictIter,
	DBusMessageIter &entryIter,
	DBusMessageIter &variantIter,
	const char *name, double value
) {
	dbus_message_iter_open_container(&dictIter, DBUS_TYPE_DICT_ENTRY, nullptr, &entryIter);
	dbus_message_iter_append_basic(&entryIter, DBUS_TYPE_STRING, &name);

	dbus_message_iter_open_container(&entryIter, DBUS_TYPE_VARIANT, "d", &variantIter);
	dbus_message_iter_append_basic(&variantIter, DBUS_TYPE_DOUBLE, &value);

	dbus_message_iter_close_container(&entryIter, &variantIter);
	dbus_message_iter_close_container(&dictIter, &entryIter);
}

void MPRIS::AddBooleanProp(
	DBusMessageIter &dictIter,
	DBusMessageIter &entryIter,
	DBusMessageIter &variantIter,
	const char *name, int value
) {
	dbus_message_iter_open_container(&dictIter, DBUS_TYPE_DICT_ENTRY, nullptr, &entryIter);
	dbus_message_iter_append_basic(&entryIter, DBUS_TYPE_STRING, &name);

	dbus_message_iter_open_container(&entryIter, DBUS_TYPE_VARIANT, "b", &variantIter);
	dbus_message_iter_append_basic(&variantIter, DBUS_TYPE_BOOLEAN, &value);

	dbus_message_iter_close_container(&entryIter, &variantIter);
	dbus_message_iter_close_container(&dictIter, &entryIter);
}

void MPRIS::AddStringProp(
	DBusMessageIter &dictIter,
	DBusMessageIter &entryIter,
	DBusMessageIter &variantIter,
	const char *name, const char *value
) {
	dbus_message_iter_open_container(&dictIter, DBUS_TYPE_DICT_ENTRY, nullptr, &entryIter);
	dbus_message_iter_append_basic(&entryIter, DBUS_TYPE_STRING, &name);

	dbus_message_iter_open_container(&entryIter, DBUS_TYPE_VARIANT, "s", &variantIter);
	dbus_message_iter_append_basic(&variantIter, DBUS_TYPE_STRING, &value);

	dbus_message_iter_close_container(&entryIter, &variantIter);
	dbus_message_iter_close_container(&dictIter, &entryIter);
}

void MPRIS::AddStringArrayProp(
	DBusMessageIter &dictIter,
	DBusMessageIter &entryIter,
	DBusMessageIter &variantIter,
	const char *name, const std::vector<std::string> &values
) {
	DBusMessageIter arrayIter{0};

	dbus_message_iter_open_container(&dictIter, DBUS_TYPE_DICT_ENTRY, nullptr, &entryIter);
	dbus_message_iter_append_basic(&entryIter, DBUS_TYPE_STRING, &name);

	dbus_message_iter_open_container(&entryIter, DBUS_TYPE_VARIANT, "as", &variantIter);
	dbus_message_iter_open_container(&variantIter, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING_AS_STRING, &arrayIter);
	for (const auto &value : values) {
		char *cStr = new char[value.length() + 1];
		memcpy(cStr, value.c_str(), value.length());
		cStr[value.length()] = '\0';
		dbus_message_iter_append_basic(&arrayIter, DBUS_TYPE_STRING, &cStr);
		delete[] cStr;
	}

	dbus_message_iter_close_container(&variantIter, &arrayIter);
	dbus_message_iter_close_container(&entryIter, &variantIter);
	dbus_message_iter_close_container(&dictIter, &entryIter);
}

void MPRIS::AddDictArrayProp(
	DBusMessageIter &dictIter,
	DBusMessageIter &entryIter,
	DBusMessageIter &variantIter,
	const char *name, const std::map<std::string, std::any> &values
) {
	DBusMessageIter arrayIter{0};
	dbus_message_iter_open_container(&dictIter, DBUS_TYPE_DICT_ENTRY, nullptr, &entryIter);
	dbus_message_iter_append_basic(&entryIter, DBUS_TYPE_STRING, &name);

	dbus_message_iter_open_container(&entryIter, DBUS_TYPE_VARIANT, "a{sv}", &variantIter);
	dbus_message_iter_open_container(&variantIter, DBUS_TYPE_ARRAY, "{sv}", &arrayIter);

	for (auto &[key, value] : values) {
		char *keyCStr = new char[key.length() + 1];
		memcpy(keyCStr, key.c_str(), key.length());
		keyCStr[key.length()] = '\0';

		if (const char * const *s = std::any_cast<const char *>(&value)) {
			DBusMessageIter entryIter2{0}, variantIter2{0};

			if ((*s)[0] == '/')
				AddPathProp(arrayIter, entryIter2, variantIter2, keyCStr, *s);
			else
				AddStringProp(arrayIter, entryIter2, variantIter2, keyCStr, *s);

		} else if (const std::string *s = std::any_cast<std::string>(&value)) {
			char *valueCStr = new char[s->length() + 1];
			memcpy(valueCStr, s->c_str(), s->length());
			valueCStr[s->length()] = '\0';

			DBusMessageIter entryIter2{0}, variantIter2{0};

			if (valueCStr[0] == '/')
				AddPathProp(arrayIter, entryIter2, variantIter2, keyCStr, valueCStr);
			else
				AddStringProp(arrayIter, entryIter2, variantIter2, keyCStr, valueCStr);

			delete[] valueCStr;
		} else if (const std::int64_t *x = std::any_cast<std::int64_t>(&value)) {
			DBusMessageIter entryIter2{0}, variantIter2{0};
			AddInt64Prop(arrayIter, entryIter2, variantIter2, keyCStr, *x);
		} else if (const std::vector<std::string> *as = std::any_cast<std::vector<std::string>>(&value)) {
			DBusMessageIter entryIter2{0}, variantIter2{0};
			AddStringArrayProp(arrayIter, entryIter2, variantIter2, keyCStr, *as);
		}

		delete[] keyCStr;
	}

	dbus_message_iter_close_container(&variantIter, &arrayIter);
	dbus_message_iter_close_container(&entryIter, &variantIter);
	dbus_message_iter_close_container(&dictIter, &entryIter);
}

void MPRIS::PropertyChanged(const std::string &name, std::any property) {
	if (dbusConnection) {
		auto *message = dbus_message_new_signal(
			"/org/mpris/MediaPlayer2",
			"org.freedesktop.DBus.Properties",
			"PropertiesChanged"
		);
		DBusMessageIter iter{0}, dictIter{0}, entryIter{0};

		dbus_message_iter_init_append(message, &iter);
		const char *interface = "org.mpris.MediaPlayer2.Player";
		dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &interface);
		dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dictIter);

		DBusMessageIter variantIter{0}, invalidateIter{0};

		if (name == "Metadata") {
			AddDictArrayProp(
				dictIter,
				entryIter,
				variantIter,
				name.c_str(),
				std::any_cast<std::map<std::string, std::any>>(property)
			);
		} else if (name == "Position") {
			AddInt64Prop(
				dictIter,
				entryIter,
				variantIter,
				name.c_str(),
				std::any_cast<int64_t>(property)
			);
		} else {
			AddStringProp(
				dictIter,
				entryIter,
				variantIter,
				name.c_str(),
				std::any_cast<const char *>(property)
			);
		}
		dbus_message_iter_close_container(&iter, &dictIter);

		dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "s", &invalidateIter);
		// TODO: ability to invalidate properties
		dbus_message_iter_close_container(&iter, &invalidateIter);

		dbus_connection_send(dbusConnection, message, nullptr);
		dbus_connection_flush(dbusConnection);
		dbus_message_unref(message);
	}
}