#if defined(__linux__) && !defined(__ANDROID__)

#include "Source/Platforms/Linux.hpp"

#include <bassalac.h>
#include <bass_aac.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

#include "Source/CApp.h"

// =====================================================
// ================ Factory Registration ===============
// =====================================================
bool Linux::Register() {
	PlatformFactory::Register("linux", [](CApp *app) {
		return std::make_unique<Linux>(app);
	});

	return true;
}
bool Linux::registered = Register();

// =====================================================
// =================== Implementation ==================
// =====================================================
Linux::Linux(CApp *app) : Desktop(app) {
	interop = new Vulkan();
}

Linux::~Linux() {
}

// =====================================================
// =================== Pure Virtuals ===================
// =====================================================

// -----------------------------------------------------
// ------------------- CApp Helpers --------------------
// -----------------------------------------------------
void Linux::OnDestroy() {
    interop->OnDestroy();

	if (listening) {
		if (listenThread.joinable())
			listenThread.join();
		listening = false;
	}
}

std::optional<bool> Linux::OnLoop() {
	return interop->OnLoop();
}

void Linux::SwapBuffers() {
}

// -----------------------------------------------------
// ------------------ Keyboard hooks -------------------
// -----------------------------------------------------
void Linux::HookKeyboard() {
}

// -----------------------------------------------------
// ---------------------- OpenGL -----------------------
// -----------------------------------------------------
bool Linux::CreateOpenGlContext() {
	app->SetOpenGlContext(SDL_GL_CreateContext(
		app->GetOpenGlWindow()
	));

	return app->GetOpenGlContext() != nullptr;
}

// -----------------------------------------------------
// ---------------- Device listening -------------------
// -----------------------------------------------------
void Linux::Listen(bool loopback) {
	StopListening();

	SetGain(1.0f);

	in = reinterpret_cast<float *>(fftwf_malloc(sizeof(float) * maxLength * 2));
	out = reinterpret_cast<fftwf_complex *>(fftwf_malloc(sizeof(fftwf_complex) * maxLength * 2));
	plan = fftwf_plan_dft_r2c_1d(static_cast<int>(maxLength * 2), in, out, FFTW_MEASURE);

    // TODO: Listening on Linux

	listening = true;
}

void Linux::StopListening() {
	if (listening) {
		//audioSink->done = true;
		if (listenThread.joinable())
			listenThread.join();

		fftwf_free(in);
		fftwf_free(out);
		fftwf_destroy_plan(plan);

		//delete audioSink;

		listening = false;
	}
}

void Linux::LoadHeardSamples(Renderer *renderer, float *floatBuffer, short *shortBuffer, const std::size_t bufferLength) {
	
}

// -----------------------------------------------------
// ----------------- Exclusive mode --------------------
// -----------------------------------------------------
bool Linux::OpenExclusive(const std::filesystem::path &path, const std::string &extension, bool exclusive, HSTREAM &target, bool force, const BASS_CHANNELINFO &channelInfo, void *data) {
	return false;
}

void Linux::StopExclusive(bool reset) {
}

// -----------------------------------------------------
// ---------------------- HDR --------------------------
// -----------------------------------------------------
std::optional<std::tuple<bool, float, float>> Linux::GetHdrProperties(int display, bool force) {
	SDL_PropertiesID displayProps = SDL_GetDisplayProperties(
		display
	);

	SDL_PropertiesID windowProps = SDL_GetWindowProperties(app->GetSdlWindow());
	bool enabled = SDL_GetBooleanProperty(displayProps, SDL_PROP_DISPLAY_HDR_ENABLED_BOOLEAN, false);
	float whitePoint = SDL_GetFloatProperty(windowProps, SDL_PROP_WINDOW_SDR_WHITE_LEVEL_FLOAT, 1.0f);
	float headroom = SDL_GetFloatProperty(windowProps, SDL_PROP_WINDOW_HDR_HEADROOM_FLOAT, 1.0f);

	enabled |= (headroom > 1.01);

	return std::make_tuple(enabled, whitePoint, headroom);
}

void Linux::UpdateHdrProperties(bool force) {
	auto displayId = SDL_GetDisplayForWindow(app->GetSdlWindow());

	Desktop::UpdateHdrProperties(displayId, force);
}

// -----------------------------------------------------
// ------------------ File Opening ---------------------
// -----------------------------------------------------
HSTREAM Linux::OpenWithFlags(const std::filesystem::path &path, const std::string &extension, DWORD flags) {
	auto ret = BASS_StreamCreateFile(
		FALSE,
		path.u8string().c_str(),
		0,
		0,
		flags
	);
	if (!ret) {
		// In case our plugins didn't properly load
		if (extension == ".flac") {
			ret = BASS_FLAC_StreamCreateFile(
				FALSE,
				path.u8string().c_str(),
				0,
				0,
				flags
			);
		} else if (extension == ".ape") {
			ret = BASS_APE_StreamCreateFile(
				FALSE,
				path.u8string().c_str(),
				0,
				0,
				flags
			);
		} else if (extension == ".wv") {
			ret = BASS_WV_StreamCreateFile(
				FALSE,
				path.u8string().c_str(),
				0,
				0,
				flags
			);
		}
		else if (extension == ".m4a" || extension == ".mp4") {
			ret = BASS_ALAC_StreamCreateFile(
				FALSE,
				path.u8string().c_str(),
				0,
				0,
				flags
			);
			if (!ret) {
				ret = BASS_AAC_StreamCreateFile(
					FALSE,
					path.u8string().c_str(),
					0,
					0,
					flags
				);
			}
		}
		else if (extension == ".tta") {
			ret = BASS_TTA_StreamCreateFile(
				FALSE,
				path.u8string().c_str(),
				0,
				0,
				flags
			);
		}
		else {
			ret = BASS_StreamCreateFile(
				FALSE,
				path.u8string().c_str(),
				0,
				0,
				flags
			);
		}
	}

	return ret;
}

// -----------------------------------------------------
// ---------------------- BASS -------------------------
// -----------------------------------------------------
void Linux::LoadBassPlugins() {
	if (!BASS_PluginLoad("./libbassflac.so", 0))
		LogError("Could not load FLAC plugin! Error code ", BASS_ErrorGetCode());
	if (!BASS_PluginLoad("./libbassape.so", 0))
		LogError("Could not load APE plugin! Error code ", BASS_ErrorGetCode());
	if (!BASS_PluginLoad("./libbasswv.so", 0))
		LogError("Could not load WavPack plugin! Error code ", BASS_ErrorGetCode());
	if (!BASS_PluginLoad("./libbassalac.so", 0))
		LogError("Could not load ALAC plugin! Error code ", BASS_ErrorGetCode());
	if (!BASS_PluginLoad("./libbass_aac.so", 0))
		LogError("Could not load AAC plugin! Error code ", BASS_ErrorGetCode());
}

// -----------------------------------------------------
// -------------------- Fullscreen ---------------------
// -----------------------------------------------------
void Linux::ToggleFullscreen() {
	if (SDL_GetWindowFlags(app->GetSdlWindow()) & SDL_WINDOW_FULLSCREEN)
		SDL_SetWindowFullscreen(app->GetSdlWindow(), 0);
	else
		SDL_SetWindowFullscreen(app->GetSdlWindow(), SDL_WINDOW_FULLSCREEN);
	
}

// =====================================================
// ===================== Virtuals ======================
// =====================================================

// -----------------------------------------------------
// ------------------ CApp Helpers ---------------------
// -----------------------------------------------------
bool Linux::OnMouseClicked(const Vector2i &mousePos) {
	if (auto toggled = app->GetControls().GetExclusiveIndicator().OnMouseClicked(mousePos)) {
		app->ToggleExclusive();
		return true;
	}

	return false;
}

// -----------------------------------------------------
// --------------- Display Properties ------------------
// -----------------------------------------------------
int Linux::GetDefaultFramebuffer() {
	return GetInterop()->GetFramebuffer();
}

#endif