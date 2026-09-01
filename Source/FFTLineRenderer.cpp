#include "FFTLineRenderer.hpp"

#include "AlbumArt.hpp"
#include "Settings.hpp"

FFTLineRenderer::FFTLineRenderer(
	const DynamicGain<float> *dynamicGain,
	const AlbumArt *albumArt
) : LineRenderer(dynamicGain, albumArt) {
}

FFTLineRenderer::FFTLineRenderer(Renderer &&right) : LineRenderer(std::move(right)) {
	SetBuffer(buffer, fullBufferLength);
	SetBufferLength(bufferLength, true);
}

FFTLineRenderer::FFTLineRenderer(LineRenderer &&right) : LineRenderer(std::move(right)) {
	SetBuffer(buffer, fullBufferLength);
}

FFTLineRenderer::FFTLineRenderer(FFTLineRenderer &&right) noexcept : LineRenderer(std::move(right)) {
	this->min = right.min;
	this->max = right.max;

	right.min = nullptr;
	right.max = nullptr;

	SetBuffer(buffer, fullBufferLength);
}

FFTLineRenderer::~FFTLineRenderer() {
	delete[] min;
	delete[] max;
}

bool FFTLineRenderer::SetBuffer(const uint8_t *const buffer, std::size_t len, bool force) {
	auto changed = Renderer::SetBuffer(buffer, len, force);
	floatBuffer = reinterpret_cast<const float *>(buffer);

	if (changed || !min || !max) {
		delete[] min;
		delete[] max;

		min = new float[fullBufferLength];
		max = new float[fullBufferLength];

		std::memset(min, 0, sizeof(float) * fullBufferLength);
		std::memset(max, 0, sizeof(float) * fullBufferLength);

		for (auto i = 0; i < fullBufferLength; ++i) {
			if (changed) {
				if (dynamicGain->adjustMin)
					min[i] = dynamicGain->minReset;

				if (dynamicGain->adjustMax)
					max[i] = dynamicGain->maxReset;
			}
		}
	}

	return changed;
}

void FFTLineRenderer::OnLoop(
	const Delta &time,
	bool fileLoaded,
	float hStep,
	Context &context,
	const Colour<float> &color,
	const Colour<float> &brightColor,
	float frameCount,
	float maxHeardSample,
	bool resetGain,
	bool miniPlayer
) {
	minPoint = std::numeric_limits<float>::max();
	maxPoint = std::numeric_limits<float>::lowest();

	if (style == Style::Line) {
		for (int i = 0; i < bufferLength; i++) {
			points[i].x = i * hStep;
			points[i].y = (floatBuffer[i] * 2000);

			if (points[i].y > maxPoint)
				maxPoint = points[i].y;
			if (points[i].y < minPoint)
				minPoint = points[i].y;
		}

		auto [newMin, newMax] = CenterPoints(miniPlayer);

		for (int i = 0; i < bufferLength; i++)
			points[i].y += (newMax - newMin) / 2.0;
	} else {
		const auto radius = albumArt->GetRadius(miniPlayer) + line.GetWidth() * 5;
		const auto height = GetHeight(line.GetWidth() * 5, miniPlayer);

		for (int i = 0; i < bufferLength; i++) {
			const auto deg2rad = (i / static_cast<float>(bufferLength)) * 360.0f * Maths::DEG2RAD<float>;
			auto rawValue = floatBuffer[i];

			if (resetGain) {
				if (dynamicGain->adjustMin)
					min[i] = dynamicGain->minReset;

				if (dynamicGain->adjustMax)
					max[i] = dynamicGain->maxReset;
			}

			if (dynamicGain->adjustMin) {
				if (rawValue < min[i])
					min[i] -= dynamicGain->largeStep;
				else
					min[i] += dynamicGain->smallStep;
			}
			if (dynamicGain->adjustMax) {
				if (rawValue > max[i])
					max[i] += dynamicGain->largeStep;
				else
					max[i] -= dynamicGain->smallStep;
			}

			// If we're listening, skip normalization
			const auto scaledValue =
				//fileLoaded ?
				std::clamp(((rawValue - min[i]) / (max[i] - min[i])) * height, 0.0f, height) //:
				//rawValue * height;
				;


			const auto sample = scaledValue + radius;

			points[i].x = sample * sin(deg2rad);
			points[i].y = sample * cos(deg2rad);
		}

		points[bufferLength].x = points[0].x;
		points[bufferLength].y = points[0].y;

		newPoints = true;
	}
}

void FFTLineRenderer::Draw(
	const Delta &time,
	float frameCount,
	const Colour<float> &color,
	const Vector<int, 2> &blurOffset,
	Context &context
) {
	context.Use("basic"_hash);
	SetColor(color, 1.0f, context);

	if (style == Style::Line) {
		context.Translate(-blurOffset.x / 2.0f - (maxDimension - windowWidth) / 2.0f, -blurOffset.y / 2.0f + windowHeight / 2.0f, 0);

		context.Translate(maxDimension / 2.0f, 0, 0);
		context.Rotate(frameCount, 0.0f, 0.0f, 1.0f);
		context.Translate(-maxDimension / 2.0f, 0, 0);

		// TODO: Allow for this to move around the screen (iTunes style)?
		context.Translate(0, (offset / 2.0f / 100.0f) * windowHeight, 0);

		context.Apply();

		if (newPoints) {
			line.SetPoints<Polyline::Join::None>(points, bufferLength);
			newPoints = false;
		}
		line.Draw<false>(context);

		context.Translate(0, -(offset / 2.0f / 100.0f) * windowHeight, 0);
		context.Translate(maxDimension / 2.0f, 0, 0);
		context.Rotate(180, 0, 0, 1);
		context.Translate(-maxDimension / 2.0f, 0, 0);
		context.Translate(0, (offset / 2.0f / 100.0f) * windowHeight, 0);
		context.Apply();
		line.Draw<true>(context);
	} else {
		context.Translate(
			-blurOffset.x / 2.0f + windowWidth / 2.0f,
			-blurOffset.y / 2.0f + windowHeight / 2.0f,
			0
		);

		context.Rotate(frameCount, 0.0f, 0.0f, 1.0f);

		context.Apply();
		if (newPoints) {
			line.SetPoints<Polyline::Join::None>(points, bufferLength + 1);
			newPoints = false;
		}
		line.Draw<true>(context);
	}

	context.Use("texture"_hash);
}

void FFTLineRenderer::Reset() {
	for (auto i = 0; i < bufferLength; ++i) {
		min[i] = dynamicGain->minReset;
		max[i] = dynamicGain->maxReset;
	}
}