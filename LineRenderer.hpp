#pragma once

#include "Renderer.hpp"
#include "Polyline.hpp"

class LineRenderer : public Renderer {
public:
	LineRenderer(
		const DynamicGain<float> *dynamicGain,
		const AlbumArt *albumArt
	) : Renderer(dynamicGain, albumArt) {
	}

	LineRenderer(Renderer &&other) : Renderer(std::move(other)) {

	}

	LineRenderer(LineRenderer &&other) noexcept :
		Renderer(std::move(other)) {
		points = std::move(other.points);
		other.points = nullptr;
		line = std::move(other.line);
	}

	virtual ~LineRenderer() {
		delete[] points;
	}

	void SetBufferLength(std::size_t bufferLength, bool changed) override {
		Renderer::SetBufferLength(bufferLength, changed);

		if (changed) {
			delete[] points;
			points = new Vector2f[bufferLength];
		}
	}

	void SetWidth(float width) {
		line.SetWidth(width);
	}

protected:
	Vector2f *points = nullptr;
	Polyline line = Polyline(4.0f);

	float minPoint = std::numeric_limits<float>::max();
	float maxPoint = std::numeric_limits<float>::lowest();
};