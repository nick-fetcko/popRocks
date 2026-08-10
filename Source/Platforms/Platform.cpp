#include "Platform.hpp"

#include "Source/CApp.h"

// =====================================================
// ===================== Virtuals ======================
// =====================================================

// CApp helpers
bool Platform::OnMouseClicked(const Vector2i &mousePos) { 
	return false; 
}

bool Platform::OnMouseDown(const Vector2i &mousePos) {
	return false;
}

// Display properties
void Platform::SetSafeArea(SDL_Window *window, Context &context, int w, int h) { 
	context.SetSafeArea({ 0, 0, w, h }); 
}

const float Platform::GetScale(SDL_Window *window, Context &context, int *x, int *y) {
	return scale; 
}

const float Platform::GetScale(bool actual) const {
	return scale;
}

const float Platform::GetScaleForPoint(int x, int y) const {
	return 1.0f;
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

bool Platform::StartPlayingExclusive(bool fromPlaylist, bool fileLoaded, bool advanceOnNextLoop) { 
	return false; 
}

bool Platform::StartExclusive(bool wasPlaying) { 
	return false; 
}

bool Platform::StopPlayingExclusive() { 
	return false; 
}

std::size_t Platform::GetAvailable() const {
	return 0;
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
	return Settings::settings.GetAutoPlay(); 
}

std::map<std::string, Platform::OutputDevice> Platform::GetOutputDevices() {
	return {{}};
}

// Mouse pointer
bool Platform::IsPointerInWindow() const {
	return true;
}

// Window management
bool Platform::IsMoving() const {
	return false;
}

bool Platform::IsResizing() const {
	return false;
}

void Platform::UpdateWindowShape() {
}

void Platform::DestroyWindow(SDL_Window *window) {
	SDL_DestroyWindow(window);
}

void Platform::HookWindow(bool miniPlayer) {

}

// Bling
void Platform::SetStatus(Status status, int progress) {

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

const int64_t &Platform::GetExclusiveBufferSizeInBytes() const {
	return exclusiveBufferBytes;
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