#pragma once

#include "Symbol.hpp"
#include "Text.hpp"

using namespace Fetcko;

class Checkbox : public Symbol {
public:
	Checkbox(AlbumArt *const albumArt) : Symbol(albumArt) {

	}

	void OnInit(OpenGLFont *font, OpenGLFont *outlineFont, float radius, Context &context) {
		Symbol::OnInit(radius);
		OnResize(radius);

		tooltip.OnInit(font, &context);
		tooltipOutline.OnInit(outlineFont, &context);
	}

	void OnDestroy() override {
		Symbol::OnDestroy();

		square.OnDestroy();
		outline.OnDestroy();
		check.OnDestroy();
		checkOutline.OnDestroy();

		tooltip.OnDestroy();
		tooltipOutline.OnDestroy();
	}

	void OnResize(float radius) override {
		Symbol::OnResize(radius);

		const auto width = std::round(radius / 16.0f);
		const auto outlineWidth = std::round(radius / 4.0f);

		square.SetWidth(width);
		check.SetWidth(width);
		outline.SetWidth(outlineWidth);
		checkOutline.SetWidth(outlineWidth);

		size = { radius / 4.0f + outlineWidth, radius / 4.0f + outlineWidth };

		std::vector<Vector2f> points = {
			{ -radius / 4.0f, -radius / 4.0f },
			{ -radius / 4.0f,  radius / 4.0f },
			{  radius / 4.0f,  radius / 4.0f },
			{  radius / 4.0f, -radius / 4.0f },
			{ -radius / 4.0f, -radius / 4.0f } // Join line
		};

		square.SetPoints<Polyline::Join::Miter>(points.data(), points.size());
		square.Loop<Polyline::Join::Miter>();

		outline.SetPoints<Polyline::Join::Miter>(points.data(), points.size());
		outline.Loop<Polyline::Join::Miter>();

		std::vector<Vector2f> checkPoints = {
			{ -radius / 7.0f,           0.0f },
			{           0.0f,  radius / 7.0f },
			{  radius / 7.0f, -radius / 7.0f }
		};

		check.SetPoints<Polyline::Join::Miter>(checkPoints.data(), checkPoints.size());
		checkOutline.SetPoints<Polyline::Join::Miter>(checkPoints.data(), checkPoints.size());
	}

	void OnLoop(float x, float y, const Delta &time, Context &context, const float *alpha = nullptr) override {
		Symbol::OnLoop(x, y, time, context, alpha);

		const auto OutlineColor = alpha ? albumArt->GetBlackColor() : 0.0f;

		context.Use("basic"_hash);
		context.Translate(x, y, 0);
		context.Apply();
		context.Color(OutlineColor, OutlineColor, OutlineColor, alpha ? *alpha : this->alpha);
		if (checked) checkOutline.Draw<false>(context);
		outline.Draw<false>(context);
		
		context.Color(GetColor().r, GetColor().g, GetColor().b, alpha ? *alpha : this->alpha);
		square.Draw<false>(context);
		if (checked) check.Draw<true>(context);

		context.Use("texture"_hash);
		context.LoadIdentity();

		if (hovered) {
			context.Color(OutlineColor, OutlineColor, OutlineColor, alpha ? *alpha : this->alpha);
			tooltipOutline.OnLoop(mousePos.x - tooltip.GetBounds().width / 2.0f, mousePos.y - tooltip.GetBounds().height * 1.5f);
			context.Color(1.0f, 1.0f, 1.0f, alpha ? *alpha : this->alpha);
			tooltip.OnLoop(mousePos.x - tooltip.GetBounds().width / 2.0f, mousePos.y - tooltip.GetBounds().height * 1.5f);
		}
	}

	const Vector2f &GetSize() const { return size; }

	void SetChecked(bool checked) { this->checked = checked; }
	const bool &GetChecked() const { return checked; }

	void SetTooltip(const std::string &tooltip) {
		this->tooltip.SetText(tooltip);
		tooltipOutline.SetText(tooltip);
	}

	void SetMousePos(const Vector2i &mousePos) {
		this->mousePos = mousePos;
	}

private:
	bool checked = false;

	Fetcko::Polyline square;
	Fetcko::Polyline outline;
	Fetcko::Polyline check;
	Fetcko::Polyline checkOutline;

	Vector2f size;

	Text tooltip;
	Text tooltipOutline;

	Vector2i mousePos = { 0, 0 };
};