#pragma once

#include "OpenGL/Polyline.hpp"

#include "MiniPlayerList.hpp"

class Arrow : public LoggableClass {
private:
	constexpr static float LineWidth = 2.5f;
	constexpr static float OutlineWidth = 4.0f;
	constexpr static int ArrowLength = 15;

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
		bool bold = false) {
		this->font = font;
		this->bold = bold;

		Update(start, end);

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

		context.Color(1.0f, 1.0f, 1.0f, alpha);
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

		context.Color(1.0f, 1.0f, 1.0f, alpha);
		for (const auto &[i, prompt] : Utils::Enumerate(prompts)) {
			const auto offset = static_cast<int>(prompts.size() - i);
			const auto inverse = static_cast<int>(prompts.size() - offset);

			prompt.OnLoop(
				start.x - prompt.GetBounds().width / 2,
				start.y - (angle > 0 ? font->GetEm().height * offset : -font->GetEm().height / 2 - (font->GetEm().height * inverse))
			);
		}
	}

	void Update(Vector2f start, Vector2f end) {
		this->start = start;

		std::vector<Vector2f> points = {
			start,
			end
		};

		line.SetWidth(LineWidth * (bold ? 2.0f : 1.0f));
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
			{end.x + ArrowLength * (bold ? 1.5f : 1.0f) * ax, end.y + ArrowLength * (bold ? 1.5f : 1.0f) * ay},
			end,
			{end.x + ArrowLength * (bold ? 1.5f : 1.0f) * bx, end.y + ArrowLength * (bold ? 1.5f : 1.0f) * by}
		};

		arrow.SetWidth(line.GetWidth());
		arrow.SetPoints<Polyline::Join::Miter>(points.data(), points.size());

		// Extend arrow's outline
		points.begin()->x = end.x + (ArrowLength * (bold ? 1.5f : 1.0f) + outline.GetWidth() / OutlineWidth) * ax;
		points.begin()->y = end.y + (ArrowLength * (bold ? 1.5f : 1.0f) + outline.GetWidth() / OutlineWidth) * ay;

		points.rbegin()->x = end.x + (ArrowLength * (bold ? 1.5f : 1.0f) + outline.GetWidth() / OutlineWidth) * bx;
		points.rbegin()->y = end.y + (ArrowLength * (bold ? 1.5f : 1.0f) + outline.GetWidth() / OutlineWidth) * by;

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
		prompt.SetColor(Colour<float>::White);
		prompt.SetText("?");

		promptOutline.OnInit(outlineFont, context);
		promptOutline.SetColor({ outlineColor, outlineColor, outlineColor});
		promptOutline.SetText("?");

		backdrop.OnInit(albumArt->GetRadius(true) * Settings::settings.GetMiniPlayerVisualizerRatio() / 2 - albumArt->GetOutline().GetWidth());
	}

	void OnResize(int windowWidth, int windowHeight) override {
		MiniPlayerList::OnResize(windowWidth, windowHeight);

		backdrop.SetRadius(albumArt->GetRadius(true) * Settings::settings.GetMiniPlayerVisualizerRatio() / 2 - albumArt->GetOutline().GetWidth());

		context->With("ring"_hash, [this, windowWidth, windowHeight] (Context::Shader &shader) {
			shader.program.Uniform2f("screenSize"_hash, windowWidth, windowHeight);
			shader.program.Uniform1f("radius"_hash, albumArt->GetRadius(true) + albumArt->GetOutline().GetWidth() / 2);
		});
	}

	void OnLoop(const Delta &time, Vector2i pos, Context &context) {
		this->pos = pos;

		albumArt->OverrideOutlineAlpha(alpha);

		if (alpha == 0.0f) {
			PreLoop();

			promptOutline.OnLoop(pos.x, pos.y);
			prompt.OnLoop(pos.x, pos.y);

			PostLoop(time);
			return;
		}

		context.Blend(true, [&] {
			albumArt->DrawPlaceholder(
				windowWidth / 2,
				windowHeight / 2,
				alpha / 1.5f,
				context
			);
		});
		
		context.Use("ring"_hash);
		context.Color(0.0f, 0.0f, 0.0f, alpha * 0.75f);
		backdrop.OnLoop(windowWidth / 2, windowHeight / 2, context);

		context.Use("texture"_hash);
		context.Color(1.0f, 1.0f, 1.0f, 1.0f);
		promptOutline.OnLoop(pos.x, pos.y);
		prompt.OnLoop(pos.x, pos.y);

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
		);
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

	void AddArrow(Vector2f start, Vector2f end, const std::vector<std::string> &lines, bool bold = false) {
		if (auto iter = arrows.find(lines[0]); iter != arrows.end()) {
			iter->second.Update(start, end);
			return;
		}

		Arrow arrow;
		arrow.OnInit(font, boldFont, outlineFont, boldOutlineFont, *context, start, end, lines, bold);
		arrows.emplace(std::make_pair(lines[0], std::move(arrow)));
	}

	void UpdateArrow(const std::string text, Vector2f start, Vector2f end) {
		arrows[text].Update(start, end);
	}

	void RemoveArrow(const std::string &text) {
		if (const auto iter = arrows.find(text); iter != arrows.end()) {
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

	Vector2i pos = { 0, 0 };
};