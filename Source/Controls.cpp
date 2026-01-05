#include "Controls.hpp"

#include <sstream>

#include "Hash.hpp"
#include "HDR.hpp"

Controls::Controls(AlbumArt *const albumArt) : 
	albumArt(albumArt), 
	FontRoot("KurintoSans") {

}

void Controls::OpenFont(Context *context, GLuint defaultFramebuffer) {
	if (font && outlineFont) {
		font->SetFontSize(static_cast<int>(18 * scale));
		font->SetDefaultFramebuffer(defaultFramebuffer);
		outlineFont->SetFontSize(static_cast<int>(18 * scale));
		outlineFont->SetDefaultFramebuffer(defaultFramebuffer);
	} else {
		font = new OpenGLFont();
		font->SetDefaultFramebuffer(defaultFramebuffer);
		font->OnInit(
			FontRoot,
			static_cast<FT_UInt>(18 * scale)
		);

		outlineFont = new OpenGLFont();
		outlineFont->SetDefaultFramebuffer(defaultFramebuffer);
		outlineFont->OnInit(
			FontRoot,
			static_cast<FT_UInt>(18 * scale),
			3
		);
	}

	if (font) {
		elapsedText.OnInit(font, context);
		elapsedOutline.OnInit(outlineFont, context);
		remainingText.OnInit(font, context);
		remainingOutline.OnInit(outlineFont, context);
		titleText.OnInit(font, context);
		titleOutline.OnInit(outlineFont, context);
		artistText.OnInit(font, context);
		artistOutline.OnInit(outlineFont, context);
		albumText.OnInit(font, context);
		albumOutline.OnInit(outlineFont, context);
		fpsCounter.OnInit(font, outlineFont, context);
		exclusiveIndicator.OnInit(font, outlineFont, context);
		volume.OnInit(FontRoot, context);
	} else {
		LogError("Could not open font!");
	}
}

void Controls::OnInit(int windowWidth, int windowHeight, Context &context, float scale, GLuint defaultFramebuffer) {
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

	sepVao = std::make_unique<VertexArray>();
	sepVbo = std::make_unique<ArrayBuffer>();
	sepEab = std::make_unique<ElementBuffer>();

	sepVao->Bind();
	sepVbo->Bind();
	sepVao->AddAttribute(VertexArray::Attribute(0, 2, 2 * sizeof(float)));
	sepVbo->Unbind();
	sepVao->Unbind();

	sepEab->Bind();
	sepEab->BufferData<std::size(Buffers::SquareBuffer)>(Buffers::SquareBuffer);
	sepEab->Unbind();

	OpenFont(&context, defaultFramebuffer);

	playlist.OnInit(windowWidth, windowHeight, font, outlineFont, &context, scale);

	albumArt->AddColorChangeListener(&volume);
	albumArt->AddColorChangeListener(&pause);
	albumArt->AddColorChangeListener(&play);
	albumArt->AddColorChangeListener(&next);
	albumArt->AddColorChangeListener(&previous);

	pause.OnInit(albumArt->GetRadius());
	play.OnInit(albumArt->GetRadius());
	next.OnInit(albumArt->GetRadius());
	previous.OnInit(albumArt->GetRadius());
}

void Controls::OnResize(int windowWidth, int windowHeight, Context &context, float scale, GLuint defaultFramebuffer) {
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;

	if (scale != this->scale) {
		this->scale = scale;
		OpenFont(&context, defaultFramebuffer);

		// Note: albumArt's radius _must_ be updated
		//       first.
		albumArt->OnResize(windowWidth, windowHeight, scale);
		volume.SetRadius(albumArt->GetRadius());
		pause.OnResize(albumArt->GetRadius());
		play.OnResize(albumArt->GetRadius());
		next.OnResize(albumArt->GetRadius());
		previous.OnResize(albumArt->GetRadius());
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
		LogDebug("Song is ", currentFileLength, " seconds long");
	}
	currentPos = 0.0;
	elapsedSeconds = -1;

	return totalBytes;
}

void Controls::SetTitle(const std::string &title) {
	titleText.SetText(title);
	titleOutline.SetText(title);
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

void Controls::ClearTags() {
	titleText.SetText("");
	titleOutline.SetText("");

	artistText.SetText("");
	artistOutline.SetText("");

	albumText.SetText("");
	albumOutline.SetText("");
}

void Controls::LoadFromTags(const std::map<std::string, std::string> &tags) {
	if (auto title = tags.find("title"); title != tags.end()) {
		titleText.SetText(title->second);
		titleOutline.SetText(title->second);
	} else {
		titleText.SetText("");
		titleOutline.SetText("");
	}

	if (auto artist = tags.find("artist"); artist != tags.end()) {
		artistText.SetText(artist->second);
		artistOutline.SetText(artist->second);
	} else if (auto albumArtist = tags.find("albumartist"); albumArtist != tags.end()) {
		artistText.SetText(albumArtist->second);
		artistOutline.SetText(albumArtist->second);
	} else {
		artistText.SetText("");
		artistOutline.SetText("");
	}

	if (auto album = tags.find("album"); album != tags.end()) {
		albumText.SetText(album->second);
		albumOutline.SetText(album->second);
	} else {
		albumText.SetText("");
		albumOutline.SetText("");
	}
}

void Controls::LoadFromID3v1(const TAG_ID3 *id3) {
	if (titleText.Empty() && id3->title[0] != '\0') {
		auto title = std::string(id3->title, id3->title + 30);
		titleText.SetText(title);
		titleOutline.SetText(title);
	}
	if (artistText.Empty() && id3->artist[0] != '\0') {
		auto artist = std::string(id3->artist, id3->artist + 30);
		artistText.SetText(artist);
		artistOutline.SetText(artist);
	}
	if (albumText.Empty() && id3->album[0] != '\0') {
		auto album = std::string(id3->album, id3->album + 30);
		albumText.SetText(album);
		albumOutline.SetText(album);
	}
}

double Controls::OnLoop(const Delta &time, HSTREAM streamHandle, Context &context, const Colour<float> &color) {
	AutoFader::OnLoop(time);

	if (context.GetSafeArea().y > 0 && alpha > 0.0f) {
		context.Use("basic"_hash);

		context.LoadIdentity();
		context.Translate(0, 0, 0);
		context.Apply();

		if (!letterboxVao) {
			letterboxVao = std::make_unique<VertexArray>();
			letterboxVbo = std::make_unique<ArrayBuffer>();
			letterboxEab = std::make_unique<ElementBuffer>();
		}

		std::vector<float> letterbox = {
			0.0f, 0.0f,
			0.0f, static_cast<float>(context.GetSafeArea().y),
			static_cast<float>(windowWidth), static_cast<float>(context.GetSafeArea().y),
			static_cast<float>(windowWidth), 0.0f
		};

		letterboxVao->Bind();

		letterboxVbo->Bind();
		letterboxVao->AddAttribute(VertexArray::Attribute(0, 2, 2 * sizeof(float)));
		letterboxVbo->BufferData(letterbox);
		letterboxVbo->Unbind();

		letterboxEab->Bind();

		letterboxEab->BufferData<std::size(Buffers::SquareBuffer)>(Buffers::SquareBuffer);
		context.Color(0.0f, 0.0f, 0.0f, alpha);
		letterboxEab->DrawElements(GL_TRIANGLES);

		letterbox = {
			0.0f, static_cast<float>(context.GetSafeArea().h + context.GetSafeArea().y),
			0.0f, static_cast<float>(windowHeight),
			static_cast<float>(windowWidth), static_cast<float>(windowHeight),
			static_cast<float>(windowWidth), static_cast<float>(context.GetSafeArea().h + context.GetSafeArea().y)
		};

		letterboxVbo->Bind();
		letterboxVbo->BufferData(letterbox);
		letterboxVbo->Unbind();

		letterboxEab->DrawElements(GL_TRIANGLES);

		letterboxEab->Unbind();
		letterboxVao->Unbind();
	}

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

			posRect = {
				0,
				0,
				0,
				SeekbarSize / 5.0f * scale,
				pixels,
				SeekbarSize / 5.0f * scale,
				pixels,
				0
			};

			sepVbo->Bind();
			sepVbo->BufferData(posRect, GL_DYNAMIC_DRAW);
			sepVbo->Unbind();
		}

		context.Use("basic"_hash);
		context.Translate(0, (context.GetSafeArea().h + context.GetSafeArea().y) - SeekbarSize * scale, 0.0f);
		context.Apply();

		context.Color(color.r, color.g, color.b, alpha);

		vao->Bind();
		eab->Bind();
		eab->DrawElements(GL_TRIANGLES);
		eab->Unbind();
		vao->Unbind();

		auto inverse = color.Inverse(HDR::Enabled ? std::max(color.r, std::max(color.g, color.b)) : 1.0f);

		if (HDR::Enabled) {
			inverse *= HDR::WhiteLevel * HDR::Headroom;
			inverse.a = 1.0f;
		}
		
		context.Color(inverse.r, inverse.g, inverse.b, alpha);

		sepVao->Bind();
		sepEab->Bind();
		sepEab->DrawElements(GL_TRIANGLES);
		sepEab->Unbind();
		sepVao->Unbind();

		context.Use("texture"_hash);
		context.LoadIdentity();

		auto elapsed = static_cast<int>(currentPos);

		if (font) {
			if (elapsed != elapsedSeconds && elapsed != -1) {
				auto elapsedFormatted = FormatSeconds(elapsed);

				elapsedOutline.SetText(elapsedFormatted);
				elapsedText.SetText(elapsedFormatted);

				auto remaining = static_cast<int>(GetCurrentSongLength() - elapsed);

				auto remainingFormatted = FormatSeconds(remaining);

				remainingOutline.SetText(remainingFormatted);
				remainingText.SetText(remainingFormatted);

				elapsedSeconds = elapsed;
			}

			constexpr static int margin = 16;

			auto yOffset = static_cast<int>(SeekbarSize * scale);

			context.Color(0.0f, 0.0f, 0.0f, alpha);
			for (const auto *text : { &elapsedOutline, &elapsedText }) {
				text->OnLoop(
					std::min(
						std::max(
							margin,
							static_cast<int>(pixels - elapsedText.GetSize().x / 2.0f)
						),
						windowWidth - elapsedText.GetSize().x - margin
					),
					(context.GetSafeArea().h + context.GetSafeArea().y) - yOffset - elapsedText.GetSize().y - outlineFont->GetOutlineRadius()
				);
				context.Color(1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, alpha);
			}

			context.Color(0.0f, 0.0f, 0.0f, alpha);
			for (const auto *text : { &remainingOutline, &remainingText }) {
				text->OnLoop(
					windowWidth - remainingText.GetSize().x - margin,
					(context.GetSafeArea().h + context.GetSafeArea().y) - yOffset / 2 - remainingText.GetSize().y / 2 + outlineFont->GetOutlineRadius()
				);
				context.Color(1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, alpha);
			}

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
						(context.GetSafeArea().h + context.GetSafeArea().y) - yOffset - albumHeight,
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
				context.Color(0.0f, 0.0f, 0.0f, alpha);
				albumOutline.OnLoop(
					margin + xOffset,
					(context.GetSafeArea().h + context.GetSafeArea().y) - (yOffset + albumText.GetBounds().height)
				);
				context.Color(1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, alpha);
				albumText.OnLoop(
					margin + xOffset,
					(context.GetSafeArea().h + context.GetSafeArea().y) - (yOffset += albumText.GetBounds().height)
				);
			}
			if (!artistText.Empty()) {
				context.Color(0.0f, 0.0f, 0.0f, alpha);
				artistOutline.OnLoop(
					margin + xOffset,
					(context.GetSafeArea().h + context.GetSafeArea().y) - (yOffset + artistText.GetBounds().height)
				);
				context.Color(1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, alpha);
				artistText.OnLoop(
					margin + xOffset,
					(context.GetSafeArea().h + context.GetSafeArea().y) - (yOffset += artistText.GetBounds().height)
				);
			}
			if (!titleText.Empty()) {
				context.Color(0.0f, 0.0f, 0.0f, alpha);
				titleOutline.OnLoop(
					margin + xOffset,
					(context.GetSafeArea().h + context.GetSafeArea().y) - (yOffset + titleText.GetBounds().height)
				);
				context.Color(1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, alpha);
				titleText.OnLoop(
					margin + xOffset,
					(context.GetSafeArea().h + context.GetSafeArea().y) - (yOffset += titleText.GetBounds().height)
				);
			}

			fpsCounter.Draw(alpha);

			auto aboveMetadata = (context.GetSafeArea().h + context.GetSafeArea().y) - yOffset - albumHeight / 2 - exclusiveIndicator.GetHeight() / 2;

			playlist.OnLoop(fpsCounter.GetSize(), aboveMetadata - albumHeight / 8.0f + (context.GetYOffset() - context.GetSafeArea().y), alpha, context);

#ifdef WIN32
			// Only render our volume if we're in exclusive mode
			if (exclusiveIndicator.IsExclusive())
				volume.OnLoop(windowWidth / 2, windowHeight / 2, time, context);

			exclusiveIndicator.OnLoop(margin, aboveMetadata, alpha, context);
#endif
			pause.OnLoop(windowWidth / 2.0f, windowHeight / 2.0f, time, context);
			play.OnLoop(windowWidth / 2.0f, windowHeight / 2.0f, time, context);
			next.OnLoop(windowWidth - albumArt->GetRadius(), windowHeight / 2.0f, time, context);
			previous.OnLoop(albumArt->GetRadius(), windowHeight / 2.0f, time, context);
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

	sepVao.reset();
	sepVbo.reset();
	sepEab.reset();

	letterboxVao.reset();
	letterboxVbo.reset();
	letterboxEab.reset();

	playlist.OnDestroy();

	pause.OnDestroy();
	play.OnDestroy();
	next.OnDestroy();
	previous.OnDestroy();

	elapsedText.OnDestroy();
	elapsedOutline.OnDestroy();
	remainingText.OnDestroy();
	remainingOutline.OnDestroy();
	titleText.OnDestroy();
	titleOutline.OnDestroy();
	artistText.OnDestroy();
	artistOutline.OnDestroy();
	albumText.OnDestroy();
	albumOutline.OnDestroy();
	fpsCounter.OnDestroy();
	volume.OnDestroy();
	exclusiveIndicator.OnDestroy();

	font->OnDestroy();
	outlineFont->OnDestroy();
	delete font;
	font = nullptr;
	delete outlineFont;
	outlineFont = nullptr;
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