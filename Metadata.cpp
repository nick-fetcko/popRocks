#include "Metadata.hpp"

void Metadata::OnLoad(
	const std::filesystem::path &path,
	const std::string &extension,
	HSTREAM streamHandle,
	TagLoader *tagLoader,
	AlbumArt *albumArt
) {
	// Prefer ID3v2, since it doesn't have a character limit
	auto id3v2 = BASS_ChannelGetTags(streamHandle, BASS_TAG_ID3V2);
	if (id3v2) {
		ID3V2 id3;

		auto tags = id3.Read(&id3v2, albumArt == nullptr);
		tagLoader->LoadFromTags(tags);

		auto &frames = id3.GetFrames();
		if (auto art = frames.find("art"); art != frames.end() && art->second.artData) {
			albumArt->Load(
				art->second.artData->mimeType,
				art->second.artData->data,
				art->second.artData->dataLength
			);
		}
	}

	// If we're missing tags from ID3v2, try to load them from v1
	if (tagLoader->AreThereEmptyTags()) {
		auto id3 = reinterpret_cast<const TAG_ID3 *>(BASS_ChannelGetTags(streamHandle, BASS_TAG_ID3));
		if (id3) {
			tagLoader->LoadFromID3v1(id3);
		} else {
			auto vorbis = BASS_ChannelGetTags(streamHandle, BASS_TAG_OGG);
			if (vorbis) {
				auto tags = GetTags(vorbis);

				tagLoader->LoadFromTags(tags);
			} else {
				auto mp4 = BASS_ChannelGetTags(streamHandle, BASS_TAG_MP4);
				if (mp4) {
					auto tags = GetTags(mp4);

					tagLoader->LoadFromTags(tags);
				} else {
					CConsole::Console.Print("Could not fully populate tags!", MSG_ERROR);
				}
			}
		}
	}

	// If title is STILL empty, use the filename
	if (!tagLoader->HasTitle()) {
		CConsole::Console.Print("Using filename in lieu of title", MSG_ALERT);
		tagLoader->SetTitle(path.stem().u8string());
	}

	// Load embedded album art
	if (albumArt) {
		if (extension == ".flac") {
			auto art = reinterpret_cast<const TAG_FLAC_PICTURE *>(BASS_ChannelGetTags(streamHandle, BASS_TAG_FLAC_PICTURE));
			if (art) {
				albumArt->Load(
					art->mime,
					const_cast<void *>(art->data),
					art->length
				);
			}
		} else if (extension == ".mp4" || extension == ".m4a") {
			MP4 mp4(path);

			// For now, we just want to grab "iTunes style" album art
			auto atom = mp4.GetAtomAtPath({ "moov", "udta", "meta", "ilst", "covr", "data" });

			if (atom) {
				atom->ReadData();
				if (albumArt->Load(atom->mimeType, atom->data, atom->dataSize))
					CConsole::Console.Print("Found iTunes-style embedded album art", MSG_DIAG);
			}
		}
	}
}

std::map<std::string, std::string> Metadata::GetTags(const char *tags) const {
	std::map<std::string, std::string> ret;
	std::pair<std::string, std::string> tag;
	auto *filling = &tag.first;

	for (std::size_t i = 0; /* we'll break when we find a double null */; ++i) {
		if (tags[i] == '=') {
			// convert our tag to lowercase (just in case (pun intended))
			std::transform(filling->begin(), filling->end(), filling->begin(), tolower);

			filling = &tag.second;
			continue;
		}

		(*filling) += tags[i];

		if (tags[i] == '\0') {
			if (i > 0 && tags[i - 1] == '\0')
				break;

			ret.emplace(std::move(tag));
			tag = std::pair<std::string, std::string>();
			filling = &tag.first;
		}
	}

	return ret;
}