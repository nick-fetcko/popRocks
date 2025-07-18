#pragma once

#include <algorithm>
#include <array>

#include <SDL_image.h>

#include "CConsole.h"

// Derived from https://blog.demofox.org/2015/08/15/resizing-images-with-bicubic-interpolation/
class Bicubic {
public:
	static SDL_Surface *ResizeImage(SDL_Surface *surface, float scale);

private:
	// t is a value that goes from 0 to 1 to interpolate in a C1 continuous way across uniformly sampled data points.
	// when t is 0, this will return B.  When t is 1, this will return C.  Inbetween values will return an interpolation
	// between B and C.  A and B are used to calculate slopes at the edges.
	static inline float CubicHermite(float A, float B, float C, float D, float t);

	static inline const uint8_t *GetPixelClamped(SDL_Surface *surface, int x, int y);

	static inline std::array<uint8_t, 3> SampleBicubic(SDL_Surface *surface, float u, float v);
};