#include "Metadata.hpp"

#include "APE.hpp"
#include "OGG.hpp"
#include "WV.hpp"

void Metadata::OnLoad(
	const std::filesystem::path &path,
	const std::string &extension,
	HSTREAM streamHandle,
	TagLoader *tagLoader,
	AlbumArt *albumArt
) {
	if (extension == ".flac") {
		FLAC flac(path);

		auto tags = flac.GetTags(false);
		tagLoader->LoadFromTags(tags);
		if (const auto &[mimeType, data] = flac.GetArt(); data.size()) {
			albumArt->Load(
				mimeType,
				data.data(),
				data.size()
			);
		} else albumArt->ClearEmbedded();

		return;
	}
	else if (extension == ".mp4" || extension == ".m4a") {
		MP4 mp4(path);

		auto tags = mp4.GetTags(false);
		tagLoader->LoadFromTags(tags);
		if (auto &art = mp4.GetArt(); art && albumArt->Load(art->mimeType, art->data, art->dataSize))
			LogDebug("Found iTunes-style embedded album art");

		return;
	} else if (extension == ".ape" || extension == ".tta" /* TTA uses APE tags */) {
		APE ape(path);

		auto tags = ape.GetTags(false);
		tagLoader->LoadFromTags(tags);
		if (auto art = ape.GetItems().find("art"); art != ape.GetItems().end()) {
			albumArt->Load(
				art->second.mimeType,
				art->second.data,
				art->second.size
			);
		} else albumArt->ClearEmbedded();

		// TTA files can use ID3 tags, too
		if (extension == ".tta" && tagLoader->AreThereEmptyTags()) {
			MP3 tta(path);

			tagLoader->LoadFromTags(tta.GetTags());
		}
		
		return;
	} else if (extension == ".wv") {
		WV wv(path);

		auto tags = wv.GetTags(false);
		tagLoader->LoadFromTags(tags);
		if (auto art = wv.GetItems().find("art"); art != wv.GetItems().end()) {
			albumArt->Load(
				art->second.mimeType,
				art->second.data,
				art->second.size
			);
		} else albumArt->ClearEmbedded();

		return;
	} else if (extension == ".ogg") {
		OGG ogg(path);

		auto tags = ogg.GetTags(false);
		tagLoader->LoadFromTags(tags);
		if (auto art = tags.find("art"); art != tags.end()) {
			auto [mimeType, data] = ogg.GetArt(art->second);

			albumArt->Load(
				mimeType,
				data.data(),
				data.size()
			);
		} else albumArt->ClearEmbedded();

		return;
	}

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
				LogError("Could not fully populate tags!");
			}
		}
	}

	// If title is STILL empty, use the filename
	if (!tagLoader->HasTitle()) {
		LogWarning("Using filename in lieu of title");
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