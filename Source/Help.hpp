#pragma once

#include "OpenGL/Polyline.hpp"

#include "MiniPlayerList.hpp"

class Arrow : public LoggableClass {
private:
	constexpr static float LineWidth = 3.5f;
	constexpr static float OutlineWidth = 4.5f;
	constexpr static int ArrowLength = 25;

public:
	void OnInit(
		OpenGLFont *font,
		OpenGLFont *boldFont,
		OpenGLFont *outlineFont,
		OpenGLFont *boldOutlineFont,
		Context &context,
		Vector2f start,
		Vector2f end,
		const std::vector<std::string> &lines,
		float scale = 1.0f,
		bool bold = false) {
		this->font = font;
		this->bold = bold;

		Update(start, end, scale);

		for (const auto &[i, text] : Utils::Enumerate(lines)) {
			Text prompt;
			Text promptOutline;

			prompt.OnInit(((bold && i == 0) ? boldFont : font), &context);
			promptOutline.OnInit(((bold && i == 0) ? boldOutlineFont : outlineFont), &context);
			prompt.SetText(text);
			promptOutline.SetText(text);

			prompts.emplace_back(std::move(prompt));
			promptOutlines.emplace_back(std::move(promptOutline));
		}
	}

	void OnDestroy() {
		for (auto &prompt : prompts)
			prompt.OnDestroy();

		for (auto &promptOutline : promptOutlines)
			promptOutline.OnDestroy();

		outline.OnDestroy();
		arrowOutline.OnDestroy();

		line.OnDestroy();
		arrow.OnDestroy();
	}

	void OnLoop(Context &context, float alpha) const {
		context.Use("basic"_hash);

		context.Color(0.0f, 0.0f, 0.0f, alpha);
		outline.Draw<false>(context);
		arrowOutline.Draw<false>(context);

		context.Color(HDR::WhiteLevel, HDR::WhiteLevel, HDR::WhiteLevel, alpha);
		line.Draw<false>(context);
		arrow.Draw<false>(context);

		context.Use("texture"_hash);
		context.Color(0.0f, 0.0f, 0.0f, alpha);
		for (const auto &[i, promptOutline] : Utils::Enumerate(promptOutlines)) {
			const auto offset = static_cast<int>(promptOutlines.size() - i);
			const auto inverse = static_cast<int>(promptOutlines.size() - offset);

			promptOutline.OnLoop(
				start.x - prompts[i].GetBounds().width / 2,
				start.y - (angle > 0 ? font->GetEm().height * offset : -font->GetEm().height / 2 - (font->GetEm().height * inverse))
			);
		}

		context.Color(HDR::WhiteLevel, HDR::WhiteLevel, HDR::WhiteLevel, alpha);
		for (const auto &[i, prompt] : Utils::Enumerate(prompts)) {
			const auto offset = static_cast<int>(prompts.size() - i);
			const auto inverse = static_cast<int>(prompts.size() - offset);

			prompt.OnLoop(
				start.x - prompt.GetBounds().width / 2,
				start.y - (angle > 0 ? font->GetEm().height * offset : -font->GetEm().height / 2 - (font->GetEm().height * inverse))
			);
		}
	}

	void Update(Vector2f start, Vector2f end, float scale = 1.0f) {
		this->start = start;

		std::vector<Vector2f> points = {
			start,
			end
		};

		line.SetWidth(LineWidth * (bold ? 2.0f : 1.0f) * scale);
		line.SetPoints<Polyline::Join::None>(points.data(), points.size());

		angle = std::atan2((end.y - start.y), (end.x - start.x));
		const auto backward = (start - end).Normalize();

		const auto arrowAngle = Maths::PI<float> / 4.0f;

		const auto ax = backward.x * std::cos(arrowAngle) - backward.y * std::sin(arrowAngle);
		const auto ay = backward.x * std::sin(arrowAngle) + backward.y * std::cos(arrowAngle);
		const auto bx = backward.x * std::cos(arrowAngle) + backward.y * std::sin(arrowAngle);
		const auto by = -backward.x * std::sin(arrowAngle) + backward.y * std::cos(arrowAngle);

		outline.SetWidth(line.GetWidth() * OutlineWidth / (bold ? 1.5f : 1.0f));

		// Extend line's outline past the start
		points.begin()->x = start.x - cos(angle) * outline.GetWidth() / OutlineWidth;
		points.begin()->y = start.y - sin(angle) * outline.GetWidth() / OutlineWidth;

		outline.SetPoints<Polyline::Join::None>(points.data(), points.size());

		points = {
			{end.x + ArrowLength * scale * (bold ? 1.5f : 1.0f) * ax, end.y + ArrowLength * scale * (bold ? 1.5f : 1.0f) * ay},
			end,
			{end.x + ArrowLength * scale * (bold ? 1.5f : 1.0f) * bx, end.y + ArrowLength * scale * (bold ? 1.5f : 1.0f) * by}
		};

		arrow.SetWidth(line.GetWidth());
		arrow.SetPoints<Polyline::Join::Miter>(points.data(), points.size());

		// Extend arrow's outline
		points.begin()->x = end.x + (ArrowLength * scale * (bold ? 1.5f : 1.0f) + outline.GetWidth() / OutlineWidth) * ax;
		points.begin()->y = end.y + (ArrowLength * scale * (bold ? 1.5f : 1.0f) + outline.GetWidth() / OutlineWidth) * ay;

		points.rbegin()->x = end.x + (ArrowLength * scale * (bold ? 1.5f : 1.0f) + outline.GetWidth() / OutlineWidth) * bx;
		points.rbegin()->y = end.y + (ArrowLength * scale * (bold ? 1.5f : 1.0f) + outline.GetWidth() / OutlineWidth) * by;

		arrowOutline.SetWidth(arrow.GetWidth() * OutlineWidth / (bold ? 1.5f : 1.0f));
		arrowOutline.SetPoints<Polyline::Join::Miter>(points.data(), points.size());
	}

	const bool &IsBold() const { return bold; }
	
	Text &GetPrompt() { return *prompts.begin(); }

private:
	bool bold = false;
	OpenGLFont *font = nullptr;

	float angle = 0.0f;

	Vector2f start;

	std::vector<Text> prompts;
	std::vector<Text> promptOutlines;

	Fetcko::Polyline outline;
	Fetcko::Polyline arrowOutline;
	Fetcko::Polyline line;
	Fetcko::Polyline arrow;
};

class Help : public MiniPlayerList {
public:
	Help(Direction direction, AlbumArt *const albumArt, const bool &vulkan) : MiniPlayerList(direction, albumArt, vulkan) {
	}

	void OnInit(OpenGLFont *font, OpenGLFont *boldFont, OpenGLFont *outlineFont, OpenGLFont *boldOutlineFont, Context *context) {
		const auto outlineColor = albumArt->GetBlackColor();

		MiniPlayerList::OnInit(font, boldFont, outlineFont, boldOutlineFont, context);

		prompt.OnInit(font, context);
		prompt.SetColor(HDR::WhiteColor);
		prompt.SetText("?");

		promptOutline.OnInit(outlineFont, context);
		promptOutline.SetColor({ outlineColor, outlineColor, outlineColor});
		promptOutline.SetText("?");

		for (const auto &[key, action] : std::vector<std::pair<std::string, std::string>>{
			{ "Hotkeys:", "" },
			{ "R", u8" \u2014 Reset this window" },
			{ "M", u8" \u2014 Toggles player and visualizer editor (beta)" },
			{ "V", u8" \u2014 Toggles Vulkan interop (beta)" },
			{ "Scroll Wheel + Shift", u8" \u2014 Adjust audio delay" },
			{ "Scroll Wheel + Control", u8" \u2014 Adjust visualizer line width" },
			{ "Scroll Wheel + Alt", u8" \u2014 Adjust visualizer rotation" }
		}) {
			Text keyText;
			keyText.OnInit(font, context);
			keyText.SetColor(HDR::WhiteColor);
			keyText.SetText(key);

			Text text;
			text.OnInit(font, context);
			text.SetColor(HDR::WhiteColor);
			text.SetText(action);

			hotkeys.emplace_back(std::make_pair(std::move(keyText), std::move(text)));

			Text keyOutline;
			keyOutline.OnInit(outlineFont, context);
			keyOutline.SetColor({ outlineColor, outlineColor, outlineColor });
			keyOutline.SetText(key);

			Text outline;
			outline.OnInit(outlineFont, context);
			outline.SetColor({ outlineColor, outlineColor, outlineColor });
			outline.SetText(action);

			hotkeyOutlines.emplace_back(std::make_pair(std::move(keyOutline), std::move(outline)));
		}

		backdrop.OnInit(albumArt->GetRadius(true) * Settings::settings.GetMiniPlayerVisualizerRatio() / 2 - albumArt->GetOutline().GetWidth());
	}

	bool OnResize(int windowWidth, int windowHeight, float scale, bool miniPlayer, float maxWidth) override {
		const auto ret = MiniPlayerList::OnResize(windowWidth, windowHeight, scale, miniPlayer, maxWidth);

		backdrop.SetRadius(albumArt->GetRadius(true) * Settings::settings.GetMiniPlayerVisualizerRatio() / 2 - albumArt->GetOutline().GetWidth());

		context->With("ring"_hash, [this, windowWidth, windowHeight] (Context::Shader &shader) {
			shader.program.Uniform2f("screenSize"_hash, windowWidth, windowHeight);
			shader.program.Uniform1f("radius"_hash, albumArt->GetRadius(true) + albumArt->GetOutline().GetWidth() / 2);
		});

		return ret;
	}

	void OnLoop(const Delta &time, Vector2i pos, Context &context, float controlAlpha, float maxHeight, float radius) {
		this->pos = pos;

		const auto &color = GetColor();

		if (alpha == 0.0f) {
			PreLoop();

			context.Blend(true, [this, &pos, &context, &controlAlpha, &color] {
				promptOutline.OnLoop(pos.x, pos.y);
				context.Color(
					color.r,
					color.g,
					color.b,
					controlAlpha
				);
				prompt.OnLoop(pos.x, pos.y);
			});

			PostLoop(time);
			return;
		}

		albumArt->OverrideOutlineAlpha(alpha);

		if (albumArt->Loaded()) {
			context.Blend(true, [&] {
				albumArt->DrawPlaceholder(
					windowWidth / 2,
					windowHeight / 2,
					alpha / 1.5f,
					context
				);
			});
		}
		
		context.Use("ring"_hash);
		context.Color(0.0f, 0.0f, 0.0f, alpha * 0.75f);
		backdrop.OnLoop(windowWidth / 2, windowHeight / 2, context);

		context.Use("texture"_hash);
		context.Color(
			color.r,
			color.g,
			color.b,
			1.0f
		);

		context.Blend(true, [this, &pos] {
			promptOutline.OnLoop(pos.x, pos.y);
			prompt.OnLoop(pos.x, pos.y);
		});

		if (const auto hotkeyHeight = hotkeys.size() * font->GetEm().height; hotkeyHeight < maxHeight - font->GetEm().height / 2) {
			context.Color(HDR::WhiteLevel, HDR::WhiteLevel, HDR::WhiteLevel, alpha);
			float yOffset = font->GetEm().height * 2;
			for (std::size_t i = 0; i < hotkeys.size(); ++i) {
				const auto width = hotkeys[i].first.GetBounds().width + hotkeys[i].second.GetBounds().width;

				hotkeyOutlines[i].first.OnLoop(windowWidth / 2 - width / 2, yOffset);
				hotkeyOutlines[i].second.OnLoop(windowWidth / 2 - width / 2 + hotkeyOutlines[i].first.GetBounds().width, yOffset);

				context.Color(hoveredColor.r, hoveredColor.g, hoveredColor.b, alpha);

				hotkeys[i].first.OnLoop(windowWidth / 2 - width / 2, yOffset);

				context.Color(HDR::WhiteLevel, HDR::WhiteLevel, HDR::WhiteLevel, alpha);

				hotkeys[i].second.OnLoop(windowWidth / 2 - width / 2 + hotkeyOutlines[i].first.GetBounds().width, yOffset);

				yOffset += std::max(hotkeys[i].first.GetBounds().height, hotkeys[i].second.GetBounds().height);
			}
		}

		context.Use("basic"_hash);
		
		for (const auto &[text, arrow] : arrows)
			arrow.OnLoop(context, alpha);

		MiniPlayerList::OnLoop(time, pos, std::nullopt);
	}

	void OnDestroy() override {
		MiniPlayerList::OnDestroy();

		backdrop.OnDestroy();
		prompt.OnDestroy();
		promptOutline.OnDestroy();

		for (auto &arrow : arrows)
			arrow.second.OnDestroy();

		for (auto &hotkey : hotkeys) {
			hotkey.first.OnDestroy();
			hotkey.second.OnDestroy();
		}

		for (auto &outline : hotkeyOutlines) {
			outline.first.OnDestroy();
			outline.second.OnDestroy();
		}
	}

	bool OnMouseMoved(const Vector2i &mousePos) {
		return MiniPlayerList::OnMouseMoved(
			mousePos,
			{
				pos.x - promptOutline.GetBounds().width / 2,
				pos.y - promptOutline.GetBounds().height / 2,
				pos.x + promptOutline.GetBounds().width / 2,
				pos.y + promptOutline.GetBounds().height / 2
			},
			true
		) != MiniPlayerList::HoverState::None;
	}

	bool OnMouseClicked(const Vector2i &mousePos) {
		return
			mousePos.x >= pos.x - promptOutline.GetBounds().width / 2 && mousePos.x <= pos.x + promptOutline.GetBounds().width / 2 &&
			mousePos.y >= pos.y - promptOutline.GetBounds().height / 2 && mousePos.y <= pos.y + promptOutline.GetBounds().height / 2;
	}

	void OnColorChanged(const Colour<float> &color, bool silent) override {
		MiniPlayerList::OnColorChanged(color, silent);

		for (auto &[text, arrow] : arrows) {
			if (arrow.IsBold()) {
				arrow.GetPrompt().SetColor(hoveredColor);
				arrow.GetPrompt().SetText(arrow.GetPrompt().GetText(), true);
			}
		}
	}

	void AddArrow(Vector2f start, Vector2f end, const std::string &key, const std::vector<std::string> &lines, float scale = 1.0f, bool bold = false) {
		if (auto iter = arrows.find(key); iter != arrows.end()) {
			iter->second.Update(start, end, scale);
			return;
		}

		Arrow arrow;
		arrow.OnInit(font, boldFont, outlineFont, boldOutlineFont, *context, start, end, lines, scale, bold);
		arrows.emplace(std::make_pair(key, std::move(arrow)));
	}

	void UpdateArrow(const std::string text, Vector2f start, Vector2f end) {
		arrows[text].Update(start, end);
	}

	void RemoveArrow(const std::string &key) {
		if (const auto iter = arrows.find(key); iter != arrows.end()) {
			iter->second.OnDestroy();
			arrows.erase(iter);
		}
	}

	const Text &GetPrompt() const { return prompt; }
private:
	Circle<Circles::Plain> backdrop;

	Text prompt;
	Text promptOutline;

	std::map<std::string, Arrow> arrows;

	std::vector<std::pair<Text, Text>> hotkeys;
	std::vector<std::pair<Text, Text>> hotkeyOutlines;

	Vector2i pos = { 0, 0 };
};