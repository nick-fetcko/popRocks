#pragma once

#include "LineRenderer.hpp"

class FFTLineRenderer : public LineRenderer {
public:
	FFTLineRenderer(
		const DynamicGain<float> *dynamicGain,
		const AlbumArt *albumArt
	) : LineRenderer(dynamicGain, albumArt) {
	}

	FFTLineRenderer(Renderer &&right) : LineRenderer(std::move(right)) {
		SetBuffer(buffer, fullBufferLength);
		SetBufferLength(bufferLength, true);
	}

	FFTLineRenderer(LineRenderer &&right) : LineRenderer(std::move(right)) {
		SetBuffer(buffer, fullBufferLength);
	}

	bool IsFloatingPoint() const override { return true; }

	bool SetBuffer(const uint8_t *const buffer, std::size_t len, bool force = false) override {
		auto ret = Renderer::SetBuffer(buffer, len, force);
		floatBuffer = reinterpret_cast<const float *>(buffer);

		return ret;
	}

	void OnLoop(
		const Delta &time,
		bool fileLoaded,
		float hStep,
		float maxHeardSample = 0.0f,
		bool resetGain = false
	) override {
		minPoint = std::numeric_limits<float>::max();
		maxPoint = std::numeric_limits<float>::lowest();

		for (int i = 0; i < bufferLength; i++) {
			points[i].x = i * hStep;
			points[i].y = -(floatBuffer[i] * 2000);

			if (points[i].y > maxPoint)
				maxPoint = points[i].y;
			if (points[i].y < minPoint)
				minPoint = points[i].y;
		}
	}

	void Draw(
		const Delta &time,
		float frameCount,
		const Colour<float> &color
	) override {
		// Center / scale points within our album art circle
		for (auto i = 0; i < bufferLength; ++i)
			points[i].y = ((points[i].y - minPoint) / (maxPoint - minPoint)) * (albumArt->GetRadius() * 2) + albumArt->GetRadius() * -1;

		SetColor(color, 1.0f);
		glTranslatef(0, windowHeight / 3.0f * 2.0f, 0);
		// TODO: allow the oscilloscope / fft line to rotate
		/*
		glRotatef(
			360.0f - frameCount,
			0.0f,
			0.0f,
			1.0f
		);
		*/
		line.SetPoints<Polyline::Join::None>(points, bufferLength);
		line.Draw();
	}

	void Reset() override {

	}

private:
	const float *floatBuffer = nullptr;
};