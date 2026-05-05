#include "Platform.hpp"

#include "Source/CApp.h"

// =====================================================
// ===================== Virtuals ======================
// =====================================================

// CApp helpers
bool Platform::OnMouseClicked(const Vector2i &mousePos) { 
	return false; 
}

// Display properties
void Platform::SetSafeArea(SDL_Window *window, Context &context, int w, int h) { 
	context.SetSafeArea({ 0, 0, w, h }); 
}

const float Platform::GetScale(float scale) const {
	return scale; 
}

const int Platform::IsAlphaPremultiplied() const {
	return (HDR::Enabled || app->GetPulseBackground() || app->GetMiniPlayer()) ? 0 : 1;
}

bool Platform::IsUiInverted() { 
	return false; 
}

GLenum Platform::GetFboInternalFormat(bool hdrEnabled) { 
	return hdrEnabled ? GL_RGBA16F : GL_RGBA; 
}

int Platform::GetAdapterIndex() { 
	return 0; 
}

bool Platform::IsTouchScreen() { 
	return false; 
}

int Platform::GetDefaultFramebuffer() { 
	return 0; 
}

// OpenGL
const bool Platform::GetOpenGlProperty() const {
	return !app->GetVulkan();
}

const char *Platform::GetOpenGlContextError() { 
	return ""; 
}

// Vulkan
const bool Platform::GetVulkanProperty() const {
	return app->GetVulkan();
}

// Exclusive mode
bool Platform::LoadExclusive(double pos) { 
	return false; 
};

bool Platform::ScaleExclusive(Renderer *renderer, uint8_t *buffer, float *floatBuffer, short *shortBuffer) { 
	return false; 
}

bool Platform::StartPlayingExclusive(bool fromPlaylist, bool fileLoaded, bool advanceOnNextLoop) { 
	return false; 
}

bool Platform::StartExclusive() { 
	return false; 
}

bool Platform::StopPlayingExclusive() { 
	return false; 
}

// Filepaths
std::filesystem::path Platform::GetNativePath(const std::filesystem::path &path) {
	return path.u8string(); 
}

void Platform::GetPicturesPath(std::stringstream &filename) {};

// Menus
void Platform::AddMenuCallbacks(Menu *menu) {}

// FBO blitting
void Platform::BlitBlurFbo() {}

// Audio
void Platform::Unmute() {}

bool Platform::PlayAfterLoad() { 
	return true; 
}

// =====================================================
// ================ Getters / Setters ==================
// =====================================================

// Display properties
const bool Platform::IsBgr() const { 
	return bgr; 
}

// Interops
Interop *Platform::GetInterop() {
	return interop; 
}

// Device listening
const float &Platform::GetMaxHeardSample() const { 
	return maxHeardSample; 
}

const bool Platform::IsListening() const { 
	return listening; 
}

void Platform::SetGain(float gain) { 
	this->gain = gain; 
}

// Exclusive mode
const float &Platform::GetExclusiveBufferSize() const {
	return exclusiveBufferSize; 
}

// Max buffer length
void Platform::SetMaxLength(std::size_t maxLength) { 
	this->maxLength = maxLength; 
}

const std::size_t &Platform::GetMaxLength() const {
	return maxLength; 
}

// =====================================================
// ====================== Factory ======================
// =====================================================
void PlatformFactory::Register(std::string &&platform, std::function<std::unique_ptr<Platform>(CApp *)> &&f) {
	builders.emplace(std::make_pair(std::move(platform), std::move(f)));
}

std::unique_ptr<Platform> PlatformFactory::Build(const std::string &platform, CApp *app) {
	return builders.at(platform)(app);
}