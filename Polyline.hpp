#pragma once

// Based off of https://stackoverflow.com/a/7854359
// and http://www.cowlumbus.nl/forum/Polyline2TriMeshTest.zip
// Dank je wel, Dutchman
//
// It's currently quite basic and missing caps, bevel / round joins, etc.
// 
// As such, rendering lines above a certain width (or
// joined with acute angles) will cause visual issues.

#include <cstddef>
#include <deque>

#include <glad/glad.h>

#include "MathCPP/Vector.hpp"

#include "CConsole.h"

class Polyline {
public:
	enum class Join { None, Miter };

	Polyline() = default;
	Polyline(float width) : width(width) {

	}

	Polyline(Polyline &&other) noexcept {
		MoveHelper(std::move(other));
	}

	Polyline &operator=(Polyline &&other) noexcept {
		MoveHelper(std::move(other));
		return *this;
	}

	virtual ~Polyline() {
		glDeleteBuffers(1, &elementBuffer);
		glDeleteBuffers(1, &arrayBuffer);
		delete[] vertexBuffer;
		delete[] indexBuffer;
	}

	// If we have more than 2 points, we have a line
	const bool HasLines() const { return lastPoints.size() > 1; }

	const float GetWidth() const { return width; }
	void SetWidth(float width) { this->width = width; }

	// This is a very costly operation and should
	// only be used to construct short polylines
	template<Join J>
	void AddPoint(Vector2f &&point) {
		if (!lastPoints.empty()) {
			auto indexBuffer = new GLushort[indexCount + 6];
			memcpy(indexBuffer, this->indexBuffer, indexCount * sizeof(GLushort));
			delete[] this->indexBuffer;
			this->indexBuffer = indexBuffer;

			auto vertexBuffer = new GLfloat[vertexCount + 8];
			memcpy(vertexBuffer, this->vertexBuffer, vertexCount * sizeof(GLfloat));
			delete[] this->vertexBuffer;
			this->vertexBuffer = vertexBuffer;

			if constexpr (J == Join::None) {
				BetweenTwoPoints<true>(lastPoints.back(), point);
			} else {
				// When we add a new point,
				// return to the last one
				// and add the miter
				if (lastPoints.size() > 1) {
					vertexCount -= 8;
					BetweenFourPoints<false>(lastPoints.front(), lastPoints[lastPoints.size() - 2], lastPoints.back(), point);
					BetweenFourPoints<true>(lastPoints[lastPoints.size() - 2], lastPoints.back(), point, point);
				} else {
					BetweenFourPoints<true>(lastPoints.back(), lastPoints.back(), point, point);
				}
			}

			UpdateElementBuffer();

			CreateArrayBuffer();
			UpdateArrayBuffer();
		}

		if (firstPoints.size() < 3)
			firstPoints.emplace_back(point);

		if (lastPoints.size() == 3)
			lastPoints.pop_front();

		lastPoints.emplace_back(point);

		++size;
	}

	// Joins the first and last points together
	template<Join J>
	void Loop() {
		if constexpr (J == Join::Miter) {
			if (lastPoints.back() != firstPoints.front()) {
				CConsole::Console.Print("Last and first points don't match! Adding an additional point.", MSG_ALERT);
				auto first = firstPoints[0];
				AddPoint<J>(std::move(first));
			}

			vertexCount -= 8;
			BetweenFourPoints<false>(lastPoints[0], lastPoints[1], firstPoints[0], firstPoints[1]);
			auto previousVertexCount = vertexCount;
			vertexCount = 0;
			BetweenFourPoints<false>(lastPoints[1], lastPoints[2], firstPoints[1], firstPoints[2]);
			vertexCount = previousVertexCount;

			UpdateArrayBuffer();
		} else {
			auto first = firstPoints[0];
			AddPoint<J>(std::move(first));
		}
	}

	template <Join J>
	void SetPoints(const Vector2f *points, std::size_t size) {
		// We refresh the vertices no matter what
		vertexCount = 0;

		if (size != this->size) {
			delete[] vertexBuffer;
			delete[] indexBuffer;

			indexCount = 0;

			if constexpr (J == Join::None) {
				vertexBuffer = new GLfloat[(size - 1) * 8];
				indexBuffer = new GLushort[(size - 1) * 6];

				for (std::size_t i = 1; i < size; ++i)
					BetweenTwoPoints<true>(points[i - 1], points[i]);
			} else {
				vertexBuffer = new GLfloat[size * 8];
				indexBuffer = new GLushort[size * 6];

				for (std::size_t i = 0; i < size; ++i) {
					auto a = (i == 0 ? 0 : (i - 1));
					auto b = i;
					auto c = ((i + 1) >= size) ? size - 1 : (i + 1);
					auto d = ((i + 2) >= size) ? size - 1 : (i + 2);

					BetweenFourPoints<true>(points[a], points[b], points[c], points[d]);
				}
			}

			UpdateElementBuffer();
			CreateArrayBuffer();

			this->size = size;
		} else {
			if constexpr (J == Join::None) {
				for (std::size_t i = 1; i < size; ++i)
					BetweenTwoPoints<false>(points[i - 1], points[i]);
			} else {
				for (std::size_t i = 0; i < size; ++i) {
					auto a = (i == 0 ? 0 : (i - 1));
					auto b = i;
					auto c = ((i + 1) >= size) ? size - 1 : (i + 1);
					auto d = ((i + 2) >= size) ? size - 1 : (i + 2);

					BetweenFourPoints<false>(points[a], points[b], points[c], points[d]);
				}
			}
		}

		UpdateArrayBuffer();

		if (size >= 3) {
			firstPoints = {
				points[0],
				points[1],
				points[2]
			};

			lastPoints = {
				points[size - 3],
				points[size - 2],
				points[size - 1]
			};
		} else {
			firstPoints.clear();
			lastPoints.clear();
			for (auto i = 0; i < size; ++i)
				firstPoints.emplace_back(points[i]);
			for (auto i = 1; i <= size; ++i)
				lastPoints.emplace_back(points[size - i]);
		}
	}

	void Draw() const {
		glEnableClientState(GL_VERTEX_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
		glVertexPointer(2, GL_FLOAT, 0, 0);
		glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT, nullptr);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glDisableClientState(GL_VERTEX_ARRAY);
		glLoadIdentity();
	}

private:
	inline void MoveHelper(Polyline &&other) {
		width = std::move(other.width);
		vertexCount = std::move(other.vertexCount);
		vertexBuffer = std::move(other.vertexBuffer);
		other.vertexBuffer = nullptr;
		indexCount = std::move(other.indexCount);
		indexBuffer = std::move(other.indexBuffer);
		other.indexBuffer = nullptr;
		firstPoints = std::move(other.firstPoints);
		lastPoints = std::move(other.lastPoints);
		size = std::move(other.size);
		elementBuffer = std::move(other.elementBuffer);
		other.elementBuffer = 0;
		arrayBuffer = std::move(other.arrayBuffer);
		other.arrayBuffer = 0;
	}

	inline void CreateArrayBuffer() {
		glDeleteBuffers(1, &arrayBuffer);
		glGenBuffers(1, &arrayBuffer);
	}

	inline void UpdateArrayBuffer() {
		glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer);
		glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof(float), vertexBuffer, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	inline void UpdateElementBuffer() {
		glDeleteBuffers(1, &elementBuffer);
		glGenBuffers(1, &elementBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(GLushort), indexBuffer, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	template<bool UpdateIndices>
	// https://www.youtube.com/watch?v=_LtQJHmW-lc
	inline void BetweenTwoPoints(const Vector2f &p1, const Vector2f &p2) {
		if constexpr (UpdateIndices) {
			indexBuffer[indexCount] = vertexCount / 2;
			indexBuffer[indexCount + 1] = vertexCount / 2 + 1;
			indexBuffer[indexCount + 2] = vertexCount / 2 + 2;
			indexBuffer[indexCount + 3] = vertexCount / 2;
			indexBuffer[indexCount + 4] = vertexCount / 2 + 2;
			indexBuffer[indexCount + 5] = vertexCount / 2 + 3;

			indexCount += 6;
		}

		const auto vector = p2 - p1;
		const Vector2f perpendicular{ -vector.y, vector.x };

		const auto normal = perpendicular.Normalize();

		// Top left
		vertexBuffer[vertexCount++] = p1.x + normal.x * width / 2;
		vertexBuffer[vertexCount++] = p1.y + normal.y * width / 2;

		// Top right
		vertexBuffer[vertexCount++] = p2.x + normal.x * width / 2;
		vertexBuffer[vertexCount++] = p2.y + normal.y * width / 2;

		// Bottom right
		vertexBuffer[vertexCount++] = p2.x - normal.x * width / 2;
		vertexBuffer[vertexCount++] = p2.y - normal.y * width / 2;

		// Bottom left
		vertexBuffer[vertexCount++] = p1.x - normal.x * width / 2;
		vertexBuffer[vertexCount++] = p1.y - normal.y * width / 2;
	}

	template<bool UpdateIndices>
	inline void BetweenFourPoints(const Vector2f &p0, const Vector2f &p1, const Vector2f &p2, const Vector2f &p3) {
		if constexpr (UpdateIndices) {
			indexBuffer[indexCount] = vertexCount / 2;
			indexBuffer[indexCount + 1] = vertexCount / 2 + 1;
			indexBuffer[indexCount + 2] = vertexCount / 2 + 2;
			indexBuffer[indexCount + 3] = vertexCount / 2;
			indexBuffer[indexCount + 4] = vertexCount / 2 + 2;
			indexBuffer[indexCount + 5] = vertexCount / 2 + 3;

			indexCount += 6;
		}

		// 1) define the line between the two points
		auto line = (p2 - p1).Normalize();

		// 2) find the normal vector of this line
		auto normal = Vector2f(-line.y, line.x).Normalize();

		// 3) find the tangent vector at both the end points:
		//		-if there are no segments before or after this one, use the line itself
		//		-otherwise, add the two normalized lines and average them by normalizing again
		auto tangent1 = (p0 == p1) ? line : ((p1 - p0).Normalize() + line).Normalize();
		auto tangent2 = (p2 == p3) ? line : ((p3 - p2).Normalize() + line).Normalize();

		// 4) find the miter line, which is the normal of the tangent
		auto miter1 = Vector2f(-tangent1.y, tangent1.x);
		auto miter2 = Vector2f(-tangent2.y, tangent2.x);

		// find length of miter by projecting the miter onto the normal,
		// take the length of the projection, invert it and multiply it by the thickness:
		//		length = thickness * ( 1 / |normal|.|miter| )
		float length1 = width / 2.0f / normal.Dot(miter1);
		float length2 = width / 2.0f / normal.Dot(miter2);

		// Upper left
		// p1 - length1 * miter1

		// Lower left
		// p1 + length1 * miter1

		// Lower right
		// p2 + length2 * miter2

		// Upper right
		// p2 - length2 * miter2

		Vector2f quad[4] = {
			p1 - length1 * miter1,
			p1 + length1 * miter1,
			p2 + length2 * miter2,
			p2 - length2 * miter2
		};

		// Top left
		vertexBuffer[vertexCount++] = quad[0].x;
		vertexBuffer[vertexCount++] = quad[0].y;

		// Top right
		vertexBuffer[vertexCount++] = quad[1].x;
		vertexBuffer[vertexCount++] = quad[1].y;

		// Bottom right
		vertexBuffer[vertexCount++] = quad[2].x;
		vertexBuffer[vertexCount++] = quad[2].y;

		// Bottom left
		vertexBuffer[vertexCount++] = quad[3].x;
		vertexBuffer[vertexCount++] = quad[3].y;
	}

	float width = 1.0f;

	GLushort vertexCount = 0;
	GLfloat *vertexBuffer = nullptr;

	GLsizei indexCount = 0;
	GLushort *indexBuffer = nullptr;

	// We need to store the first 3 points
	// for joining the first and last lines
	// together in Loop().
	std::vector<Vector2f> firstPoints;

	// We need to store the last 3 points
	// for joining to the previous line
	// when a new line is added.
	std::deque<Vector2f> lastPoints;

	std::size_t size = 0;

	GLuint elementBuffer = 0;
	GLuint arrayBuffer = 0;
};