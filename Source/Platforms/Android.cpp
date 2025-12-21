#ifdef __ANDROID__
#include "Android.hpp"

#include "Source/CApp.h"

// =====================================================
// ================ Factory Registration ===============
// =====================================================
bool Android::Register() {
	PlatformFactory::Register("android", [](CApp *app) {
		return std::make_unique<Android>(app);
	});

	return true;
}
bool Android::registered = Register();

// =====================================================
// =================== Implementation ==================
// =====================================================
Android::Android(CApp *app) : Platform(app) {
}

Android::~Android() {

}

// =====================================================
// =================== Pure Virtuals ===================
// =====================================================

// -----------------------------------------------------
// ------------------- CApp Helpers --------------------
// -----------------------------------------------------
void Android::OnInit(Interop::InitArgs args, Context &context) {
}

void Android::OnDestroy() {
	eglDestroyContext(display, eglContext);
	eglDestroySurface(display, eglSurface);
}

void Android::OnResize(int windowWidth, int windowHeight) {
	if (auto &context = app->GetContext()) {
		context->SetIdentity(glm::ortho(0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight), 0.0f));
		context->Apply();
	}
}

std::optional<bool> Android::OnLoop() {
	if (destroyNextFrame) {
		paused = true;

		LogDebug("Destroying EGL surface!");
		if (eglMakeCurrent(display,  EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) != EGL_TRUE)
		LogError("Couldn't disassociate EGL context! ", std::hex, glGetError());
		eglDestroySurface(display, eglSurface);
		destroyNextFrame = false;
		return std::nullopt;
	}

	if (createNextFrame) {
		CreateEGLWindowSurface();
		createNextFrame = false;
		return std::nullopt;
	}

	if (paused) return std::nullopt;

	if (loadNextLoop) {
		app->LoadFile(*loadNextLoop);
		loadNextLoop = std::nullopt;

		return std::nullopt;
	}

	return true;
}

void Android::SwapBuffers() {
	if (!paused)
		eglSwapBuffers(display, eglSurface);
}

// -----------------------------------------------------
// ------------------ Keyboard hooks -------------------
// -----------------------------------------------------
void Android::HookKeyboard() {}

// -----------------------------------------------------
// ---------------------- OpenGL -----------------------
// -----------------------------------------------------
void Android::SetGlAttributes() {
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
}

bool Android::CreateOpenGlContext() {
	InitEGL();
	return true;
}

void Android::OpenOpenGlWindow(SDL_PropertiesID &props) {

}

bool Android::LoadGlad() {
	return gladLoadGLES2Loader((GLADloadproc)eglGetProcAddress);
}

// -----------------------------------------------------
// ---------------- Device listening -------------------
// -----------------------------------------------------
void Android::Listen(bool loopback) {}
void Android::StopListening() {}
void Android::LoadHeardSamples(Renderer *renderer, float *floatBuffer, short *shortBuffer, const std::size_t bufferLength) {}

// -----------------------------------------------------
// ----------------- Exclusive mode --------------------
// -----------------------------------------------------
bool Android::OpenExclusive(const std::filesystem::path &path, const std::string &extension, bool exclusive, HSTREAM &target, bool force, const BASS_CHANNELINFO &channelInfo, void *data) {
	return false;
}
void Android::StopExclusive(bool reset) {}

// -----------------------------------------------------
// ---------------------- HDR --------------------------
// -----------------------------------------------------
std::optional<std::tuple<bool, float, float>> Android::GetHdrProperties(int display, bool force) {
	return std::make_tuple(true, 1.0f, 1.0f);
}
void Android::SetHdr(bool enabled, void *hwnd, int width, int height) {}
void Android::UpdateHdrProperties(bool force) {}

// -----------------------------------------------------
// ------------------ File Opening ---------------------
// -----------------------------------------------------
HSTREAM Android::OpenWithFlags(const std::filesystem::path &path, const std::string &extension, DWORD flags) {
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
void Android::LoadBassPlugins() {
	if (!BASS_PluginLoad((libraryPath / "libbassflac.so").u8string().c_str(), 0))
		LogError("Could not load FLAC plugin! Error code ", BASS_ErrorGetCode());
	if (!BASS_PluginLoad((libraryPath / "libbassape.so").u8string().c_str(), 0))
		LogError("Could not load APE plugin! Error code ", BASS_ErrorGetCode());
	if (!BASS_PluginLoad((libraryPath / "libbasswv.so").u8string().c_str(), 0))
		LogError("Could not load WavPack plugin! Error code ", BASS_ErrorGetCode());
	if (!BASS_PluginLoad((libraryPath / "libbassalac.so").u8string().c_str(), 0))
		LogError("Could not load ALAC plugin! Error code ", BASS_ErrorGetCode());
	if (!BASS_PluginLoad((libraryPath / "libbass_aac.so").u8string().c_str(), 0))
		LogError("Could not load AAC plugin! Error code ", BASS_ErrorGetCode());
}

// -----------------------------------------------------
// -------------------- Fullscreen ---------------------
// -----------------------------------------------------
void Android::ToggleFullscreen() {}

// =====================================================
// ===================== Virtuals ======================
// =====================================================

// -----------------------------------------------------
// ------------------ CApp Helpers ---------------------
// -----------------------------------------------------

// -----------------------------------------------------
// --------------- Display Properties ------------------
// -----------------------------------------------------
void Android::SetSafeArea(SDL_Window *window, Context &context, int w, int h) {
	SDL_Rect rect;
	SDL_GetWindowSafeArea(window, &rect);

	LogDebug("Safe area = { ", rect.x, ", ", rect.y, ", ", rect.w, ", ", rect.h, " }");

	context.SetSafeArea({rect.x, rect.y, rect.w, rect.h});
}

const float Android::GetScale(float scale) const {
	// Default scale on Android is
	// a little too high
	if (scale > 1.5f)
		scale /= 1.5f;

	return scale;
}

bool Android::IsUiInverted() { return true; }
GLenum Android::GetFboInternalFormat(bool hdrEnabled) { return GL_RGBA16F; }
bool Android::IsTouchScreen() { return true; }

// -----------------------------------------------------
// --------------------- OpenGL ------------------------
// -----------------------------------------------------
const bool Android::GetOpenGlProperty() const {
	return false;
}

// -----------------------------------------------------
// --------------------- Vulkan ------------------------
// -----------------------------------------------------
const bool Android::GetVulkanProperty() const {
	return false;
}

// -----------------------------------------------------
// --------------------- Menus -------------------------
// -----------------------------------------------------
void Android::AddMenuCallbacks(Menu *menu) {
	menu->SetOnHdrChanged([this](bool hdr) {
		Settings::settings.SetHdr(hdr);

		HDR::Enabled = hdr;

		DestroyEGLWindowSurface();
		RecreateEGLWindowSurface();
	});
	menu->SetOnColorspaceChanged([this](std::string colorspace) {
		Settings::settings.SetColorspace(colorspace);

		DestroyEGLWindowSurface();
		RecreateEGLWindowSurface();
	});
}

// -----------------------------------------------------
// ---------------------- Audio ------------------------
// -----------------------------------------------------
bool Android::PlayAfterLoad() {
	return !loadNextLoop || (loadNextLoop && playNextLoop);
}

// =====================================================
// ================== Android helpers ==================
// =====================================================
void Android::CreateEGLWindowSurface() {
	std::vector<EGLAttrib> attributes;

	if (auto colorspace = Settings::GetEnumForColorspace(Settings::settings.GetColorspace()); HDR::Enabled && colorspace) {
		attributes.push_back(EGL_GL_COLORSPACE_KHR);
		attributes.push_back(*colorspace);
		attributes.push_back(EGL_NONE);
	}

	auto win = (ANativeWindow*)SDL_GetPointerProperty (SDL_GetWindowProperties(app->GetSdlWindow()), SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, NULL);

	eglSurface = eglCreatePlatformWindowSurface(display, config,  win, attributes.empty() ? NULL : attributes.data());
	if (eglSurface == EGL_NO_SURFACE)
		LogError("Failed to create surface: ", std::hex, eglGetError());
	else LogDebug("Added surface to window ", win);

	const EGLint contextAttribs[] = {
			EGL_CONTEXT_CLIENT_VERSION, 3,
			EGL_CONTEXT_MAJOR_VERSION, 3,
			EGL_CONTEXT_MINOR_VERSION, 2,
			EGL_NONE
	};

	if (!eglContext) {
		eglContext = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
		if (eglContext == EGL_NO_CONTEXT)
			LogError("Failed to create context: ", std::hex, eglGetError());
	}

	if (eglMakeCurrent(display, eglSurface,  eglSurface, eglContext) != EGL_TRUE)
		LogError("Failed to make context current: ", std::hex, eglGetError());

	paused = false;
}

void Android::RecreateEGLWindowSurface() {
	createNextFrame = true;
}

void Android::InitEGL() {
	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	EGLint majorVersion, minorVersion;
	if (eglInitialize(display, &majorVersion, &minorVersion) != EGL_TRUE) {
		LogError("Could not initialize EGL! ", std::hex, eglGetError());
	}

	LogDebug("EGL version: ", majorVersion, ".", minorVersion);

	auto extensions = eglQueryString(display, EGL_EXTENSIONS);
	auto split = Utils::Split(std::string(extensions, extensions + strlen(extensions)), ' ');
	for (auto &&extension : split) {
		if (extension.find("EGL_EXT_gl_colorspace") == 0) {
			app->GetMenu().AddColorspace(std::move(extension));
		}
	}

	const EGLint attribs[] = {
			EGL_RED_SIZE, 16,
			EGL_GREEN_SIZE, 16,
			EGL_BLUE_SIZE, 16,
			EGL_ALPHA_SIZE, 16,
			EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,
			EGL_COLOR_COMPONENT_TYPE_EXT, EGL_COLOR_COMPONENT_TYPE_FLOAT_EXT,
			EGL_NONE
	};

	EGLint numConfigs;
	if (eglChooseConfig(display, attribs, &config, 1, &numConfigs) || (numConfigs < 1) != EGL_TRUE) {
		LogError("Could not choose EGL config! ", std::hex, eglGetError());
	}

	CreateEGLWindowSurface();
}
void Android::DestroyEGLWindowSurface() {
	destroyNextFrame = true;
}
void Android::LoadFileNextLoop(std::filesystem::path path, bool andPlay) {
	loadNextLoop = path;
	playNextLoop = andPlay;
}
#endif