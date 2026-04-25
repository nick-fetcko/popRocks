#include "Desktop.hpp"

#include "OpenGL/Interops/Vulkan.hpp"

#include "Source/CApp.h"

Desktop::Desktop(CApp *app) : Platform(app) {

}

Desktop::~Desktop() {
	if (in) fftwf_free(in);
	if (out) fftwf_free(out);
	if (plan) fftwf_destroy_plan(plan);
}

// =====================================================
// =================== Pure Virtuals ===================
// =====================================================

// -----------------------------------------------------
// ------------------- CApp Helpers --------------------
// -----------------------------------------------------
void Desktop::OnInit(Interop::InitArgs args, Context &context) {
	interop->OnInit(args);

	const auto format = dynamic_cast<Vulkan *>(interop)->GetSwapchainImageFormat();
	if ((format > 29 && format < 37) || (format > 43 && format < 51)) {
		context.With("texture"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform1i("bgr"_hash, 1);
		});
		context.With("rotate"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform1i("bgr"_hash, 1);
		});
		context.With("blur"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform1i("bgr"_hash, 1);
		});
		context.With("basic"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform1i("bgr"_hash, 1);
		});
		context.With("blit"_hash, [this](Context::Shader &shader) {
			shader.program.Uniform1i("bgr"_hash, 1);
		});

		bgr = true;
	}
}

void Desktop::OnResize(int windowWidth, int windowHeight) {
	if (auto &context = app->GetContext()) {
		GetInterop()->OnResize(windowWidth, windowHeight);

		context->SetIdentity(glm::ortho(0.0f, static_cast<float>(windowWidth), 0.0f, static_cast<float>(windowHeight)));
		context->Apply();
	}
}

// -----------------------------------------------------
// ---------------------- OpenGL -----------------------
// -----------------------------------------------------
void Desktop::SetGlAttributes() {
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
}

void Desktop::OpenOpenGlWindow(SDL_PropertiesID &props) {
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, false);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);

	app->SetOpenGlWindow(SDL_CreateWindowWithProperties(
		props
	));
}

bool Desktop::LoadGlad() {
	return gladLoadGL();
}

// -----------------------------------------------------
// ---------------------- HDR --------------------------
// -----------------------------------------------------
void Desktop::SetHdr(bool enabled, void *hwnd, int width, int height) {
	if (!enabled && HDR::Enabled) {
		GetInterop()->SetHdr(enabled, nullptr, width, height);

		if (auto &context = app->GetContext())
			context->SetIdentity(glm::ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height)));
	} else if (enabled && !HDR::Enabled) {
		if (app->GetBlur()) {
			app->SetBlurFbo(std::make_unique<MultisampledFramebufferObject>(app->GetMaxDimension(), app->GetMaxDimension(), enabled ? GL_RGBA16F : GL_RGBA));
			app->SetLastFrame(std::make_unique<MultisampledFramebufferObject>(app->GetMaxDimension(), app->GetMaxDimension(), enabled ? GL_RGBA16F : GL_RGBA));
		}

		app->SetUiFbo(std::make_unique<MultisampledFramebufferObject>(app->GetWindowSize().first, app->GetWindowSize().second, enabled ? GL_RGBA16F : GL_RGBA, IsUiInverted()));

		GetInterop()->SetHdr(enabled,
			hwnd,
			width,
			height
		);

		if (auto &context = app->GetContext()) {
			context->SetIdentity(glm::ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height)));
			context->Apply();
		}

		if (auto &blurFbo = app->GetBlurFbo())
			blurFbo->SetDefaultFramebuffer(GetInterop()->GetFramebuffer());
		if (auto &lastFrame = app->GetLastFrame())
			lastFrame->SetDefaultFramebuffer(GetInterop()->GetFramebuffer());
		if (auto &uiFbo = app->GetUiFbo())
			uiFbo->SetDefaultFramebuffer(GetInterop()->GetFramebuffer());

		if (auto font = app->GetControls().GetFont())
			font->SetDefaultFramebuffer(GetInterop()->GetFramebuffer());
		if (auto boldFont = app->GetControls().GetFont())
			boldFont->SetDefaultFramebuffer(GetInterop()->GetFramebuffer());
		if (auto outlineFont = app->GetControls().GetOutlineFont())
			outlineFont->SetDefaultFramebuffer(GetInterop()->GetFramebuffer());
		if (auto boldOutlineFont = app->GetControls().GetBoldOutlineFont())
			boldOutlineFont->SetDefaultFramebuffer(GetInterop()->GetFramebuffer());
	}
}

void Desktop::UpdateHdrProperties(int displayId, bool force) {
	if (auto properties = GetHdrProperties(displayId, force)) {
		auto &[enabled, whitePoint, headroom] = *properties;

		LogDebug("HDR properties changed: ");
		LogDebug("\tEnabled: ", enabled ? "Yes" : "No");
		LogDebug("\tWhitePoint: ", whitePoint);
		LogDebug("\tHeadroom: ", headroom);

		SetHdr(enabled, nullptr, app->GetWindowSize().first, app->GetWindowSize().second);

		HDR::Enabled = enabled;
		HDR::WhiteLevel = whitePoint;
		HDR::Headroom = headroom;
	}
}

// =====================================================
// ===================== Virtuals ======================
// =====================================================

// -----------------------------------------------------
// --------------------- OpenGL ------------------------
// -----------------------------------------------------
const char *Desktop::GetOpenGlContextError() {
	return SDL_GetError();
}

// -----------------------------------------------------
// ------------------- FBO blitting --------------------
// -----------------------------------------------------
void Desktop::BlitBlurFbo() {
	app->GetBlurFbo()->Blit(
		*app->GetContext(),
		nullptr,
		app->GetMiniPlayer() ? app->GetAlbumArt().GetChromaColor() : 0.0f,
		app->GetMiniPlayer() ? 1.0f : 0.0f
	);
}