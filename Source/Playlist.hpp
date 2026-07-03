#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

#include "Utils/Logger.hpp"
#include "Utils/Utils.hpp"

#include "AlbumArt.hpp"
#include "Buffer.hpp"
#include "Cue.hpp"
#include "Hash.hpp"
#include "HDR.hpp"
#include "Metadata.hpp"
#include "MiniPlayerList.hpp"
#include "Settings.hpp"
#include "TagLoader.hpp"
#include "ScrollingText.hpp"

using namespace Fetcko;

class Playlist : public MiniPlayerList, public LoggableClass {
public:
	struct Track {
		std::filesystem::path path;
		std::string title;
		double startTime = 0.0;
	};

	Playlist(AlbumArt *const albumArt, const bool &vulkan) : MiniPlayerList(Direction::Down, albumArt, vulkan), albumArt(albumArt), currentTitle(vulkan), outline(vulkan) {
		albumArt->AddBlackChangedListener(this);

		const auto &black = albumArt->GetBlackColor();

		outline.SetColor({ black, black, black });
	}

	virtual ~Playlist() {
		albumArt->RemoveBlackChangedListener(this);
	}

	std::optional<Track> OnLoad(
		const std::filesystem::path &path,
		const std::string_view &extension,
		const bool &loading,
		std::function<HSTREAM(const std::filesystem::path &, const std::string &, DWORD)> openWithFlags
	);

	void OnInit(int windowWidth, int windowHeight, OpenGLFont *font, OpenGLFont *boldFont, OpenGLFont *outlineFont, OpenGLFont *boldOutlineFont, Context *context, float scale = 1.0f);
	bool OnResize(int windowWidth, int windowHeight, float scale = 1.0f, bool miniPlayer = false, float maxWidth = 0.0f) override;
	void OnDestroy() override;

	void AddFile(const std::filesystem::path &path);

	void Clear();

	std::optional<Track> Current();
	const std::optional<std::size_t> GetCurrentIndex() const;
	std::optional<Track> TrackAtIndex(std::size_t index);
	std::optional<Track> Previous();
	std::optional<Track> Next();

	const std::optional<Track> GetNext() const;

	void OnLoop(const Delta &time, Vector2i pos, float maxHeight, float alpha, Context &context, bool miniPlayer = false, bool hidden = false);

	std::optional<Track> OnMouseClicked(const Vector2i &mousePos);
	bool OnMouseMoved(const Vector2i &mousePos);

	void SetVisible(bool visible) {
		this->visible = visible;
		Settings::settings.SetPlaylistOnScreen(visible);
	}

	void SetFade(bool fade) {
		this->fade = fade;
		Settings::settings.SetPlaylistFade(fade);
	}

	void SetCurrentSongVisible(bool currentSongVisible) {
		this->currentSongVisible = currentSongVisible;
		Settings::settings.SetCurrentSongVisible(currentSongVisible);
	}

	void OnBlackChanged(const float &black) override;

	const std::unique_ptr<Cue> &GetCue() const;
	const std::filesystem::path &GetPath() const;

	const std::vector<ScrollingText> &GetTitles() const { return items; }

	const bool IsHovered() const { return hovered; }

	const float &GetMiniPlayerAlpha() const { return MiniPlayerList::alpha; }

	const ScrollingText &GetOutline() const { return outline; }

	void LoadTitles();

	const bool IsLoaded() const { return loaded; }

	static constexpr bool IsCue(const std::string_view &lowercaseExtension) {
		return lowercaseExtension == ".cue";
	}

	static constexpr auto &GetSupportedExtensions() {
		return SupportedExtensions;
	}

private:
	constexpr inline static float NoFadeAlpha = 0.75f;
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
		for (const auto &[i, title] : Utils::Enumerate(titles)) {
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

			const auto &bounds = AddItem(stream.str(), i, title.title);

			size.y += bounds.height;
			if (bounds.width > size.x)
				size.x = bounds.width;
		}

		if (miniPlayer) {
			scrollOffset = 0;
			OnRadiusChanged();
		}

		UpdateSize();

		loaded = true;
	}

	template<typename T>
	void OnLoop(
		const Delta &time,
		const std::vector<T> &tracks,
		const typename std::vector<T>::const_iterator &current,
		Vector2i pos, // need a local pos var because we modify it
		float maxHeight,
		float alpha,
		Context &context,
		bool miniPlayer = false,
		bool hidden = false
	) {
		if (tracks.empty() || items.empty())
			return;

		// Mini-player playlist has no backing rectangle
		if (miniPlayer) {
			this->pos = pos;

			const auto currentIndex = std::distance(tracks.begin(), current);
			const auto &title = items.at(currentIndex);

			currentTitle.SetText(title.GetAltText());

			const auto &bounds = currentTitle.GetBounds();

			if (currentSongVisible) {
				outline.SetText(title.GetAltText());
				context.Use("scrolling"_hash);
				context.Color(1.0f, 1.0f, 1.0f, std::max(hidden ? 0.0f : 0.5f, alpha));
				outline.OnLoop(pos.x - bounds.width / 2, pos.y - bounds.height / 2, time);
				context.Color(
					HDR::WhiteLevel,
					HDR::WhiteLevel,
					HDR::WhiteLevel,
					std::max(hidden ? 0.0f : 0.5f, alpha)
				);
			} else {
				context.Use("scrolling"_hash);
				context.Color(HDR::WhiteLevel, HDR::WhiteLevel, HDR::WhiteLevel, alpha);
			}

			currentTitle.OnLoop(pos.x - bounds.width / 2, pos.y - bounds.height / 2, time);

			MiniPlayerList::PreLoop(currentIndex);
			MiniPlayerList::OnLoop(time, pos, currentIndex, std::min(GetAlpha(), alpha));
			MiniPlayerList::PostLoop(time);

			return;
		}

		const auto distance = static_cast<int32_t>(
			current - tracks.begin()
		);

		const auto trackHeight = ((tracks.size() - distance) * font->GetEm().height);

		// Limit our background rectangle to our
		// max height, rounding down to the nearest
		// line height
		if (auto height = std::min(static_cast<int>((maxHeight - pos.y) / font->GetEm().height) * font->GetEm().height, size.y + font->GetEm().height / 2);
			this->height > height || trackHeight > lastTrackHeight) {
			this->height = height;

			const auto heightMinusOne = static_cast<float>(height - font->GetEm().height);

			fadeOut = (HDR::Enabled ? 0.25f : (1.0f - heightMinusOne / height));

			vbo->Bind();

			// Bottom of top half
			vbo->BufferSubData(7, sizeof(float), &heightMinusOne);
			vbo->BufferSubData(13, sizeof(float), &heightMinusOne);

			// Top of bottom half
			vbo->BufferSubData(25, sizeof(float), &heightMinusOne);
			vbo->BufferSubData(43, sizeof(float), &heightMinusOne);

			// Bottom of bottom half
			vbo->BufferSubData(31, sizeof(float), &this->height);
			vbo->BufferSubData(37, sizeof(float), &this->height);

			vbo->Unbind();
		}

		lastTrackHeight = trackHeight;

		if (this->maxHeight != maxHeight)
			this->maxHeight = maxHeight;

		context.Use("color"_hash);
		context.Translate(static_cast<GLfloat>(pos.x), static_cast<GLfloat>(pos.y), 0.0f);
		context.Apply();

		vbo->Bind();

		const auto zero = (fade ? 0.0f : (NoFadeAlpha * alpha));
		const auto fadedOut = std::min(fade ? fadeOut : NoFadeAlpha, alpha);

		// Top of top half
		vbo->BufferSubData(5, sizeof(float), fade ? &alpha : &zero);
		vbo->BufferSubData(23, sizeof(float), fade ? &alpha : &zero);
		
		// Bottom of top half
		vbo->BufferSubData(11, sizeof(float), &fadedOut);
		vbo->BufferSubData(17, sizeof(float), &fadedOut);

		// Top of bottom half
		vbo->BufferSubData(29, sizeof(float), &fadedOut);
		vbo->BufferSubData(47, sizeof(float), &fadedOut);

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
			items.empty() ?
				0 :
				(maxHeight - pos.y) / items.begin()->GetBounds().height
		);

		pos.y -= items.begin()->GetBounds().height * distance;

		auto iter = tracks.begin();
		for (const auto &[i, title] : Fetcko::Utils::Enumerate(items)) {
			if (pos.y + title.GetBounds().height > maxHeight)
				return;

			if (iter == current) {
				if (currentSongVisible) {
					outline.SetText(title.GetText());
					context.Color(0.0f, 0.0f, 0.0f, std::max(0.5f, alpha));
					outline.OnLoop(pos.x, pos.y, time);
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
					(fade ? 0.4f : NoFadeAlpha) * alpha
				);
			} else {
				const auto faded = fade ? std::max(
						0.0f,
						alpha - (static_cast<float>(i - distance) / std::min(items.size(), maxIndex))
				) : std::min(((static_cast<int64_t>(i) - distance) > 0 ? 1.0f : 0.0f), alpha) ;

				context.Color(
					0.6f * HDR::WhiteLevel * (HDR::Enabled ? faded : 1.0f),
					0.6f * HDR::WhiteLevel * (HDR::Enabled ? faded : 1.0f),
					0.6f * HDR::WhiteLevel * (HDR::Enabled ? faded : 1.0f),
					faded
				);
			}

			title.OnLoop(pos.x, pos.y, time);

			// Reduce the height as we near the end of the playlist
			if (i == items.size() - 2 && pos.y < maxHeight - this->pos.y) {
				height = trackHeight + 
					// Line for previous track
					((current != tracks.begin()) ? font->GetEm().height: 0.0f);

				// Add extra line so we can fade out more gradually
				if (fade && height < maxHeight - this->pos.y)
					height += font->GetEm().height;

				const auto heightMinusOne = height - font->GetEm().height;

				fadeOut = (HDR::Enabled ? 0.25f : (1.0f - heightMinusOne / height));

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

	bool loaded = false;

	std::filesystem::path path;
	std::vector<std::filesystem::path> files;
	std::vector<Title> titles;
	std::vector<std::filesystem::path>::iterator currentFile = files.end();

	OpenGLFont *font = nullptr;
	OpenGLFont *boldFont = nullptr;
	OpenGLFont *outlineFont = nullptr;
	OpenGLFont *boldOutlineFont = nullptr;
	Context *context = nullptr;
	ScrollingText currentTitle;
	ScrollingText outline;

	Vector2i size{ 0, 0 };

	std::unique_ptr<Cue> cue;

	std::unique_ptr<VertexArray> vao;
	std::unique_ptr<ArrayBuffer> vbo;
	std::unique_ptr<ElementBuffer> eab;

	float height = 0.0f;

	bool visible = Settings::settings.GetPlaylistOnScreen();
	bool fade = Settings::settings.GetPlaylistFade();
	bool currentSongVisible = Settings::settings.GetCurrentSongVisible();

	float maxHeight = 0.0f;
	float fadeOut = 0.0f;

	int lastTrackHeight = 0;

	AlbumArt *const albumArt = nullptr;
};
