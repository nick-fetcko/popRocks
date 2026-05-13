#include "Controls.hpp"

#include <sstream>

#include "Close.hpp"
#include "Hash.hpp"
#include "HDR.hpp"
#include "Preset.hpp"

Controls::Controls(AlbumArt *const albumArt, const bool &vulkan) :
	albumArt(albumArt),
	playlist(albumArt, vulkan),
	presetList(albumArt, vulkan),
	play(albumArt),
	pause(albumArt),
	next(albumArt),
	previous(albumArt),
	captureCheckbox(albumArt),
	FontRoot("KurintoSans"),
	artistText(vulkan),
	artistOutline(vulkan),
	albumText(vulkan),
	albumOutline(vulkan),
	presetText(vulkan),
	presetOutline(vulkan),
	help(MiniPlayerList::Direction::Down, albumArt, vulkan) {
	albumArt->AddBlackChangedListener(this);
	Preset::AddChangeListener(this);
}

Controls::~Controls() {
	albumArt->RemoveBlackChangedListener(this);
	Preset::RemoveChangeListener(this);
}

void Controls::OpenFont(Context *context, GLuint defaultFramebuffer) {
	if (font && outlineFont) {
		font->SetDefaultFramebuffer(defaultFramebuffer);
		boldFont->SetDefaultFramebuffer(defaultFramebuffer);
		outlineFont->SetDefaultFramebuffer(defaultFramebuffer);
		boldOutlineFont->SetDefaultFramebuffer(defaultFramebuffer);

		UpdateFontSize();
	} else {
		font = new OpenGLFont();
		font->SetDefaultFramebuffer(defaultFramebuffer);
		font->OnInit(
			FontRoot,
			static_cast<FT_UInt>(18 * scale)
		);

		boldFont = new OpenGLFont(true);
		boldFont->SetDefaultFramebuffer(defaultFramebuffer);
		boldFont->OnInit(
			FontRoot,
			static_cast<FT_UInt>(18 * scale)
		);

		outlineFont = new OpenGLFont();
		outlineFont->SetDefaultFramebuffer(defaultFramebuffer);
		outlineFont->OnInit(
			FontRoot,
			static_cast<FT_UInt>(18 * scale),
			3.75f
		);

		boldOutlineFont = new OpenGLFont(true);
		boldOutlineFont->SetDefaultFramebuffer(defaultFramebuffer);
		boldOutlineFont->OnInit(
			FontRoot,
			static_cast<FT_UInt>(18 * scale),
			3.75f
		);
	}

	if (font) {
		const auto &black = albumArt->GetBlackColor();

		elapsedText.OnInit(font, context);
		elapsedOutline.OnInit(outlineFont, context);
		remainingText.OnInit(font, context);
		remainingOutline.OnInit(outlineFont, context);
		titleText.OnInit(font, context);
		titleOutline.OnInit(outlineFont, context);
		artistText.OnInit(font, context);
		artistOutline.OnInit(outlineFont, context);
		artistOutline.SetColor({ black, black, black });
		albumText.OnInit(font, context);
		albumOutline.OnInit(outlineFont, context);
		albumOutline.SetColor({ black, black, black });
		fpsCounter.OnInit(font, outlineFont, context);
		exclusiveIndicator.OnInit(font, outlineFont, context);
		volume.OnInit(FontRoot, context);
		presetText.OnInit(font, context);
		presetOutline.OnInit(outlineFont, context);
	} else {
		LogError("Could not open font!");
	}
}

void Controls::OnRadiusChanged(Context &context) {
	OnRadiusChanged(context, miniPlayer);
}

void Controls::OnRadiusChanged(Context &context, bool miniPlayer) {
	const auto radius = albumArt->GetRadius(miniPlayer);
	const auto maxWidth =
		miniPlayer ? 2 * std::sqrt(std::pow(radius, 2) - std::pow(radius / 2 + font->GetEm().height + ScrollingText::BleedEdge * (radius / AlbumArt::BaseRadius), 2)) : windowWidth;

	context.With("scrolling"_hash, [&](Context::Shader &shader) {
		shader.program.Uniform1f("maxWidth"_hash, miniPlayer ? maxWidth : windowWidth);
	});

	artistText.SetMaxWidth(maxWidth);
	artistOutline.SetMaxWidth(maxWidth);
	albumText.SetMaxWidth(maxWidth);
	albumOutline.SetMaxWidth(maxWidth);

	presetText.SetMaxWidth(maxWidth);
	presetOutline.SetMaxWidth(maxWidth);

	const auto size = GetIconSize();

	pause.OnResize(size);
	play.OnResize(size);
	next.OnResize(size);
	previous.OnResize(size);
	captureCheckbox.OnResize(size);

	if (exclusiveIndicator.IsExclusive())
		volume.SetRadius(radius / scale);

	playlist.SetMiniPlayer(miniPlayer);
	playlist.OnRadiusChanged();
	presetList.SetMiniPlayer(miniPlayer);
	presetList.OnRadiusChanged();
	help.SetMiniPlayer(miniPlayer);
	help.OnRadiusChanged();
}

void Controls::OnPresetsChanged(const std::vector<Preset> &presets) {
	presetList.Clear();
	for (const auto &[i, preset] : Utils::Enumerate(presets)) {
		if (!preset.GetPulseBackground() &&
			preset.GetAvailableInMiniPlayer())
			presetList.AddItem(preset.GetName(), i);
	}
}

void Controls::OnPresetChanged(const std::vector<Preset> &presets) {
	if (auto presetIndex = Settings::settings.GetMiniPlayerPresetIndex()) {
		const auto &name = presets.at(*presetIndex).GetName();

		presetText.SetText(name);
		presetOutline.SetText(name);
	}
}

void Controls::OnInit(int windowWidth, int windowHeight, Context &context, float scale, GLuint defaultFramebuffer, bool miniPlayer) {
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;
	this->scale = scale;
	this->miniPlayer = miniPlayer;

	waitTime = miniPlayer ? Settings::settings.GetMiniPlayerWaitTime() : Settings::settings.GetWaitTime();

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

	albumArt->OnInit(windowWidth, windowHeight, scale);

	playlist.OnInit(windowWidth, windowHeight, font, boldFont, outlineFont, boldOutlineFont, &context, scale);
	presetList.OnInit(font, boldFont, outlineFont, boldOutlineFont, &context);
	help.OnInit(font, boldFont, outlineFont, boldOutlineFont, &context);

	// Populate preset list
	const auto &presets = Preset::GetPresets();

	OnPresetsChanged(presets);

	OnPresetChanged(presets);

	albumArt->AddColorChangeListener(&volume);
	albumArt->AddColorChangeListener(&pause);
	albumArt->AddColorChangeListener(&play);
	albumArt->AddColorChangeListener(&next);
	albumArt->AddColorChangeListener(&previous);
	albumArt->AddColorChangeListener(&captureCheckbox);

	const auto size = GetIconSize();

	pause.OnInit(size);
	play.OnInit(size);
	next.OnInit(size);
	previous.OnInit(size);
	captureCheckbox.OnInit(font, outlineFont, size, context);
	captureCheckbox.SetChecked(Settings::settings.GetCaptureKeyboardMediaKeys());

	captureCheckbox.SetTooltip("Capture keyboard media keys?");

	OnRadiusChanged(context);
	OnBlackChanged(albumArt->GetBlackColor());

	// Set alpha to 1 if not in mini-player
	if (!miniPlayer) alpha = 1.0f;
}

void Controls::SetMiniPlayer(Context &context, bool miniPlayer) {
	this->miniPlayer = miniPlayer;

	exclusiveIndicator.SetMiniPlayer(miniPlayer);

	waitTime = miniPlayer ? Settings::settings.GetMiniPlayerWaitTime() : Settings::settings.GetWaitTime();

	if (!miniPlayer) {
		Unstick();
		alpha = 1.0f;
	}

	const auto size = GetIconSize();

	pause.OnResize(size);
	play.OnResize(size);
	next.OnResize(size);
	previous.OnResize(size);
	captureCheckbox.OnResize(size);

	OnRadiusChanged(context);
}

void Controls::OnResize(int windowWidth, int windowHeight, Context &context, float scale, GLuint defaultFramebuffer, bool miniPlayer) {
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;

	const auto radius = albumArt->GetRadius(miniPlayer);

	if (scale != this->scale) {
		// Note: albumArt's radius _must_ be updated
		//       first.
		albumArt->OnResize(windowWidth, windowHeight, scale);

		volume.SetRadius(radius);
	}

	if (scale != this->scale || miniPlayer != this->miniPlayer) {
		// UpdateFontSize() is dependent on scale
		if (scale != this->scale) {
			this->scale = scale;
			UpdateFontSize();
		}
		SetMiniPlayer(context, miniPlayer);
	}

	const auto maxWidth = 
		2 * 
		std::sqrt(
			std::pow(
				radius,
				2
			) - 
			std::pow(
				radius / 2 + font->GetEm().height + ScrollingText::BleedEdge * (radius / AlbumArt::BaseRadius),
				2
			)
		);

	context.With("scrolling"_hash, [&](Context::Shader &shader) {
		shader.program.Uniform1f("maxWidth"_hash, miniPlayer ? maxWidth : windowWidth);
	});

	playlist.OnResize(windowWidth, windowHeight, scale, miniPlayer, maxWidth);

	presetList.SetMiniPlayer(miniPlayer);
	presetList.OnResize(windowWidth, windowHeight);
	presetList.SetMaxWidth(maxWidth);

	help.SetMiniPlayer(miniPlayer);
	help.OnResize(windowWidth, windowHeight);
	help.SetMaxWidth(maxWidth);

	artistText.OnResize(windowWidth, windowHeight);
	artistOutline.OnResize(windowWidth, windowHeight);
	albumText.OnResize(windowWidth, windowHeight);
	albumOutline.OnResize(windowWidth, windowHeight);

	presetText.OnResize(windowWidth, windowHeight);
	presetOutline.OnResize(windowWidth, windowHeight);

	help.AddArrow(
		{ windowWidth / 2, windowHeight / 2 - radius - font->GetEm().height },
		{ windowWidth / 2, windowHeight / 2 - radius / 2 - font->GetEm().height },
		{ "Hover for playlist" }
	);

	help.AddArrow(
		{ windowWidth / 2, windowHeight / 2 + radius + font->GetEm().height },
		{ windowWidth / 2, windowHeight / 2 + radius - font->GetEm().height },
		{ "Hover for visualizer styles" }
	);

	help.AddArrow(
		{ 
			windowWidth / 2 - cos(0.5) * radius * 1.75f,
			windowHeight / 2 - sin(0.5) * radius * 1.75f
		},
		{ 
			windowWidth / 2 - cos(0.5) * (radius + albumArt->GetOutline().GetWidth()),
			windowHeight / 2 - sin(0.5) * (radius + albumArt->GetOutline().GetWidth())
		},
		{ "Hover, click, and drag to resize album art" }
	);

	help.AddArrow(
		{
			windowWidth / 2 + cos(0.5) * ((radius * Settings::settings.GetMiniPlayerVisualizerRatio() / 3) + albumArt->GetOutline().GetWidth()),
			windowHeight / 2 + sin(0.5) * ((radius * Settings::settings.GetMiniPlayerVisualizerRatio() / 3) + albumArt->GetOutline().GetWidth())
		},
		{ 
			windowWidth / 2 + cos(0.5) * (radius * Settings::settings.GetMiniPlayerVisualizerRatio() / 2 - albumArt->GetOutline().GetWidth()),
			windowHeight / 2 + sin(0.5) * (radius * Settings::settings.GetMiniPlayerVisualizerRatio() / 2 - albumArt->GetOutline().GetWidth())
		},
		{ "Hover, click, and drag to resize visualizer" }
	);

	help.AddArrow(
		{ 
			windowWidth / 2 - cos(0.5) * radius * 2.1f,
			windowHeight / 2
		},
		{ 
			windowWidth / 2 - (radius * MiniPlayerSeekbarRatio) / 2.0f - exclusiveIndicator.GetOutlineWidth() * 1.05f - exclusiveIndicator.GetWidth() / 2,
			windowHeight / 2 + font->GetEm().height
		},
		{ "Click to toggle exclusive output" }
	);

	help.AddArrow(
		{
			windowWidth / 2 + cos(0.5) * radius * 2.1f,
			windowHeight / 2
		},
		{
			windowWidth / 2 + (radius * MiniPlayerSeekbarRatio) / 2.0f + captureCheckbox.GetSize().x * 2,
			windowHeight / 2 + font->GetEm().height
		},
		{ "Click to toggle capturing", "of keyboard media keys" }
	);

	const auto closeSize = GetIconSize() / Close::GetLowestRatio();
	help.AddArrow(
		{ 
			windowWidth / 2 + radius * 1.5f,
			windowHeight / 2 - radius * 1.5f
		},
		{ 
			windowWidth / 2 + radius + closeSize / 2,
			windowHeight / 2 - radius - closeSize / 2, 
		},
		{ "Click to close" }
	);

	if (!Settings::settings.GetHelpDismissed()) {
		help.AddArrow(
			{
				windowWidth / 2 + radius * 2,
				windowHeight / 2 + radius / 2.25f
			},
			{
				windowWidth / 2.0f + radius * MiniPlayerIconRatio * 4.25f,
				(windowHeight / 2.0f + (SeekbarSize + radius * MiniPlayerIconRatio * 2.0f)) - help.GetPrompt().GetBounds().height / 2 + radius * MiniPlayerIconRatio / 4
			},
			{ "Click to dismiss", "Hover to show again" }
		);
	}
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
	auto title = std::string(id3->title, id3->title + 30);

	// Sometimes ID3v1 tags are filled with
	// whitespace instead of nulls
	Utils::ltrim(title);
	Utils::rtrim(title);

	if (title.empty() && id3->title[0] != '\0') {
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

double Controls::OnLoop(const Delta &time, HSTREAM streamHandle, Context &context, const Colour<float> &color, bool playing) {
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
		context.Color(albumArt->GetBlackColor(), albumArt->GetBlackColor(), albumArt->GetBlackColor(), alpha);
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
		const auto alpha = miniPlayer ? std::clamp(
			std::min(
				std::min(
					AutoFader<true>::alpha - playlist.GetMiniPlayerAlpha(),
					AutoFader<true>::alpha - presetList.GetAlpha()
				),
				AutoFader<true>::alpha - volume.GetAlpha()
			),
			0.0f,
			1.0f
		) : AutoFader<true>::alpha;

		const float inverseAlpha = AutoFader::alpha - volume.GetAlpha();

		const auto &cue = playlist.GetCue();

		currentPos = BASS_ChannelBytes2Seconds(
			streamHandle,
			BASS_ChannelGetPosition(streamHandle, BASS_POS_BYTE)
		);

		if (cue)
			currentPos -= cue->GetCurrentTrack()->startTime;

		const auto SeekbarSize = Controls::SeekbarSize * (miniPlayer ? (albumArt->GetRadius(miniPlayer) / scale / AlbumArt::BaseRadius) : 1.0f);

		// Only update if BASS didn't error out
		float pixels = 0.0f;
		if (currentPos != -1) {
			pixels = static_cast<float>((currentPos / GetCurrentSongLength()) * (miniPlayer ? albumArt->GetRadius(miniPlayer) * MiniPlayerSeekbarRatio : windowWidth));

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
				SeekbarSize / (miniPlayer ? 1.0f : 5.0f) * scale,
				miniPlayer ? albumArt->GetRadius(miniPlayer) * MiniPlayerSeekbarRatio : pixels,
				SeekbarSize / (miniPlayer ? 1.0f : 5.0f) * scale,
				miniPlayer ? albumArt->GetRadius(miniPlayer) * MiniPlayerSeekbarRatio : pixels,
				0
			};

			sepVbo->Bind();
			sepVbo->BufferData(posRect, GL_DYNAMIC_DRAW);
			sepVbo->Unbind();
		}

		context.Use("basic"_hash);
		context.Translate(
			miniPlayer ? windowWidth / 2 - (albumArt->GetRadius(miniPlayer) * MiniPlayerSeekbarRatio / 2) : 0.0f,
			(miniPlayer ? windowHeight / 2 + font->GetEm().height - SeekbarSize / 2 * scale : (context.GetSafeArea().h + context.GetSafeArea().y) - SeekbarSize * scale),
			0.0f
		);
		context.Apply();

		const auto &black = albumArt->GetBlackColor();
		const auto &tintedBlack = albumArt->GetTintedBlackColor();
		if (miniPlayer) {
			context.Color(tintedBlack.r, tintedBlack.g, tintedBlack.b, alpha * 0.667f);
			
			sepVao->Bind();
			sepEab->Bind();
			context.Blend(alpha > 0.0f, [&] {
				sepEab->DrawElements(GL_TRIANGLES);
			});
			sepEab->Unbind();
			sepVao->Unbind();
		}

		context.Color(color.r, color.g, color.b, alpha);

		vao->Bind();
		eab->Bind();
		context.Blend(miniPlayer && alpha > 0.0f, [&] {
			eab->DrawElements(GL_TRIANGLES);
		});
		eab->Unbind();
		vao->Unbind();

		if (!miniPlayer) {
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
		}

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

			if (miniPlayer) context.StartBlend();

			context.Color(1.0f, 1.0f, 1.0f, alpha);
			for (const auto *text : { &elapsedOutline, &elapsedText }) {
				text->OnLoop(
					miniPlayer ? 
						windowWidth / 2 - (albumArt->GetRadius(miniPlayer) * MiniPlayerSeekbarRatio / 2) :
						std::min(
							std::max(
								margin,
								static_cast<int>(pixels - elapsedText.GetSize().x / 2.0f)
							),
							windowWidth - elapsedText.GetSize().x - margin
						),
					miniPlayer ?
						windowHeight / 2 - outlineFont->GetOutlineRadius() * 2:
						(context.GetSafeArea().h + context.GetSafeArea().y) - yOffset - elapsedText.GetSize().y - outlineFont->GetOutlineRadius()
				);
				context.Color(1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, alpha);
			}

			context.Color(1.0f, 1.0f, 1.0f, alpha);
			for (const auto *text : { &remainingOutline, &remainingText }) {
				text->OnLoop(
					miniPlayer ? 
						windowWidth / 2 + (albumArt->GetRadius(miniPlayer) * MiniPlayerSeekbarRatio / 2) - remainingText.GetSize().x :
						windowWidth - remainingText.GetSize().x - margin,
					miniPlayer ?
						windowHeight / 2 - outlineFont->GetOutlineRadius() * 2:
						(context.GetSafeArea().h + context.GetSafeArea().y) - yOffset / 2 - remainingText.GetSize().y / 2 + outlineFont->GetOutlineRadius()
				);
				context.Color(1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, alpha);
			}

			if (miniPlayer) context.EndBlend();

			iconY = windowHeight / 2.0f + (miniPlayer ? SeekbarSize + albumArt->GetRadius(miniPlayer) * MiniPlayerIconRatio * 2.0f : 0.0f);

			if (!miniPlayer) {
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
						(context.GetSafeArea().h + context.GetSafeArea().y) - (yOffset + albumText.GetBounds().height),
						time
					);
					context.Color(1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, alpha);
					albumText.OnLoop(
						margin + xOffset,
						(context.GetSafeArea().h + context.GetSafeArea().y) - (yOffset += albumText.GetBounds().height),
						time
					);
				}
				if (!artistText.Empty()) {
					context.Color(0.0f, 0.0f, 0.0f, alpha);
					artistOutline.OnLoop(
						margin + xOffset,
						(context.GetSafeArea().h + context.GetSafeArea().y) - (yOffset + artistText.GetBounds().height),
						time
					);
					context.Color(1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, alpha);
					artistText.OnLoop(
						margin + xOffset,
						(context.GetSafeArea().h + context.GetSafeArea().y) - (yOffset += artistText.GetBounds().height),
						time
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

				playlist.OnLoop(
					time,
					fpsCounter.GetSize(),
					aboveMetadata - albumHeight / 8.0f + (context.GetYOffset() - context.GetSafeArea().y) - exclusiveIndicator.GetHeight() / 2,
					alpha,
					context
				);

				exclusiveIndicator.OnLoop(margin, aboveMetadata, alpha, albumArt, context);
			} else {
				context.Use("scrolling"_hash);
				context.GetShaderProgram().Uniform2f("screenSize"_hash, windowWidth, windowHeight);

				if (!artistText.Empty()) {
					context.Color(1.0f, 1.0f, 1.0f, alpha);
					artistOutline.OnLoop(
						windowWidth / 2 - artistText.GetBounds().width / 2,
						windowHeight / 2 - albumText.GetBounds().height - artistText.GetBounds().height * 2,
						time
					);
					context.Color(1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, alpha);
					artistText.OnLoop(
						windowWidth / 2 - artistText.GetBounds().width / 2,
						windowHeight / 2 - albumText.GetBounds().height - artistText.GetBounds().height * 2,
						time
					);
				}

				if (!albumText.Empty()) {
					context.Color(1.0f, 1.0f, 1.0f, alpha);
					albumOutline.OnLoop(
						windowWidth / 2 - albumText.GetBounds().width / 2,
						windowHeight / 2 - albumText.GetBounds().height - artistText.GetBounds().height,
						time
					);
					context.Color(1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, alpha);
					albumText.OnLoop(
						windowWidth / 2 - albumText.GetBounds().width / 2,
						windowHeight / 2 - albumText.GetBounds().height - artistText.GetBounds().height,
						time
					);
				}

				context.Color(1.0f, 1.0f, 1.0f, presetList.GetAlpha() > 0.0f ? volume.GetAlpha() == 0.0f ? 1.0f : inverseAlpha : alpha);
				presetOutline.OnLoop(
					windowWidth / 2 - presetText.GetBounds().width / 2,
					iconY + albumArt->GetRadius(miniPlayer) * MiniPlayerIconRatio * 1.5f,
					time
				);
				context.Color(1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, 1.0f * HDR::WhiteLevel, presetList.GetAlpha() > 0.0f ? volume.GetAlpha() == 0.0f ? 1.0f : inverseAlpha : alpha);
				presetText.OnLoop(
					windowWidth / 2 - presetText.GetBounds().width / 2,
					iconY + albumArt->GetRadius(miniPlayer) * MiniPlayerIconRatio * 1.5f,
					time
				);

				context.Use("texture"_hash);
				exclusiveIndicator.OnLoop(
					windowWidth / 2 - (albumArt->GetRadius(miniPlayer) * MiniPlayerSeekbarRatio) / 2.0f - exclusiveIndicator.GetOutlineWidth() * 1.05f,
					windowHeight / 2 + font->GetEm().height,
					alpha,
					albumArt,
					context
				);
			}

			if (miniPlayer)
				context.StartBlend();

			if ((playing || pause.IsClicked()) && !play.IsClicked()) {
				pause.OnLoop(
					windowWidth / 2.0f,
					iconY,
					time,
					context,
					miniPlayer ? &alpha : nullptr
				);
			} else {
				play.OnLoop(
					windowWidth / 2.0f,
					iconY,
					time,
					context,
					miniPlayer ? &alpha : nullptr
				);
			}
			next.OnLoop(
				(miniPlayer ? (windowWidth / 2.0f + albumArt->GetRadius(miniPlayer) * MiniPlayerIconRatio * 2) : (windowWidth - albumArt->GetRadius(miniPlayer))),
				iconY,
				time,
				context,
				miniPlayer ? &alpha : nullptr
			);
			previous.OnLoop(
				(miniPlayer ? (windowWidth / 2.0f - albumArt->GetRadius(miniPlayer) * MiniPlayerIconRatio * 2) : (albumArt->GetRadius(miniPlayer))),
				iconY,
				time, 
				context,
				miniPlayer ? &alpha : nullptr
			);

			if (miniPlayer) {
				captureCheckbox.OnLoop(
					windowWidth / 2 + (albumArt->GetRadius(miniPlayer) * MiniPlayerSeekbarRatio) / 2.0f + captureCheckbox.GetSize().x,
					windowHeight / 2 + font->GetEm().height,
					time,
					context,
					miniPlayer ? &alpha : nullptr
				);

				context.EndBlend();

				if (presetList.GetAlpha() != 1.0f && volume.GetAlpha() != 1.0f) {
					playlist.OnLoop(
						time,
						{ windowWidth / 2, windowHeight / 2 - albumArt->GetRadius(miniPlayer) / 2 },
						font->GetEm().height,
						// FIXME: this is a ternary mess
						volume.GetAlpha() > 0.0f ?
							AutoFader::alpha == 0.0f ?
								0.5f - volume.GetAlpha() :
								inverseAlpha :
								presetList.GetAlpha() > 0.0f ?
									alpha :
									AutoFader<true>::alpha,
						context,
						true,
						presetList.GetAlpha() > 0.0f || volume.GetAlpha() > 0.0f
					);
				}

				context.Use("scrolling"_hash);

				presetList.PreLoop();

				if (presetList.GetAlpha() > 0.0f) {
					presetList.OnLoop(
						time,
						{ windowWidth / 2, windowHeight / 2 - albumArt->GetRadius(miniPlayer) / 2 },
						*Settings::settings.GetMiniPlayerPresetIndex(),
						volume.GetAlpha() == 0.0f ? presetList.GetAlpha() : inverseAlpha
					);
				}

				presetList.PostLoop(time);

				context.Use("texture"_hash);
				context.Color(1.0f, 1.0f, 1.0f, alpha);
				const auto closeSize = GetIconSize() / Close::GetLowestRatio();
				
				help.OnLoop(
					time, 
					{ 
						windowWidth / 2.0f + albumArt->GetRadius(miniPlayer) * MiniPlayerIconRatio * 3.75f,
						iconY - help.GetPrompt().GetBounds().height / 2 + albumArt->GetRadius(miniPlayer) * MiniPlayerIconRatio / 4
					},
					context
				);
				
			}

#ifdef WIN32
			// Only render our volume if we're in exclusive mode
			if (exclusiveIndicator.IsExclusive()) {
				context.Use("texture"_hash);
				volume.OnLoop(windowWidth / 2, windowHeight / 2, time, albumArt, miniPlayer, context);
			}
#endif
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
	presetList.OnDestroy();
	help.OnDestroy();

	pause.OnDestroy();
	play.OnDestroy();
	next.OnDestroy();
	previous.OnDestroy();
	captureCheckbox.OnDestroy();

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
	presetText.OnDestroy();
	presetOutline.OnDestroy();

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

Controls::ControlButton Controls::GetButtonAtPos(const Vector2i &pos) {
	if (!miniPlayer) return ControlButton::None;

	const auto iconSize = GetIconSize();

	// Don't allow interaction if the volume controls
	// are active in the mini-player
	if (((exclusiveIndicator.IsExclusive() && volume.GetAlpha() == 0.0f) || 
		(!exclusiveIndicator.IsExclusive())) &&
		pos.y >= iconY - iconSize / 2 &&
		pos.y <= iconY + iconSize / 2) {
		if (auto previousX = windowWidth / 2.0f - iconSize * 2;
			pos.x >= previousX - iconSize &&
			pos.x <= previousX + iconSize / 2
			) {
			return ControlButton::Previous;
		} else if (auto nextX = windowWidth / 2.0f + iconSize * 2;
			pos.x >= nextX - iconSize / 2 &&
			pos.x <= nextX + iconSize) {
			return ControlButton::Next;
		} else if (auto playPauseX = windowWidth / 2.0f;
			pos.x >= playPauseX - iconSize / 2 &&
			pos.x <= playPauseX + iconSize / 2) {
			return ControlButton::PlayPause;
		}
	} else if (
		pos.y >= windowHeight / 2 + font->GetEm().height - captureCheckbox.GetSize().y / 2 &&
		pos.y <= windowHeight / 2 + font->GetEm().height + captureCheckbox.GetSize().y / 2 &&
		pos.x >= windowWidth / 2 + (albumArt->GetRadius(miniPlayer) * MiniPlayerSeekbarRatio) / 2.0f + captureCheckbox.GetSize().x / 2 &&
		pos.x <= windowWidth / 2 + (albumArt->GetRadius(miniPlayer) * MiniPlayerSeekbarRatio) / 2.0f + captureCheckbox.GetSize().x * 1.5f)
		return ControlButton::CaptureCheckbox;

	return ControlButton::None;
}

void Controls::OnMouseMoved(const Vector2i &mousePos) {
	// Don't allow interaction if Help is visible
	auto button = help.IsHovered() ? ControlButton::None : GetButtonAtPos(mousePos);

	if (!help.IsHovered() && playlist.OnMouseMoved(mousePos))
		button = ControlButton::None;
	else if (!help.IsHovered() && presetList.OnMouseMoved(
		mousePos,
		{
			windowWidth / 2 - presetText.GetBounds().width / 2,
			static_cast<int>(iconY + albumArt->GetRadius(miniPlayer) * MiniPlayerIconRatio * 1.5f - presetText.GetBounds().height / 2.0f),
			windowWidth / 2 + presetText.GetBounds().width / 2,
			static_cast<int>(iconY + albumArt->GetRadius(miniPlayer) * MiniPlayerIconRatio * 1.5f + presetText.GetBounds().height / 2.0f)
		}
	))
		button = ControlButton::None;
	else if (help.OnMouseMoved(mousePos))
		button = ControlButton::None;

	next.SetHovered(button == ControlButton::Next);
	previous.SetHovered(button == ControlButton::Previous);
	play.SetHovered(button == ControlButton::PlayPause);
	pause.SetHovered(button == ControlButton::PlayPause);
	captureCheckbox.SetHovered(button == ControlButton::CaptureCheckbox);
	captureCheckbox.SetMousePos(mousePos);
}

Controls::ControlButton Controls::OnMouseClicked(const Vector2i &mousePos, std::function<void(float)> seekCallback, bool playing, bool canTakeAction) {
	// Don't allow interaction if Help is visible
	if (help.IsHovered()) {
		if (!Settings::settings.GetHelpDismissed() && help.OnMouseClicked(mousePos)) {
			help.SetHovered(false, false, false, [this] {
				help.RemoveArrow("Click to dismiss");
			});

			Settings::settings.SetHelpDismissed(true);
		}

		return ControlButton::None;
	}

	const auto SeekbarSize = Controls::SeekbarSize * (miniPlayer ? (albumArt->GetRadius(miniPlayer) / AlbumArt::BaseRadius) : 1.0f);
	const auto seekbarPos = windowHeight / 2 + font->GetEm().height - SeekbarSize / 2 * scale;

	if (miniPlayer && playlist.GetMiniPlayerAlpha() == 0.0f && presetList.GetAlpha() == 0.0f) {
		if (auto ret = GetButtonAtPos(mousePos); ret != ControlButton::None) {
			if (ret == ControlButton::Previous) {
				LogInfo("Click captured! Previous button.");
				previous.SetClicked(true);
			} else if (ret == ControlButton::Next) {
				LogInfo("Click captured! Next button.");
				next.SetClicked(true);
			} else if (ret == ControlButton::PlayPause) {
				LogInfo("Click captured! Play/pause button.");
				if (!playing) play.SetClicked(true);
				else pause.SetClicked(true);
			} else if (ret == ControlButton::CaptureCheckbox) {
				LogInfo("Click captured! Capture checkbox.");
				if (canTakeAction) {
					captureCheckbox.SetClicked(true);

					const auto capture = !captureCheckbox.GetChecked();

					Settings::settings.SetCaptureKeyboardMediaKeys(capture);
					captureCheckbox.SetChecked(capture);
				}
			}

			return ret;
		} else if (mousePos.y >= seekbarPos &&
			mousePos.y <= seekbarPos + SeekbarSize * scale) { // Seekbar

			float pos = mousePos.x - (windowWidth / 2 - (albumArt->GetRadius(miniPlayer) * MiniPlayerSeekbarRatio / 2));
			pos /= (albumArt->GetRadius(miniPlayer) * MiniPlayerSeekbarRatio);

			if (pos >= 0.0f && pos <= 1.0f) {
				seekCallback(pos);

				LogInfo("Click captured! Seekbar, ", pos * 100, "%");
			}
		}
	}

	return ControlButton::None;
}

void Controls::AddToScrollOffset(int offset) {
	playlist.AddToScrollOffset(offset);
	presetList.AddToScrollOffset(offset);
	help.AddToScrollOffset(offset);
}

void Controls::OnBlackChanged(const float &black) {
	const Colourf color = { 
		black,
		black,
		black
	};

	elapsedOutline.SetColor(color);
	remainingOutline.SetColor(color);
	artistOutline.SetColor(color);
	albumOutline.SetColor(color);
	presetOutline.SetColor(color);

	artistOutline.SetText(artistOutline.GetText(), true, false /* don't update scrolling */);
	albumOutline.SetText(albumOutline.GetText(), true, false /* don't update scrolling */);
	presetOutline.SetText(presetOutline.GetText(), true, false /* don't update scrolling */);
}

float Controls::UpdateFontSize(std::optional<float> radius) {
	if (!radius)
		radius = miniPlayer ? Settings::settings.GetMiniPlayerRadius() : Settings::settings.GetRadius();

	const auto newFontSize = miniPlayer ? std::lround(20 / (AlbumArt::BaseRadius / *radius)) * scale : 18.0f * scale;
	const auto newOutlineSize = miniPlayer ? 4.75f / (AlbumArt::BaseRadius / *radius) * scale : 3.0f * scale;

	font->SetFontSize(newFontSize);
	boldFont->SetFontSize(newFontSize);
	outlineFont->SetOutlineRadius(newOutlineSize);
	outlineFont->SetFontSize(newFontSize);
	boldOutlineFont->SetOutlineRadius(newOutlineSize);
	boldOutlineFont->SetFontSize(newFontSize);

	if (miniPlayer)
		Settings::settings.SetMiniPlayerFontSize(newFontSize / scale, true);

	return newFontSize;
}