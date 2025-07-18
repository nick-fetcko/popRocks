#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

#include "Buffer.hpp"
#include "Cue.hpp"
#include "Metadata.hpp"
#include "TagLoader.hpp"
#include "Text.hpp"
#include "Utils.hpp"

class Playlist {
public:
	struct Track {
		std::filesystem::path path;
		double startTime = 0.0;
	};

	std::optional<Track> OnLoad(
		const std::filesystem::path &path,
		const std::string_view &extension,
		std::function<HSTREAM(const std::filesystem::path &, const std::string &, DWORD)> openWithFlags
	);

	void OnInit(int windowWidth, int windowHeight, TTF_Font *font, float scale = 1.0f);
	void OnResize(int windowWidth, int windowHeight, float scale = 1.0f);

	void Clear();

	std::optional<Track> Current();
	std::optional<Track> Previous();
	std::optional<Track> Next();

	// FIXME: you pretty much NEED to const_cast
	//        to get this overload, so it's not
	//        that intuitive
	const std::optional<Track> Next() const;

	void OnLoop(Vector2i pos, float maxHeight, float alpha);

	std::optional<Track> OnMouseClicked(const Vector2i &mousePos);

	const std::unique_ptr<Cue> &GetCue() const;
	const std::filesystem::path &GetPath() const;

	static constexpr bool IsCue(const std::string_view &lowercaseExtension) {
		return lowercaseExtension == ".cue";
	}

	static constexpr auto &GetSupportedExtensions() {
		return SupportedExtensions;
	}

private:
	constexpr inline static std::array<std::string_view, 9> SupportedExtensions = { ".flac", ".mp3", ".m4a", ".mp4", ".ape", ".wv", ".ogg", ".aac", ".wav"};

	static constexpr bool IsSupported(const std::string_view &lowercaseExtension) {
		for (const auto &extension : SupportedExtensions)
			if (lowercaseExtension == extension)
				return true;

		return false;
	}

	struct Title {
		std::size_t disc;
		std::size_t index;
		std::string title;

		friend std::ostream &operator<<(std::ostream &left, const Title &right) {
			left << right.title;
			return left;
		}
	};

	std::optional<std::filesystem::path> FindCue(const std::filesystem::path &path);

	inline void UpdateSize();

	// First key is disc #
	// Second key is track #
	using Sorter = std::map<std::size_t, std::map<std::size_t, std::pair<std::string, std::filesystem::path>>>;
	Sorter::iterator GuessDisc(Sorter &sorter, std::size_t index);

	template <typename T>
	void LoadTitles(const std::vector<T> &titles) {
		if (titles.empty()) return;

		size = { 0, 0 };

		std::size_t numberOfDiscs = 1;
		std::size_t maxTracksPerDisc = 1;
		for (const auto &title : titles) {
			if (title.disc > numberOfDiscs)
				numberOfDiscs = title.disc;
			if (title.index > maxTracksPerDisc)
				maxTracksPerDisc = title.index;
		}

		auto digits = Fetcko::Utils::GetNumberOfDigits(maxTracksPerDisc);
		for (const auto &title : titles) {
			std::stringstream stream;

			// If we have multiple discs,
			// also display disc number
			if (numberOfDiscs > 1) {
				stream
					<< title.disc
					<< "-";
			}
				
			stream
				<< std::setfill('0')
				<< std::setw(digits) 
				// Cue sheets use a uint8_t for index,
				// so we have to explicitly cast it to
				// a non-character type
				<< static_cast<int>(title.index) 
				<< " - " 
				<< title;

			Text text;
			text.OnInit(font);
			text.SetText(stream.str());

			size.y += text.GetSize().y;
			if (text.GetSize().x > size.x)
				size.x = text.GetSize().x;

			this->titles.emplace_back(std::move(text));
		}

		UpdateSize();
	}

	template<typename T>
	void OnLoop(
		const std::vector<T> &tracks,
		typename const std::vector<T>::const_iterator &current,
		Vector2i pos, // need a local pos var because we modify it
		float maxHeight,
		float alpha
	) {
		// Limit our background rectangle to our
		// max height
		if (rect[3] > maxHeight + em.y / 2 - pos.y)
			rect[3] = rect[5] = maxHeight + em.y / 2 - pos.y;

		glTranslatef(static_cast<GLfloat>(pos.x), static_cast<GLfloat>(pos.y), 0.0f);

		glVertexPointer(2, GL_FLOAT, 0, rect);

		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);

		rectColors[3] = rectColors[15] = alpha;

		glColorPointer(4, GL_FLOAT, 0, rectColors);

		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, Buffer::SquareBuffer.data());
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);

		glLoadIdentity();

		std::size_t maxIndex = static_cast<std::size_t>(
			titles.empty() ?
				0 :
				(maxHeight - pos.y) / titles.begin()->GetSize().y
		);

		auto distance = static_cast<int32_t>(
			std::distance(tracks.begin(), current)
		);
		pos.y -= titles.begin()->GetSize().y * distance;

		auto iter = tracks.begin();
		for (const auto &[i, title] : Fetcko::Utils::Enumerate(titles)) {
			if (pos.y + title.GetSize().y > maxHeight)
				return;

			if (iter == current)
				glColor4f(1.0f, 1.0f, 1.0f, alpha);
			else
				glColor4f(0.6f, 0.6f, 0.6f, alpha - (static_cast<float>(i - distance) / maxIndex));

			title.OnLoop(pos.x, pos.y);

			// Reduce the height as we near the end of the playlist
			if (i == titles.size() - 2 && pos.y < maxHeight)
				rect[3] = rect[5] = pos.y + em.y / 2.0f;

			pos.y += title.GetSize().y;

			++iter;
		}
	}

	std::filesystem::path path;
	std::vector<std::filesystem::path> files;
	std::vector<std::filesystem::path>::iterator currentFile = files.end();

	int windowWidth = 0, windowHeight = 0;
	TTF_Font *font = nullptr;
	std::vector<Text> titles;

	Vector2i pos{ 0, 0 };
	Vector2i size{ 0, 0 };
	Vector2f em{ 0.0f, 0.0f };

	std::unique_ptr<Cue> cue;

	float rect[8] = { 0.0f };
	float rectColors[16] = {
		0.0f, 0.0f, 0.0f, 0.75f,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.75f
	};

	float scale = 1.0f;
};