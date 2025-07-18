#include "Gaussian.hpp"

#define MULTITHREADED 1

Gaussian::Gaussian(int kernelSize, double sigma) :
	kernelSize(kernelSize),
	sigma(sigma) {
	kernel = new double *[kernelSize];
	for (int i = 0; i < kernelSize; ++i)
		kernel[i] = new double[kernelSize];

	GenerateKernel(kernel);
}

Gaussian::~Gaussian() {
	if (kernel) {
		for (auto i = 0; i < kernelSize; ++i)
			delete[] kernel[i];

		delete[] kernel;
	}
}

SDL_Surface *Gaussian::Blur(SDL_Surface *surface) {
	SDL_Surface *ret = SDL_CreateRGBSurfaceWithFormat(
		surface->flags,
		surface->w,
		surface->h,
		surface->format->BytesPerPixel,
		surface->format->format
	);

	const auto pixels = reinterpret_cast<uint8_t *>(ret->pixels);

#if MULTITHREADED
	const auto numThreads = std::max(1u, std::thread::hardware_concurrency() - 1);
	std::vector<std::thread> threads(numThreads);

	for (unsigned int t = 0; t < numThreads; ++t) {
		threads[t] = std::thread([this, t, numThreads, surface, ret, pixels] {
			for (int row = static_cast<int>(t * std::ceil(static_cast<float>(surface->h) / numThreads));
				row < (t + 1) * std::ceil(static_cast<float>(surface->h) / numThreads) && row < surface->h;
				++row) {
#else
				for (int row = 0; row < surface->h; ++row) {
#endif
				for (int col = 0; col < surface->w; ++col) {
					for (int k = 0; k < 3; k++) {
						pixels[row * ret->pitch + 3 * col + k] = GetPixel(surface, col, row, k);
					}
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

void Gaussian::GenerateKernel(double **kernel) {
	double r, s = 2.0 * sigma * sigma;
	double sum = 0.0;

	// Generate
	for (int x = -(kernelSize / 2); x <= (kernelSize / 2); ++x) {
		for (int y = -(kernelSize / 2); y <= (kernelSize / 2); ++y) {
			r = std::sqrt(x * x + y * y);
			kernel[x + kernelSize / 2][y + kernelSize / 2] = (std::exp(-(r * r) / s)) / (Maths::PI<double> *s);
			sum += kernel[x + kernelSize / 2][y + kernelSize / 2];
		}
	}

	// Normalize
	for (int i = 0; i < kernelSize; ++i)
		for (int j = 0; j < kernelSize; ++j)
			kernel[i][j] /= sum;
}

inline int Gaussian::GetPixel(SDL_Surface *surface, int col, int row, int k) {
	double sum = 0;
	double sumKernel = 0;

	for (int j = -(kernelSize / 2); j <= kernelSize / 2; ++j) {
		for (int i = -(kernelSize / 2); i <= kernelSize / 2; ++i) {
			if ((row + j) >= 0 && (row + j) < surface->h && (col + i) >= 0 && (col + i) < surface->w) {
				int color = reinterpret_cast<uint8_t *>(surface->pixels)[(row + j) * surface->pitch + (col + i) * 3 + k];
				sum += color * kernel[i + kernelSize / 2][j + kernelSize / 2];
				sumKernel += kernel[i + kernelSize / 2][j + kernelSize / 2];
			}
		}
	}

	return static_cast<int>(sum / sumKernel);
}