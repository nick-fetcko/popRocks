#pragma once

#include "OpenGL/Polyline.hpp"

#include "Renderer.hpp"

class LineRenderer : public Renderer {
public:
	enum class Style {
		Line,
		Circle
	};

	LineRenderer(
		const DynamicGain<float> *dynamicGain,
		const AlbumArt *albumArt
	);

	LineRenderer(Renderer &&other);
	LineRenderer(LineRenderer &&other) noexcept;

	void OnDestroy() override;

	virtual ~LineRenderer();

	void SetBufferLength(std::size_t bufferLength, bool changed) override;

	void SetWidth(float width);

	void SetStyle(Style style);

protected:
	std::pair<float, float> CenterPoints(bool miniPlayer);

	Vector2f *points = nullptr;
	Fetcko::Polyline line;

	float minPoint = std::numeric_limits<float>::max();
	float maxPoint = std::numeric_limits<float>::lowest();

	bool newPoints = true;

	Style style;
};