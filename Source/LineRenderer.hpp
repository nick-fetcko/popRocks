#pragma once

#include "Renderer.hpp"
#include "Polyline.hpp"
#include "Settings.hpp"

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

	void OnDestroy() override {
		line.OnDestroy();
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
		Settings::settings.SetWidth(width);
	}

protected:
	inline void CenterPoints() {
		// Center / scale points within our album art circle
		for (auto i = 0; i < bufferLength; ++i)
			points[i].y = (((points[i].y - minPoint) / (maxPoint - minPoint)) * (albumArt->GetRadius() * 2) + albumArt->GetRadius() * -1) * scale;
	}

	Vector2f *points = nullptr;
	Polyline line = Polyline(Settings::settings.GetWidth());

	float minPoint = std::numeric_limits<float>::max();
	float maxPoint = std::numeric_limits<float>::lowest();
};