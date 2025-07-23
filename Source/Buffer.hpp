#pragma once

#include <array>

class Buffer {
public:
	constexpr static std::array<unsigned short, 6> TriangleBuffer{ 0, 3, 1, 0, 3, 1 };
	constexpr static std::array<unsigned short, 6> SquareBuffer{ 0, 1, 2, 0, 2, 3 };

	constexpr static std::array<float, 8> TexCoordBuffer = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f };
};