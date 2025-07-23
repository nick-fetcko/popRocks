#pragma once

#include <map>
#include <string>

#include <bass.h>

class TagLoader {
public:
	virtual ~TagLoader() = default;

	virtual void LoadFromTags(const std::map<std::string, std::string> &tags) = 0;
	virtual bool AreThereEmptyTags() const = 0;
	virtual bool HasTitle() const = 0;

	virtual void LoadFromID3v1(const TAG_ID3 *id3) = 0;

	virtual void SetTitle(const std::string &title) = 0;

protected:
};