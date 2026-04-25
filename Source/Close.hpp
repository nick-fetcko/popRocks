#pragma once

#include "Symbol.hpp"

class Close : public Symbol {
private:
	constexpr inline static float VerticalRatio = 18.0f;
	constexpr inline static float HorizontalRatio = 3.0f;

	constexpr inline static float OutlineVerticalRatio = 5.625f;
	constexpr inline static float OutlineHorizontalRatio = 2.25f;

public:
	Close(AlbumArt* const albumArt) : Symbol(albumArt) {
	}

	void OnInit(float radius) override {
		Symbol::OnInit(radius);

		vao->AddAttribute(VertexArray::Attribute(0, 2, 2 * sizeof(float)));

		eab = std::make_unique<ElementBuffer>();

		outlineVao = std::make_unique<VertexArray>();
		outlineVbo = std::make_unique<ArrayBuffer>();

		outlineVao->Bind();
		outlineVbo->Bind();

		outlineVao->AddAttribute(VertexArray::Attribute(0, 2, 2 * sizeof(float)));

		outlineVao->Unbind();
		outlineVbo->Unbind();

		OnResize(radius);

		auto squareBuffer = std::vector(Buffers::SquareBuffer.begin(), Buffers::SquareBuffer.end());

		eab->Bind();
		for (std::size_t i = 0; i < Buffers::SquareBuffer.size(); ++i)
			squareBuffer.emplace_back(Buffers::SquareBuffer[i] + 4);
		eab->BufferData(squareBuffer);
		eab->Unbind();
	}

	void OnDestroy() override {
		Symbol::OnDestroy();

		eab.reset();
		outlineVao.reset();
		outlineVbo.reset();
	}

	void OnResize(float radius) override {
		Symbol::OnResize(radius);

		std::vector<float> crossBuffer = {
			static_cast<GLfloat>(-radius / VerticalRatio), static_cast<GLfloat>(-radius / HorizontalRatio),
			static_cast<GLfloat>(-radius / VerticalRatio), static_cast<GLfloat>(radius / HorizontalRatio) ,
			static_cast<GLfloat>(radius / VerticalRatio) , static_cast<GLfloat>(radius / HorizontalRatio) ,
			static_cast<GLfloat>(radius / VerticalRatio) , static_cast<GLfloat>(-radius / HorizontalRatio),
			static_cast<GLfloat>(-radius / HorizontalRatio), static_cast<GLfloat>(-radius / VerticalRatio),
			static_cast<GLfloat>(-radius / HorizontalRatio), static_cast<GLfloat>(radius / VerticalRatio) ,
			static_cast<GLfloat>(radius / HorizontalRatio) , static_cast<GLfloat>(radius / VerticalRatio) ,
			static_cast<GLfloat>(radius / HorizontalRatio) , static_cast<GLfloat>(-radius / VerticalRatio),
		};

		vbo->BufferData(crossBuffer);
		vbo->Unbind();
		vao->Unbind();

		crossBuffer = {
			static_cast<GLfloat>(-radius / OutlineVerticalRatio), static_cast<GLfloat>(-radius / OutlineHorizontalRatio),
			static_cast<GLfloat>(-radius / OutlineVerticalRatio), static_cast<GLfloat>(radius / OutlineHorizontalRatio) ,
			static_cast<GLfloat>(radius / OutlineVerticalRatio) , static_cast<GLfloat>(radius / OutlineHorizontalRatio) ,
			static_cast<GLfloat>(radius / OutlineVerticalRatio) , static_cast<GLfloat>(-radius / OutlineHorizontalRatio),
			static_cast<GLfloat>(-radius / OutlineHorizontalRatio), static_cast<GLfloat>(-radius / OutlineVerticalRatio),
			static_cast<GLfloat>(-radius / OutlineHorizontalRatio), static_cast<GLfloat>(radius / OutlineVerticalRatio) ,
			static_cast<GLfloat>(radius / OutlineHorizontalRatio) , static_cast<GLfloat>(radius / OutlineVerticalRatio) ,
			static_cast<GLfloat>(radius / OutlineHorizontalRatio) , static_cast<GLfloat>(-radius / OutlineVerticalRatio),
		};

		outlineVao->Bind();
		outlineVbo->Bind();
		outlineVbo->BufferData(crossBuffer);
		outlineVbo->Unbind();
		outlineVao->Unbind();
	}

	void OnLoop(float x, float y, const Delta &time, Context &context, const float *alpha = nullptr) override {
		Symbol::OnLoop(x, y, time, context, alpha);

		const auto OutlineColor = alpha ? albumArt->GetBlackColor() : 0.0f;

		context.Use("basic"_hash);

		AutoFader::OnLoop(time);

		context.Translate(x, y, 0);
		context.Rotate(45.0f, 0.0f, 0.0f, 1.0f);
		context.Apply();

		context.Color(OutlineColor, OutlineColor, OutlineColor, alpha ? *alpha : this->alpha);

		// Outline
		outlineVao->Bind();
		eab->Bind();
		eab->DrawElements(GL_TRIANGLES);

		context.Color(GetColor().r, GetColor().g, GetColor().b, alpha ? *alpha : this->alpha);

		// Cross
		vao->Bind();
		eab->Bind();
		eab->DrawElements(GL_TRIANGLES);
		eab->Unbind();
		vao->Unbind();

		context.Use("texture"_hash);
		context.LoadIdentity();
	}

	constexpr static float GetLowestRatio() {
		return std::min(
			VerticalRatio,
			HorizontalRatio
		);
	}

private:
	std::unique_ptr<VertexArray> outlineVao;
	std::unique_ptr<ArrayBuffer> outlineVbo;

	std::unique_ptr<ElementBuffer> eab;
};