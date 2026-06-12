#pragma once

#include "MathCPP/Duration.hpp"

#include "OpenGL/Polyline.hpp"

#include "Text.hpp"

class Loading {
private:
	inline static constexpr float FillSpeed = 50.0f;
	inline static constexpr float RotationSpeed = 360.0f;

public:
	void OnInit(float width, float radius, OpenGLFont *font, OpenGLFont *outlineFont, Context &context, const std::string &promptText) {
		this->radius = radius;

		line.SetWidth(width);
		outline.SetWidth(width * 5);

		prompt.OnInit(font, &context);
		prompt.SetText(promptText);

		promptOutline.SetColor(Colour<float>::Black);
		promptOutline.OnInit(outlineFont, &context);
		promptOutline.SetText(promptText);
	}

	void OnLoop(const Delta &time, int x, int y, Context &context) {
		context.Use("basic"_hash);

		context.Translate(x, y, 0.0f);
		context.Rotate(angle, 0.0f, 0.0f, 1.0f);
		context.Apply();

		if (progress < 100.0f) {
			progress += time.change.AsSeconds() * FillSpeed;
			if (progress > 100.0f) {
				reverseProgress = 0.0f;
				progress = 100.0f;
			}
		} else {
			reverseProgress += time.change.AsSeconds() * FillSpeed;

			if (reverseProgress > 100.0f) {
				progress = 0.0f;
				reverseProgress = 0.0f;
			}
		}

		angle += time.change.AsSeconds() * RotationSpeed;

		if (progress > 100.0f)
			progress = 0.0f;

		std::vector<Vector2f> points(static_cast<int>(progress - reverseProgress));

		if (!points.empty()) {
			for (float i = std::ceil(reverseProgress); i < static_cast<int>(progress); i += 1.0f) {
				const auto deg2rad = (i / 100.0f) * 360.0f * Maths::DEG2RAD<float>;

				points[std::floorl(i - reverseProgress)].x = sin(deg2rad) * radius;
				points[std::floorl(i - reverseProgress)].y = cos(deg2rad) * radius;
			}

			outline.SetPoints<Polyline::Join::Miter>(points.data(), points.size());
			line.SetPoints<Polyline::Join::Miter>(points.data(), points.size());

			context.Color(0.0f, 0.0f, 0.0f, 1.0f);
			outline.Draw<false>(context);

			context.Color(1.0f, 1.0f, 1.0f, 1.0f);
			line.Draw<true>(context);
		} else {
			context.LoadIdentity();
		}

		context.Use("texture"_hash);

		context.Color(1.0f, 1.0f, 1.0f, 0.5f);
		context.Blend(true, [this, &x, &y] {
			promptOutline.OnLoop(x - prompt.GetBounds().width / 2, y - prompt.GetBounds().height / 2 + radius / 4);
			prompt.OnLoop(x - prompt.GetBounds().width / 2, y - prompt.GetBounds().height / 2 + radius / 4);
		});
		context.Color(1.0f, 1.0f, 1.0f, 1.0f);
	}

	void OnDestroy() {
		prompt.OnDestroy();
		promptOutline.OnDestroy();

		outline.OnDestroy();
		line.OnDestroy();
	}

	const float &GetRadius() const { return radius; }

private:
	Text prompt;
	Text promptOutline;

	Fetcko::Polyline outline;
	Fetcko::Polyline line;

	float radius = 20.0f;

	float progress = 0.0f;
	float reverseProgress = 0.0f;

	float angle = 0.0f;
};