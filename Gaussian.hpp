#pragma once

#include <cmath>
#include <sstream>

#include <SDL_image.h>
#include "MathCPP/Maths.hpp"

#include "CConsole.h"

using namespace MathsCPP;

// This was derived from a combination of 
// https://www.geeksforgeeks.org/blogs/gaussian-filter-generation-c/
// and 
// https://stackoverflow.com/questions/42186498/gaussian-blur-image-processing-c
class Gaussian {
public:
	Gaussian(int kernelSize = 3, double sigma = 1.0);
	~Gaussian();

	SDL_Surface *Blur(SDL_Surface *surface);

private:
	void GenerateKernel(double **kernel);

	inline int GetPixel(SDL_Surface *surface, int col, int row, int k);

	// This is an int so we can negate
	// without casting to a signed type
	// every time
	int kernelSize = 3;

	double sigma = 1.0;

	double **kernel = nullptr;
};