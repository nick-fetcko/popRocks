#include "Text.hpp"

#include "Buffer.hpp"

void Text::OnInit(TTF_Font *font) {
	this->font = font;

	vao = std::make_unique<VertexArray>();
	vbo = std::make_unique<ArrayBuffer>();
	eab = std::make_unique<ElementBuffer>();

	vao->Bind();
	vbo->Bind();
	vao->AddAttribute(VertexArray::Attribute(0, 2, 4 * sizeof(float)));
	vao->AddAttribute(VertexArray::Attribute(1, 2, 4 * sizeof(float), 2 * sizeof(float)));
	vbo->Unbind();
	vao->Unbind();

	eab->Bind();
	eab->BufferData<std::size(Buffers::SquareBuffer)>(Buffers::SquareBuffer);
	eab->Unbind();

	// Refresh our text, if there is any
	SetText(text, true);
}

Vector2i Text::MeasureText(const std::string &text) const {
	Vector2i ret;
	TTF_SizeUTF8(font, text.c_str(), &ret.x, &ret.y);
	return ret;
}

void Text::SetText(const std::string &text, bool force) {
	if ((this->text == text || !font) && !force) return;

	this->text = text;

	// Size needs to be re-measured
	size = { 0, 0 };

	// Don't try to load an empty string
	//
	// We'll likely get a null surface anyway
	if (Empty()) return;

	auto surface = TTF_RenderUTF8_Blended(
		font,
		text.c_str(),
		SDL_Color {
			static_cast<uint8_t>(color.r * 255.0f),
			static_cast<uint8_t>(color.g * 255.0f),
			static_cast<uint8_t>(color.b * 255.0f),
			static_cast<uint8_t>(color.a * 255.0f)
		}
	);

	if (!surface) {
		this->text.clear();
		return;
	}

	glEnable(GL_TEXTURE_2D);
	glDeleteTextures(1, &texture);
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	GLenum textureFormat = GL_RGBA;
	if (surface->format->BytesPerPixel == 4) {		// alpha
		if (surface->format->Rmask == 0x000000ff)
			textureFormat = GL_RGBA;
		else
			textureFormat = GL_BGRA;
	} else {										// no alpha
		if (surface->format->Rmask == 0x000000ff)
			textureFormat = GL_RGB;
		else
			textureFormat = GL_BGR;
	}

	// For some reason (on my machine, at least) each row has a ton of extra padding
	// Use this to (slowly) flatten the data
	uint8_t *flat = new uint8_t[surface->w * surface->h * surface->format->BytesPerPixel];
	auto pixels = reinterpret_cast<uint8_t *>(surface->pixels);

	for (auto y = 0; y < surface->h; ++y) {
		for (auto x = 0; x < surface->w; ++x) {
			flat[y * surface->w * 4 + (x * 4 + 0)] = pixels[y * surface->pitch + (x * 4 + 0)];
			flat[y * surface->w * 4 + (x * 4 + 1)] = pixels[y * surface->pitch + (x * 4 + 1)];
			flat[y * surface->w * 4 + (x * 4 + 2)] = pixels[y * surface->pitch + (x * 4 + 2)];
			flat[y * surface->w * 4 + (x * 4 + 3)] = pixels[y * surface->pitch + (x * 4 + 3)];
		}
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0, textureFormat, GL_UNSIGNED_BYTE, flat);

	delete[] flat;

	std::vector<float> buffer(16);

	buffer[0] = 0;
	buffer[1] = 0;
	buffer[2] = Buffers::TexCoordBuffer[0];
	buffer[3] = Buffers::TexCoordBuffer[1];
	buffer[4] = 0;
	buffer[5] = static_cast<float>(surface->h);
	buffer[6] = Buffers::TexCoordBuffer[2];
	buffer[7] = Buffers::TexCoordBuffer[3];
	buffer[8] = static_cast<float>(surface->w);
	buffer[9] = static_cast<float>(surface->h);
	buffer[10] = Buffers::TexCoordBuffer[4];
	buffer[11] = Buffers::TexCoordBuffer[5];
	buffer[12] = static_cast<float>(surface->w);
	buffer[13] = 0;
	buffer[14] = Buffers::TexCoordBuffer[6];
	buffer[15] = Buffers::TexCoordBuffer[7];

	vbo->Bind();
	vbo->BufferData(buffer);
	vbo->Unbind();

	size = { surface->w, surface->h + TTF_FontDescent(font) / 2.0f };

	SDL_FreeSurface(surface);
	glDisable(GL_TEXTURE_2D);
}

void Text::OnDestroy() {
	glDeleteTextures(1, &texture);
	texture = 0;

	vao.reset();
	vbo.reset();
	eab.reset();
}

void Text::OnLoop(int x, int y, Context &context) const {
	if (!font || Empty()) return;

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, texture);

	context.Translate(x, y, 0.0f);
	context.Apply();

	vao->Bind();
	eab->Bind();
	eab->DrawElements(GL_TRIANGLES);
	eab->Unbind();
	vao->Unbind();

	context.LoadIdentity();

	glDisable(GL_TEXTURE_2D);
}