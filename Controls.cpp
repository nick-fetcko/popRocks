#include "Controls.hpp"

#include <sstream>

#include "CConsole.h"

// FIXME: KurintoSans covers a good span of Unicode characters, but not all.
//        For example: it has all the kana, but no kanji
//
//        We should swap it out with the other language variants (e.g. CJK)
//        as needed, but SDL_ttf isn't really designed to support this as we
//        might need multiple fonts present in the _same string_ if there
//        are a mix of Latin and other language characters.
//
//        A homegrown font implementation similar to https://learnopengl.com/In-Practice/Text-Rendering
//        would be ideal here, as we can use a different font _per character_.
Controls::Controls(AlbumArt *const albumArt) : albumArt(albumArt), FontFile("../../Data/KurintoSans-Rg.ttf") {

}

inline void Controls::OpenFont() {
	if (font)
		TTF_SetFontSize(font, static_cast<int>(18 * scale));
	else
		font = TTF_OpenFont(FontFile.data(), static_cast<int>(18 * scale));

	if (font) {
		elapsedText.OnInit(font);
		remainingText.OnInit(font);
		titleText.OnInit(font);
		artistText.OnInit(font);
		albumText.OnInit(font);
		fpsCounter.OnInit(font);
		exclusiveIndicator.OnInit(font);
		volume.OnInit(FontFile);
	} else {
		CConsole::Console.Print("Could not open font!", MSG_ERROR);
	}
}

void Controls::OnInit(int windowWidth, int windowHeight, float scale) {
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;
	this->scale = scale;

	auto ret = TTF_Init();

	if (ret == 0)
		OpenFont();
	else
		CConsole::Console.Print("Could not initialize SDL_ttf!", MSG_ERROR);

	playlist.OnInit(windowWidth, windowHeight, font, scale);
	albumArt->AddColorChangeListener(&volume);
}

void Controls::OnResize(int windowWidth, int windowHeight, float scale) {
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;

	if (scale != this->scale) {
		this->scale = scale;
		OpenFont();

		// Note: albumArt's radius _must_ be updated
		//       first.
		albumArt->OnResize(windowWidth, windowHeight, scale);
		volume.SetRadius(albumArt->GetRadius());
	}

	playlist.OnResize(windowWidth, windowHeight, scale);
}

QWORD Controls::OnLoad(HSTREAM streamHandle) {
	QWORD totalBytes = 0;
	if (streamHandle) {
		totalBytes = BASS_ChannelGetLength(streamHandle, BASS_POS_BYTE);
		currentFileLength = BASS_ChannelBytes2Seconds(
			streamHandle,
			totalBytes
		);
		CConsole::Console.Print("Song is " + std::to_string(currentFileLength) + " seconds long", MSG_DIAG);
	}

	titleText.SetText("");
	artistText.SetText("");
	albumText.SetText("");

	currentPos = 0.0;
	elapsedSeconds = -1;

	return totalBytes;
}

void Controls::SetTitle(const std::string &title) {
	titleText.SetText(title);
}

void Controls::LoadFromCue() {
	if (auto &cue = playlist.GetCue()) {
		LoadFromTags({
			{ "album", cue->GetTitle() },
			{ "artist", cue->GetCurrentTrack()->performer.empty() ? cue->GetPerformer() : cue->GetCurrentTrack()->performer },
			{ "title", cue->GetCurrentTrack()->title }
		});
	}
}

void Controls::LoadFromTags(const std::map<std::string, std::string> &tags) {
	if (auto title = tags.find("title"); title != tags.end())
		titleText.SetText(title->second);

	if (auto artist = tags.find("artist"); artist != tags.end())
		artistText.SetText(artist->second);
	else if (auto albumArtist = tags.find("albumartist"); albumArtist != tags.end())
		artistText.SetText(albumArtist->second);

	if (auto album = tags.find("album"); album != tags.end())
		albumText.SetText(album->second);
}

void Controls::LoadFromID3v1(const TAG_ID3 *id3) {
	if (titleText.Empty() && id3->title[0] != '\0')
		titleText.SetText(std::string(id3->title, id3->title + 30));
	if (artistText.Empty() && id3->artist[0] != '\0')
		artistText.SetText(std::string(id3->artist, id3->artist + 30));
	if (albumText.Empty() && id3->album[0] != '\0')
		albumText.SetText(std::string(id3->album, id3->album + 30));
}

double Controls::OnLoop(const Delta &time, HSTREAM streamHandle, std::function<void(float)> setColor) {
	AutoFader::OnLoop(time);

	if (streamHandle) {
		auto &cue = playlist.GetCue();

		currentPos = BASS_ChannelBytes2Seconds(
			streamHandle,
			BASS_ChannelGetPosition(streamHandle, BASS_POS_BYTE)
		);

		if (cue)
			currentPos -= cue->GetCurrentTrack()->startTime;

		// Only update if BASS didn't error out
		auto pixels = posRect[4];
		if (currentPos != -1) {
			pixels = static_cast<float>((currentPos / GetCurrentSongLength()) * windowWidth);

			posRect[0] = 0;
			posRect[1] = 0;
			posRect[2] = 0;
			posRect[3] = SeekbarSize * scale;
			posRect[4] = pixels;
			posRect[5] = SeekbarSize * scale;
			posRect[6] = pixels;
			posRect[7] = 0;
		}

		glTranslatef(0, windowHeight - SeekbarSize * scale, 0.0f);

		glVertexPointer(2, GL_FLOAT, 0, posRect);

		glEnableClientState(GL_VERTEX_ARRAY);

		setColor(alpha);

		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, Buffer::SquareBuffer.data());
		glDisableClientState(GL_VERTEX_ARRAY);

		glLoadIdentity();

		auto elapsed = static_cast<int>(currentPos);

		if (font) {
			if (elapsed != elapsedSeconds && elapsed != -1) {
				elapsedText.SetText(FormatSeconds(elapsed));

				auto remaining = static_cast<int>(GetCurrentSongLength() - elapsed);
				remainingText.SetText(FormatSeconds(remaining));

				elapsedSeconds = elapsed;
			}

			constexpr static int margin = 16;

			auto yOffset = static_cast<int>(SeekbarSize * scale);

			glColor4f(1.0f, 1.0f, 1.0f, alpha);

			elapsedText.OnLoop(
				std::min(
					std::max(
						margin,
						static_cast<int>(pixels - elapsedText.GetSize().x / 2.0f)
					),
					windowWidth - elapsedText.GetSize().x - margin
				),
				windowHeight - yOffset - elapsedText.GetSize().y
			);

			remainingText.OnLoop(
				windowWidth - remainingText.GetSize().x - margin,
				windowHeight - yOffset / 2 - remainingText.GetSize().y / 2
			);

			int xOffset = 0;

			int albumHeight =
				(albumText.GetSize().y +
				 artistText.GetSize().y +
				 titleText.GetSize().y);

			yOffset += elapsedText.GetSize().y * 2 /* put an empty line between */ + margin;
			if (albumArt->Loaded()) {

				// Album art is 4x the total height of the text 
				albumHeight *= 4;

				xOffset +=
					albumArt->DrawSquare(
						margin,
						windowHeight - yOffset - albumHeight,
						albumHeight,
						alpha
					) + margin;

				// Since album height is 4 * text height:
				// 
				// Half album height is 4/8
				// Half text height is 1/8
				// 
				// 4/8 - 1/8 = 3/8
				yOffset += static_cast<int>(albumHeight * (3.0f / 8.0f));
			}

			if (!albumText.Empty()) {
				albumText.OnLoop(
					margin + xOffset,
					windowHeight - (yOffset += albumText.GetSize().y)
				);
			}
			if (!artistText.Empty()) {
				artistText.OnLoop(
					margin + xOffset,
					windowHeight - (yOffset += artistText.GetSize().y)
				);
			}
			if (!titleText.Empty()) {
				titleText.OnLoop(
					margin + xOffset,
					windowHeight - (yOffset += titleText.GetSize().y)
				);
			}

			fpsCounter.Draw();

			auto aboveMetadata = windowHeight - yOffset - albumHeight / 2 - exclusiveIndicator.GetHeight() / 2;

			playlist.OnLoop(fpsCounter.GetSize(), aboveMetadata - albumHeight / 8.0f, alpha);

			// Only render our volume if we're in exclusive mode
			if (exclusiveIndicator.IsExclusive())
				volume.OnLoop(windowWidth / 2, windowHeight / 2, time);

			exclusiveIndicator.OnLoop(margin, aboveMetadata, alpha);
		}

		return currentPos;
	}

	return 0.0;
}

void Controls::SetElapsedSeconds(int elapsedSeconds) {
	this->elapsedSeconds = elapsedSeconds;
}

double Controls::GetCurrentFileLength() const {
	return currentFileLength;
}

double Controls::GetCurrentSongLength() const {
	if (auto &cue = playlist.GetCue())
		return cue->GetCurrentTrackLength(currentFileLength);

	return currentFileLength;
}

std::optional<double> Controls::GetNextSongLength() const {
	if (auto &cue = playlist.GetCue()) {
		if (auto next = cue->GetCurrentTrack() + 1; next != cue->GetTracks().end()) {
			if (auto afterNext = cue->GetCurrentTrack() + 2; afterNext != cue->GetTracks().end())
				return afterNext->startTime - next->startTime;
			else
				return currentFileLength - next->startTime;
		}

		return currentFileLength;
	}

	return std::nullopt;
}

void Controls::OnDestroy() {
	albumArt->RemoveColorChangeListener(&volume);

	elapsedText.OnDestroy();
	remainingText.OnDestroy();
	titleText.OnDestroy();
	artistText.OnDestroy();
	albumText.OnDestroy();
	fpsCounter.OnDestroy();
	volume.OnDestroy();
	exclusiveIndicator.OnDestroy();

	TTF_CloseFont(font);
	TTF_Quit();
}

std::string Controls::FormatSeconds(int seconds) const {
	std::stringstream stream;
	stream <<
		std::setw(2) << std::setfill('0') <<
		seconds / 60 <<
		":" <<
		std::setw(2) << std::setfill('0') <<
		seconds % 60;

	return stream.str();
}