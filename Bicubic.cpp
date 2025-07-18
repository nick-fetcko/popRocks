#include "Bicubic.hpp"

#define MULTITHREADED 1

SDL_Surface *Bicubic::ResizeImage(SDL_Surface *surface, float scale) {
	SDL_Surface *ret = SDL_CreateRGBSurfaceWithFormat(
		surface->flags,
		static_cast<int>(std::ceil(surface->w * scale)),
		static_cast<int>(std::ceil(surface->h * scale)),
		surface->format->BytesPerPixel,
		surface->format->format
	);

	if (!ret) return surface;

	uint8_t *pixels = reinterpret_cast<uint8_t *>(ret->pixels);

#if MULTITHREADED
	const auto numThreads = std::max(1u, std::thread::hardware_concurrency() - 1);
	std::vector<std::thread> threads(numThreads);

	for (unsigned int t = 0; t < numThreads; ++t) {
		threads[t] = std::thread([t, numThreads, ret, pixels, surface] {
			for (int y = static_cast<int>(t * std::ceil(static_cast<float>(ret->h) / numThreads));
				y < (t + 1) * std::ceil(static_cast<float>(ret->h) / numThreads) && y < ret->h;
				++y) {
#else
			for (int y = 0; y < ret->h; ++y) {
#endif
				uint8_t *destPixel = pixels + y * ret->pitch;
				float v = float(y) / float(ret->h - 1);
				for (int x = 0; x < ret->w; ++x) {
					float u = float(x) / float(ret->w - 1);
					auto sample = SampleBicubic(surface, u, v);

					destPixel[0] = sample[0];
					destPixel[1] = sample[1];
					destPixel[2] = sample[2];

					destPixel += 3;
				}
			}
#if MULTITHREADED
		});
	}

	for (unsigned int t = 0; t < numThreads; ++t) {
		if (threads[t].joinable()) {
			threads[t].join();
		}
	}
#endif

	SDL_FreeSurface(surface);

	return ret;
}

inline float Bicubic::CubicHermite(float A, float B, float C, float D, float t) {
	float a = -A / 2.0f + (3.0f * B) / 2.0f - (3.0f * C) / 2.0f + D / 2.0f;
	float b = A - (5.0f * B) / 2.0f + 2.0f * C - D / 2.0f;
	float c = -A / 2.0f + C / 2.0f;
	float d = B;

	return a * t * t * t + b * t * t + c * t + d;
}

inline const uint8_t *Bicubic::GetPixelClamped(SDL_Surface *surface, int x, int y) {
	x = std::clamp(x, 0, surface->w - 1);
	y = std::clamp(y, 0, surface->h - 1);

	auto pixels = reinterpret_cast<uint8_t *>(surface->pixels);

	return &pixels[(y * surface->pitch) + x * 3 + 0];
}

inline std::array<uint8_t, 3> Bicubic::SampleBicubic(SDL_Surface *surface, float u, float v) {
	// calculate coordinates -> also need to offset by half a pixel to keep image from shifting down and left half a pixel
	float x = (u * surface->w) - 0.5f;
	const int xInt = static_cast<int>(x);
	float xfract = x - std::floor(x);

	float y = (v * surface->h) - 0.5f;
	const int yInt = static_cast<int>(y);
	float yfract = y - std::floor(y);

	// 1st row
	auto p00 = GetPixelClamped(surface, xInt - 1, yInt - 1);
	auto p10 = GetPixelClamped(surface, xInt + 0, yInt - 1);
	auto p20 = GetPixelClamped(surface, xInt + 1, yInt - 1);
	auto p30 = GetPixelClamped(surface, xInt + 2, yInt - 1);

	// 2nd row
	auto p01 = GetPixelClamped(surface, xInt - 1, yInt + 0);
	auto p11 = GetPixelClamped(surface, xInt + 0, yInt + 0);
	auto p21 = GetPixelClamped(surface, xInt + 1, yInt + 0);
	auto p31 = GetPixelClamped(surface, xInt + 2, yInt + 0);

	// 3rd row
	auto p02 = GetPixelClamped(surface, xInt - 1, yInt + 1);
	auto p12 = GetPixelClamped(surface, xInt + 0, yInt + 1);
	auto p22 = GetPixelClamped(surface, xInt + 1, yInt + 1);
	auto p32 = GetPixelClamped(surface, xInt + 2, yInt + 1);

	// 4th row
	auto p03 = GetPixelClamped(surface, xInt - 1, yInt + 2);
	auto p13 = GetPixelClamped(surface, xInt + 0, yInt + 2);
	auto p23 = GetPixelClamped(surface, xInt + 1, yInt + 2);
	auto p33 = GetPixelClamped(surface, xInt + 2, yInt + 2);

	// interpolate bi-cubically!
	// Clamp the values since the curve can put the value below 0 or above 255
	std::array<uint8_t, 3> ret;
	for (int i = 0; i < 3; ++i) {
		float col0 = CubicHermite(p00[i], p10[i], p20[i], p30[i], xfract);
		float col1 = CubicHermite(p01[i], p11[i], p21[i], p31[i], xfract);
		float col2 = CubicHermite(p02[i], p12[i], p22[i], p32[i], xfract);
		float col3 = CubicHermite(p03[i], p13[i], p23[i], p33[i], xfract);
		float value = CubicHermite(col0, col1, col2, col3, yfract);
		value = std::clamp(value, 0.0f, 255.0f);
		ret[i] = uint8_t(value);
	}
	return ret;
}