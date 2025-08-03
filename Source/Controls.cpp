#include "Controls.hpp"

#include <sstream>

#include "Hash.hpp"

Controls::Controls(AlbumArt *const albumArt) : 
	albumArt(albumArt), 
	FontFiles({
			"KurintoSans-Rg.ttf",
			"KurintoSansAux-Rg.ttf",
			"KurintoSansJP-Rg.ttf",
			"KurintoSansKR-Rg.ttf"
	}) {

}

inline void Controls::OpenFont(Context *context) {
	if (font)
		font->SetFontSize(static_cast<int>(18 * scale));
	else {
		font = new OpenGLFont();
		font->OnInit(
			FontFiles,
			static_cast<FT_UInt>(18 * scale)
		);
	}

	if (font) {
		elapsedText.OnInit(font, context);
		remainingText.OnInit(font, context);
		titleText.OnInit(font, context);
		artistText.OnInit(font, context);
		albumText.OnInit(font, context);
		fpsCounter.OnInit(font, context);
		exclusiveIndicator.OnInit(font, context);
		volume.OnInit(FontFiles, context);
	} else {
		logger.LogError("Could not open font!");
	}
}

void Controls::OnInit(int windowWidth, int windowHeight, Context &context, float scale) {
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;
	this->scale = scale;

	vao = std::make_unique<VertexArray>();
	vbo = std::make_unique<ArrayBuffer>();
	eab = std::make_unique<ElementBuffer>();

	vao->Bind();
	vbo->Bind();
	vao->AddAttribute(VertexArray::Attribute(0, 2, 2 * sizeof(float)));
	vbo->Unbind();
	vao->Unbind();

	eab->Bind();
	eab->BufferData<std::size(Buffers::SquareBuffer)>(Buffers::SquareBuffer);
	eab->Unbind();

	OpenFont(&context);

	playlist.OnInit(windowWidth, windowHeight, font, &context, scale);
	albumArt->AddColorChangeListener(&volume);
}

void Controls::OnResize(int windowWidth, int windowHeight, Context &context, float scale) {
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;

	if (scale != this->scale) {
		this->scale = scale;
		OpenFont(&context);

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
		logger.LogDebug("Song is ", currentFileLength, " seconds long");
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

double Controls::OnLoop(const Delta &time, HSTREAM streamHandle, Context &context, std::function<void(float)> setColor) {
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
		float pixels = 0.0f;
		if (currentPos != -1) {
			pixels = static_cast<float>((currentPos / GetCurrentSongLength()) * windowWidth);

			std::vector<float> posRect = {
				0,
				0,
				0,
				SeekbarSize * scale,
				pixels,
				SeekbarSize * scale,
				pixels,
				0 
			};

			vbo->Bind();
			vbo->BufferData(posRect, GL_DYNAMIC_DRAW);
			vbo->Unbind();
		}

		context.Use("basic"_hash);
		context.Translate(0, windowHeight - SeekbarSize * scale, 0.0f);
		context.Apply();

		setColor(alpha);

		vao->Bind();
		eab->Bind();
		eab->DrawElements(GL_TRIANGLES);
		eab->Unbind();
		vao->Unbind();

		context.Use("texture"_hash);
		context.LoadIdentity();

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

			context.Color(1.0f, 1.0f, 1.0f, alpha);

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
				(albumText.GetBounds().height +
				 artistText.GetBounds().height +
				 titleText.GetBounds().height);

			yOffset += elapsedText.GetBounds().height * 2 /* put an empty line between */ + margin;
			if (albumArt->Loaded()) {

				// Album art is 4x the total height of the text 
				albumHeight *= 4;

				xOffset +=
					albumArt->DrawSquare(
						margin,
						windowHeight - yOffset - albumHeight,
						albumHeight,
						alpha,
						context
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
					windowHeight - (yOffset += albumText.GetBounds().height)
				);
			}
			if (!artistText.Empty()) {
				artistText.OnLoop(
					margin + xOffset,
					windowHeight - (yOffset += artistText.GetBounds().height)
				);
			}
			if (!titleText.Empty()) {
				titleText.OnLoop(
					margin + xOffset,
					windowHeight - (yOffset += titleText.GetBounds().height)
				);
			}

			fpsCounter.Draw(context);

			auto aboveMetadata = windowHeight - yOffset - albumHeight / 2 - exclusiveIndicator.GetHeight() / 2;

			playlist.OnLoop(fpsCounter.GetSize(), aboveMetadata - albumHeight / 8.0f, alpha, context);

			// Only render our volume if we're in exclusive mode
			if (exclusiveIndicator.IsExclusive())
				volume.OnLoop(windowWidth / 2, windowHeight / 2, time, context);

			exclusiveIndicator.OnLoop(margin, aboveMetadata, alpha, context);
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

	vao.reset();
	vbo.reset();
	eab.reset();

	playlist.OnDestroy();

	elapsedText.OnDestroy();
	remainingText.OnDestroy();
	titleText.OnDestroy();
	artistText.OnDestroy();
	albumText.OnDestroy();
	fpsCounter.OnDestroy();
	volume.OnDestroy();
	exclusiveIndicator.OnDestroy();

	font->OnDestroy();
	delete font;
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