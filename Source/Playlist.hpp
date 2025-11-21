#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

#include "Utils/Logger.hpp"
#include "Utils/Utils.hpp"

#include "Buffer.hpp"
#include "Cue.hpp"
#include "Hash.hpp"
#include "HDR.hpp"
#include "Metadata.hpp"
#include "Settings.hpp"
#include "TagLoader.hpp"
#include "Text.hpp"

using namespace Fetcko;

class Playlist : public LoggableClass {
public:
	struct Track {
		std::filesystem::path path;
		std::string title;
		double startTime = 0.0;
	};

	std::optional<Track> OnLoad(
		const std::filesystem::path &path,
		const std::string_view &extension,
		std::function<HSTREAM(const std::filesystem::path &, const std::string &, DWORD)> openWithFlags
	);

	void OnInit(int windowWidth, int windowHeight, OpenGLFont *font, OpenGLFont *outlineFont, Context *context, float scale = 1.0f);
	void OnResize(int windowWidth, int windowHeight, float scale = 1.0f);
	void OnDestroy();

	void Clear();

	std::optional<Track> Current();
	std::optional<Track> Previous();
	std::optional<Track> Next();

	const std::optional<Track> GetNext() const;

	void OnLoop(Vector2i pos, float maxHeight, float alpha, Context &context);

	std::optional<Track> OnMouseClicked(const Vector2i &mousePos);

	void SetCurrentSongVisible(bool currentSongVisible) { 
		this->currentSongVisible = currentSongVisible;
		Settings::settings.SetCurrentSongVisible(currentSongVisible);
	}

	const std::unique_ptr<Cue> &GetCue() const;
	const std::filesystem::path &GetPath() const;

	static constexpr bool IsCue(const std::string_view &lowercaseExtension) {
		return lowercaseExtension == ".cue";
	}

	static constexpr auto &GetSupportedExtensions() {
		return SupportedExtensions;
	}

private:
	constexpr inline static std::array<std::string_view, 10> SupportedExtensions = { ".flac", ".mp3", ".m4a", ".mp4", ".ape", ".wv", ".ogg", ".aac", ".tta", ".wav"};

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

	std::vector<std::filesystem::path> FindCue(const std::filesystem::path &path);

	inline void UpdateSize();

	// First key is disc #
	// Second key is track #
	using Sorter = std::map<std::size_t, std::map<std::size_t, std::pair<std::string, std::filesystem::path>>>;
	Sorter::iterator GuessDisc(Sorter &sorter, std::optional<std::size_t> &index);

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
					<< static_cast<std::size_t>(title.disc)
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
			text.OnInit(font, context);
			text.SetText(stream.str());

			size.y += text.GetBounds().height;
			if (text.GetSize().x > size.x)
				size.x = text.GetSize().x;

			this->titles.emplace_back(std::move(text));
		}

		UpdateSize();
	}

	template<typename T>
	void OnLoop(
		const std::vector<T> &tracks,
		const typename std::vector<T>::const_iterator &current,
		Vector2i pos, // need a local pos var because we modify it
		float maxHeight,
		float alpha,
		Context &context
	) {
		// Limit our background rectangle to our
		// max height
		if (auto height = maxHeight + font->GetEm().height / 2 - pos.y; this->height > height) {
			this->height = height;

			const auto heightMinusOne = height - font->GetEm().height * 2;

			vbo->Bind();

			// Bottom of top half
			vbo->BufferSubData(7, sizeof(float), &heightMinusOne);
			vbo->BufferSubData(13, sizeof(float), &heightMinusOne);

			// Top of bottom half
			vbo->BufferSubData(25, sizeof(float), &heightMinusOne);
			vbo->BufferSubData(43, sizeof(float), &heightMinusOne);

			// Bottom of bottom half
			vbo->BufferSubData(31, sizeof(float), &height);
			vbo->BufferSubData(37, sizeof(float), &height);

			vbo->Unbind();
		}

		if (this->maxHeight != maxHeight)
			this->maxHeight = maxHeight;

		context.Use("color"_hash);
		context.Translate(static_cast<GLfloat>(pos.x), static_cast<GLfloat>(pos.y), 0.0f);
		context.Apply();

		vbo->Bind();

		// Top of top half
		vbo->BufferSubData(5, sizeof(float), &alpha);
		vbo->BufferSubData(23, sizeof(float), &alpha);

		auto fadeOut = alpha;

		if (HDR::Enabled) {
			fadeOut *= 0.5f;
		} else {
			fadeOut = 0.0f;
		}

		const auto zero = 0.0f;
		
		// Bottom of top half
		vbo->BufferSubData(11, sizeof(float), &fadeOut);
		vbo->BufferSubData(17, sizeof(float), &fadeOut);

		// Top of bottom half
		vbo->BufferSubData(29, sizeof(float), &fadeOut);
		vbo->BufferSubData(47, sizeof(float), &fadeOut);

		// Bottom of bottom half
		vbo->BufferSubData(35, sizeof(float), &zero);
		vbo->BufferSubData(41, sizeof(float), &zero);

		vbo->Unbind();

		vao->Bind();
		eab->Bind();
		eab->DrawElements(GL_TRIANGLES);
		eab->Unbind();
		vao->Unbind();

		context.Use("texture"_hash);
		context.LoadIdentity();

		// Add a line for our previous track
		if (current != tracks.begin())
			pos.y += font->GetEm().height;

		std::size_t maxIndex = static_cast<std::size_t>(
			titles.empty() ?
				0 :
				(maxHeight - pos.y) / titles.begin()->GetBounds().height
		);

		auto distance = static_cast<int32_t>(
			std::distance(tracks.begin(), current)
		);
		pos.y -= titles.begin()->GetBounds().height * distance;

		auto iter = tracks.begin();
		for (const auto &[i, title] : Fetcko::Utils::Enumerate(titles)) {
			if (pos.y + title.GetBounds().height > maxHeight)
				return;

			if (iter == current) {
				if (currentSongVisible) {
					outline.SetText(title.GetText());
					context.Color(0.0f, 0.0f, 0.0f, std::max(0.5f, alpha));
					outline.OnLoop(pos.x, pos.y);
					context.Color(
						HDR::WhiteLevel,
						HDR::WhiteLevel,
						HDR::WhiteLevel,
						std::max(0.5f, alpha)
					);
				} else context.Color(HDR::WhiteLevel, HDR::WhiteLevel, HDR::WhiteLevel, alpha);
			} else if (current != tracks.begin() && iter == current - 1) {
				context.Color(
					0.6f * HDR::WhiteLevel,
					0.6f * HDR::WhiteLevel,
					0.6f * HDR::WhiteLevel,
					0.4f * alpha
				);
			} else {
				context.Color(
					0.6f * HDR::WhiteLevel,
					0.6f * HDR::WhiteLevel,
					0.6f * HDR::WhiteLevel,
					std::max(
						0.0f,
						alpha - (static_cast<float>(i - distance) / maxIndex)
					)
				);
			}

			title.OnLoop(pos.x, pos.y);

			// Reduce the height as we near the end of the playlist
			if (i == titles.size() - 2 && pos.y < maxHeight) {
				height = pos.y + font->GetEm().height / 2.0f;

				const auto heightMinusOne = height - font->GetEm().height * 2;

				vbo->Bind();

				// Bottom of top half
				vbo->BufferSubData(7, sizeof(float), &heightMinusOne);
				vbo->BufferSubData(13, sizeof(float), &heightMinusOne);

				// Top of bottom half
				vbo->BufferSubData(25, sizeof(float), &heightMinusOne);
				vbo->BufferSubData(43, sizeof(float), &heightMinusOne);

				// Bottom of bottom half
				vbo->BufferSubData(31, sizeof(float), &height);
				vbo->BufferSubData(37, sizeof(float), &height);

				vbo->Unbind();
			}

			pos.y += title.GetBounds().height;

			++iter;
		}
	}

	std::filesystem::path path;
	std::vector<std::filesystem::path> files;
	std::vector<std::filesystem::path>::iterator currentFile = files.end();

	int windowWidth = 0, windowHeight = 0;
	OpenGLFont *font = nullptr;
	OpenGLFont *outlineFont = nullptr;
	Context *context = nullptr;
	std::vector<Text> titles;
	Text outline;

	Vector2i pos{ 0, 0 };
	Vector2i size{ 0, 0 };

	std::unique_ptr<Cue> cue;

	float scale = 1.0f;

	std::unique_ptr<VertexArray> vao;
	std::unique_ptr<ArrayBuffer> vbo;
	std::unique_ptr<ElementBuffer> eab;

	float height = 0.0f;

	bool currentSongVisible = Settings::settings.GetCurrentSongVisible();

	float maxHeight = 0.0f;
};
