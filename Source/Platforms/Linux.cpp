#if defined(__linux__) && !defined(__ANDROID__)

#include "Source/Platforms/Linux.hpp"

#include <unistd.h>

#include <linux/input-event-codes.h>

#include <SDL3/SDL.h>

#include <bassalac.h>
#include <bass_aac.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

#include <spa-0.2/spa/param/props.h>
#include <spa-0.2/spa/pod/vararg.h>

#include "Source/CApp.h"

#define DEBUG_RASTERIZATION 0

constexpr std::string_view DesktopFileTemplate = 
R"([Desktop Entry]
Version=0.9
Name=popRocks Visualizer
GenericName=popRocks
Comment=An audiovisual music player, with an emphasis on the visual
MimeType=audio/mpeg
Path=${POPROCKS_PATH}
Exec=popRocks
Type=Application
Terminal=false
Categories=AudioVideo;Audio
Icon=${POPROCKS_ICON}
MimeType=audio/flac;audio/ogg;audio/mpeg;audio/mp4)";

constexpr std::string_view ServiceMenuTemplate = 
R"([Desktop Entry]
Type=Service
MimeType=inode/directory
Actions=visualizeWithPopRocks

[Desktop Action visualizeWithPopRocks]
Name=Visualize with popRocks
Icon=${POPROCKS_ICON}
Exec=bash -c 'cd ${POPROCKS_PATH} && ./popRocks "$1"' -- %f)";

const std::string PathPlaceholder = "${POPROCKS_PATH}";
const std::string IconPlaceholder = "${POPROCKS_ICON}";

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
Linux::Linux(CApp *app) : Desktop(app), mpris(app) {
	CreateInterop();
}

Linux::~Linux() {
	if (!lastTempFile.empty())
		unlink(lastTempFile.c_str());
}

// =====================================================
// =================== Pure Virtuals ===================
// =====================================================

// -----------------------------------------------------
// ------------------- CApp Helpers --------------------
// -----------------------------------------------------
void Linux::OnInit(Interop::InitArgs args, Context &context) {
	char path[PATH_MAX];
	std::size_t count = readlink("/proc/self/exe", path, PATH_MAX);

	if (count != -1) {
		const auto pathPath = std::filesystem::path(path, path + count);
		const auto iconPath = (pathPath.parent_path() / "Data" / "popRocks.png").u8string();
		const auto basePath = pathPath.parent_path().u8string();

		// Create .desktop file
		auto desktopPath = std::filesystem::path(getenv("HOME")) / ".local" / "share" / "applications";
		if (!std::filesystem::exists(desktopPath))
			std::filesystem::create_directories(desktopPath);
		
		if (std::filesystem::exists(desktopPath)) {
			desktopPath /= "popRocks.desktop";

			const auto pathStart = DesktopFileTemplate.find(PathPlaceholder);
			const auto iconStart = DesktopFileTemplate.find(IconPlaceholder);

			std::string desktopFileContents =
				std::string(
					DesktopFileTemplate.begin(),
					DesktopFileTemplate.begin() + pathStart
				);

			desktopFileContents += basePath;

			desktopFileContents += 
				std::string(
					DesktopFileTemplate.begin() + pathStart + PathPlaceholder.length(),
					DesktopFileTemplate.begin() + iconStart
				);

			desktopFileContents += iconPath;

			desktopFileContents +=
				std::string(
					DesktopFileTemplate.begin() + iconStart + IconPlaceholder.length(),
					DesktopFileTemplate.end()
				);

			std::ofstream outFile(desktopPath);
			outFile << desktopFileContents << std::endl;
		}

		// Create servicemenu .desktop file
		auto serviceMenuPath = std::filesystem::path(getenv("HOME")) / ".local" / "share" / "kio" / "servicemenus";
		if (!std::filesystem::exists(serviceMenuPath))
			std::filesystem::create_directories(serviceMenuPath);

		if (std::filesystem::exists(serviceMenuPath)) {
			serviceMenuPath /= "visualizeWithPopRocks.desktop";

			const auto iconStart = ServiceMenuTemplate.find(IconPlaceholder);
			const auto pathStart = ServiceMenuTemplate.find(PathPlaceholder);

			std::string serviceMenuContents =
				std::string(
					ServiceMenuTemplate.begin(),
					ServiceMenuTemplate.begin() + iconStart
				);

			serviceMenuContents += iconPath;

			serviceMenuContents +=
				std::string(
					ServiceMenuTemplate.begin() + iconStart + IconPlaceholder.length(),
					ServiceMenuTemplate.begin() + pathStart
				);

			serviceMenuContents += basePath;

			serviceMenuContents +=
				std::string(
					ServiceMenuTemplate.begin() + pathStart + PathPlaceholder.length(),
					ServiceMenuTemplate.end()
				);

			std::ofstream outFile(serviceMenuPath);
			outFile << serviceMenuContents << std::endl;
			outFile.close();

			std::filesystem::permissions(
				serviceMenuPath,
				std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec,
				std::filesystem::perm_options::add
			);
		}
	}

	HookWindow(miniPlayer);

	HookKeyboard();

	if (app->GetVulkan())
		Desktop::OnInit(args, context);
}

void Linux::OnResize(int windowWidth, int windowHeight) {
	if (auto &context = app->GetContext()) {
		if (!app->GetVulkan()) {
			context->SetIdentity(glm::ortho(0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight), 0.0f));
			context->Apply();
		} else {
			Desktop::OnResize(windowWidth, windowHeight);
		}
	}
}

void Linux::OnDestroy() {
	StopPipeWire();
	
	DestroyInterop();

	if (listening) {
		if (listenThread.joinable())
			listenThread.join();
		listening = false;
	}

	DestroyWindow(app->GetSdlWindow());

	mpris.Disable();
	mpris.OnDestroy();
}

std::optional<bool> Linux::OnLoop() {
	mpris.OnLoop();

	if (loop)
		pw_loop_iterate(loop, 0);

	if (app->GetVulkan())
		return interop->OnLoop();

	return true;
}

void Linux::SwapBuffers() {
	SDL_GL_SwapWindow(app->GetSdlWindow());
}

// -----------------------------------------------------
// ------------------ Keyboard hooks -------------------
// -----------------------------------------------------
void Linux::HookKeyboard() {
	if (!Settings::settings.GetCaptureKeyboardMediaKeys() && mpris.IsEnabled())
		mpris.Disable();
	else if (Settings::settings.GetCaptureKeyboardMediaKeys() && !mpris.IsEnabled())
		mpris.Enable();
}

// -----------------------------------------------------
// ---------------------- OpenGL -----------------------
// -----------------------------------------------------
void Linux::OpenOpenGlWindow(SDL_PropertiesID &props) {
	if (app->GetVulkan())
		Desktop::OpenOpenGlWindow(props);
}

bool Linux::CreateOpenGlContext() {
	app->SetOpenGlContext(SDL_GL_CreateContext(
		app->GetVulkan() ? 
			app->GetOpenGlWindow() :
			app->GetSdlWindow()
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
// --------------------- Interops ----------------------
// -----------------------------------------------------
void Linux::DestroyInterop() {
	if (interop) {
		interop->OnDestroy();
		delete interop;
		interop = nullptr;
	}
}

void Linux::CreateInterop() {
	if (app->GetVulkan())
		interop = new Vulkan();
}

// -----------------------------------------------------
// ----------------- Exclusive mode --------------------
// -----------------------------------------------------
bool Linux::OpenExclusive(const std::filesystem::path &path, const std::string &extension, bool exclusive, HSTREAM &target, HSTREAM &visualTarget, bool force, const BASS_CHANNELINFO &channelInfo, void *data) {
	exclusiveBufferSize = 0.0f;
	if (exclusiveChannelInfo.freq == 0 || channelInfo.freq != exclusiveChannelInfo.freq) {
		if (exclusiveChannelInfo.freq != 0) StopExclusive(TRUE);

		int argc = 0;

		uint8_t buffer[1024];
		struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
		const struct spa_pod *params[1];

		const auto audioInfo = SPA_AUDIO_INFO_RAW_INIT(
			.format = SPA_AUDIO_FORMAT_F32,
			.flags = 0,
			.rate = channelInfo.freq,
			.channels = channelInfo.chans
		);

		params[0] = spa_format_audio_raw_build(
			&b,
			SPA_PARAM_EnumFormat,
			&audioInfo
		);

		exclusiveChannelInfo = channelInfo;

		BASS_Free();
		
		GetDefaultDevice();
		
		pw_init(&argc, nullptr);

		pwData.loop = pw_main_loop_new(NULL);
		loop = pw_main_loop_get_loop(pwData.loop);

		context = pw_context_new(
			loop,
			NULL,
			0
		);

		core = pw_context_connect(
			context,
			NULL,
			0
		);

		while(defaultSinkName.empty())
			pw_loop_iterate(loop, 0);
		
		streamEvents.version = PW_VERSION_STREAM_EVENTS;
		streamEvents.state_changed = &Linux::PipeWireStateChanged;
		streamEvents.process = &Linux::PipeWireProcess;

		pwData.stream = pw_stream_new_simple(
			loop,
			"popRocks Visualizer",
			pw_properties_new(
				PW_KEY_MEDIA_TYPE, "Audio",
				PW_KEY_MEDIA_CATEGORY, "Playback",
				PW_KEY_MEDIA_ROLE, "Music",
				PW_KEY_NODE_EXCLUSIVE, "true",
				PW_KEY_NODE_DONT_RECONNECT, "true",
				PW_KEY_NODE_TARGET, defaultSinkName.c_str(),
				NULL
			),
			&streamEvents,
			this
		);

		auto ret = pw_stream_connect(
			pwData.stream,
			PW_DIRECTION_OUTPUT,
			PW_ID_ANY,
			static_cast<pw_stream_flags>(
				PW_STREAM_FLAG_AUTOCONNECT |
				PW_STREAM_FLAG_MAP_BUFFERS |
				PW_STREAM_FLAG_EXCLUSIVE |
				PW_STREAM_FLAG_RT_PROCESS
			),
			params,
			1
		);

		if (ret >= 0) {
			while (streamState != PW_STREAM_STATE_STREAMING &&
				   streamState != PW_STREAM_STATE_UNCONNECTED)
				pw_loop_iterate(loop, 0);

			if (streamState != PW_STREAM_STATE_UNCONNECTED) {
				BASS_Init(
					0 /* no sound */,
					app->GetFreq() /* technically doesn't matter */,
					0,
					0,
					nullptr
				);

				target = OpenWithFlags(path, extension, BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);
				visualTarget = OpenWithFlags(path, extension, BASS_STREAM_PRESCAN | BASS_STREAM_DECODE);

				return true;
			} else {
				StopPipeWire();

				BASS_Init(
					Platform::GetDeviceIndex<true>(Settings::settings.GetOutputDevice()),
					app->GetFreq(),
					0,
					0,
					nullptr
				);

				return false;
			}
		} else {
			StopPipeWire();

			BASS_Init(
				Platform::GetDeviceIndex<true>(Settings::settings.GetOutputDevice()),
				app->GetFreq(),
				0,
				0,
				nullptr
			);

			return false;
		}
	} else {
		target = OpenWithFlags(path, extension, BASS_STREAM_DECODE | BASS_SAMPLE_FLOAT);
		visualTarget = OpenWithFlags(path, extension, BASS_STREAM_PRESCAN | BASS_STREAM_DECODE);

		return true;
	}

	return false;
}

void Linux::StopExclusive(bool reset) {
	if (reset) {
		StopPipeWire();

		BASS_Free();
		BASS_Init(
			Platform::GetDeviceIndex<true>(Settings::settings.GetOutputDevice()),
			app->GetFreq(),
			0,
			0,
			nullptr
		);
	} else {
		pw_stream_set_active(pwData.stream, false);
	}

	exclusiveChannelInfo = {0, 0, 0, 0, 0, 0, 0, nullptr};
	exclusiveBufferBytes = 0;
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

std::filesystem::path Linux::GetTemporaryFile(const std::string &pattern) {
	if (!lastTempFile.empty())
		unlink(lastTempFile.c_str());

	auto tempFileName = new char[pattern.length() + 1];
	memcpy(tempFileName, pattern.c_str(), pattern.length());
	tempFileName[pattern.length()] = '\0';
	mkstemp(tempFileName);

	lastTempFile = std::string(tempFileName, tempFileName + pattern.length());

	delete[] tempFileName;

	return lastTempFile;
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

// -----------------------------------------------------
// -------------------- Miniplayer ---------------------
// -----------------------------------------------------
void Linux::SetChromaKey(bool enabled) {
	if (enabled == colorKeyEnabled) return;

	colorKeyEnabled = enabled;

	//LogDebug("ColorKey ", enabled ? "Enabled" : "Disabled");
}

void Linux::SetMiniPlayer(bool miniPlayer, uint8_t chromaKey) {
	this->miniPlayer = miniPlayer;
	UpdateWindowShape();
}

bool Linux::SetTransparent(bool transparent) {
	return true;
}

bool Linux::AllowsWindowMovement() const {
	return false;
}

std::optional<Vector2i> Linux::SetWindowPos(int x, int y, int width, int height) {
	Vector2i ret = {x, y};

	app->OnResize(
		Settings::settings.GetMiniPlayerWidth(),
		Settings::settings.GetMiniPlayerHeight(),
		app->GetScale(),
		true
	);

	return ret;
}

// =====================================================
// ===================== Virtuals ======================
// =====================================================

// -----------------------------------------------------
// ------------------ CApp Helpers ---------------------
// -----------------------------------------------------
bool Linux::OnMouseClicked(const Vector2i &mousePos) {
	return false;
}

bool Linux::OnMouseDown(const Vector2i &mousePos) {
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
	return app->GetVulkan() ? GetInterop()->GetFramebuffer() : 0;
}

// -----------------------------------------------------
// ------------------ Exclusive Mode -------------------
// -----------------------------------------------------
bool Linux::StopPlayingExclusive() {
	if (app->GetControls().GetExclusiveIndicator().IsExclusive()) {
		const auto active = streamState == PW_STREAM_STATE_PAUSED;

		pw_stream_set_active(pwData.stream, active);
		app->SetPlaying(active);

		return true;
	}

	return false;
}

std::size_t Linux::GetAvailable() const {
	pw_time time;
	pw_stream_get_time_n(pwData.stream, &time, sizeof(pw_time));

	const int64_t elapsed = 
		(time.rate.denom * (pw_stream_get_nsec(pwData.stream) - time.now)) /
		(time.rate.num * SPA_NSEC_PER_SEC);

	const auto currentTime = (time.ticks + elapsed) * 1000 * time.rate.num / time.rate.denom;

	return (queueSize - (currentTime - lastQueueTime)) * // Milliseconds
		(exclusiveChannelInfo.freq / 1000.0) * // (mono) samples per millisecond
		exclusiveChannelInfo.chans * // total channels
		sizeof(float); // sample size
}

// -----------------------------------------------------
// ------------------ Mouse Pointer --------------------
// -----------------------------------------------------
bool Linux::IsPointerInWindow() const {
	return inWindow;
}

// -----------------------------------------------------
// ---------------- Window Management ------------------
// -----------------------------------------------------
bool Linux::IsMoving() const {
	return moving;
}

bool Linux::IsResizing() const {
	return resizing;
}

void Linux::UpdateWindowShape() {
	// If we don't have a wl_surface yet, ignore
	if (!surface) return;

	if (!miniPlayer) {
		LogDebug("Resetting input region!");
		wl_surface_set_input_region(surface, nullptr);
		wl_surface_commit(surface);

		return;
	}

	const auto start = std::chrono::system_clock::now();

	struct wl_region *region = wl_compositor_create_region(compositor);

	const auto width = app->GetWindowSize().first;
	const auto height = app->GetWindowSize().second;

	LogDebug("Updating window shape based on window size of ", width, "x", height);

#if DEBUG_RASTERIZATION
	std::vector<uint8_t> bitmap(width * height * 3);
	memset(bitmap.data(), 0xFF, width * height * 3);
#endif

	const auto baseRadius = app->GetAlbumArt().GetRadius(true);

	// Rasterize album art circle
	const auto radius = baseRadius + app->GetAlbumArt().GetOutline().GetWidth();

	for (long y = -radius; y < radius; ++y) {
		const auto dx = std::floor(std::sqrt(radius * radius - y * y));

#if DEBUG_RASTERIZATION
		for (long x = width / 2 - dx; x < width / 2 + dx; ++x) {
			const auto index = ((y + height / 2) * width + x) * 3;

			if (index < 0 || index > width * height * 3 - 3)
				continue;

			bitmap[index] = 0;
			bitmap[index + 1] = 0;
			bitmap[index + 2] = 0;
		}
#endif

		wl_region_add(
			region,
			width / 2 - dx,
			y + height / 2,
			dx * 2,
			1
		);
	}

	// Drop a square down for the close box
	const int32_t closeSize = app->GetControls().GetIconSize() / Close::GetLowestRatio();

	const Rectanglei closeRect = {
		static_cast<int32_t>(width / 2 + baseRadius) - closeSize * 2,
		static_cast<int32_t>(height / 2 - baseRadius) + closeSize / 2,
		closeSize * 2,
		closeSize * 2
	};

	wl_region_add(
		region,
		closeRect.x,
		closeRect.y,
		closeRect.w,
		closeRect.h
	);

#if DEBUG_RASTERIZATION
	for (auto x = closeRect.x; x < closeRect.x + closeRect.w; ++x) {
		for (auto y = closeRect.y; y < closeRect.y + closeRect.h; ++y) {
			const auto index = (y * width + x) * 3;

			bitmap[index] = 0;
			bitmap[index + 1] = 0;
			bitmap[index + 2] = 0;
		}
	}
#endif

	// Rasterize the outer ring
	const auto outerRadius = baseRadius * Settings::settings.GetMiniPlayerVisualizerRatio() / 2;
	const auto innerRadius = outerRadius - app->GetAlbumArt().GetVisualizerOutline().GetWidth() * 3;
	
	for (long y = -outerRadius; y < outerRadius; ++y) {
		const auto dx = std::floor(std::sqrt(outerRadius * outerRadius - y * y));
		auto innerDx = std::floor(std::sqrt(innerRadius * innerRadius - y * y));

		if (std::isnan(innerDx))
			innerDx = 0;

#if DEBUG_RASTERIZATION
		for (long x = width / 2 - dx; x < width / 2 + dx; ++x) {
			if (y > -innerRadius && y < innerRadius && x > width / 2 - innerDx && x < width / 2 + innerDx)
				continue;
				
			const auto index = ((y + height / 2) * width + x) * 3;

			if (index < 0 || index > width * height * 3 - 3)
				continue;

			bitmap[index] = 0;
			bitmap[index + 1] = 0;
			bitmap[index + 2] = 0;
		}
#endif

		// Make two rectangles
		wl_region_add(
			region,
			width / 2 - dx,
			y + height / 2,
			dx - innerDx,
			1
		);
		wl_region_add(
			region,
			width / 2 + innerDx,
			y + height / 2,
			dx - innerDx,
			1
		);
	}
	

#if DEBUG_RASTERIZATION
	lodepng::encode(
		std::filesystem::path(getenv("HOME")) / "Desktop/test.png",
		bitmap,
		width,
		height,
		LCT_RGB
	);
#endif

	wl_surface_set_input_region(surface, region);
	wl_surface_commit(surface);
	wl_region_destroy(region);

	LogDebug("Rasterizing input region took ", Duration<Microseconds>(std::chrono::system_clock::now() - start).AsSeconds(), " seconds");
}

void Linux::HookWindow(bool miniPlayer) {
	if (!miniPlayer) return;

	display = reinterpret_cast<wl_display*>(
		SDL_GetPointerProperty(
			SDL_GetWindowProperties(app->GetSdlWindow()),
			SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER,
			NULL
		)
	);

	seatListener.name = &Linux::SeatName;
	seatListener.capabilities = &Linux::SeatCapabilities;

	registryListener.global = &Linux::WaylandRegistryGlobal;
	registryListener.global_remove = &Linux::WaylandRegistryGlobalRemove;

	pointerListener.enter = &Linux::PointerEnter;
	pointerListener.leave = &Linux::PointerLeave;
	pointerListener.motion = &Linux::PointerMotion;
	pointerListener.button = &Linux::PointerButton;
	pointerListener.axis = &Linux::PointerAxis;
	pointerListener.frame = &Linux::PointerFrame;
	pointerListener.axis_source = &Linux::PointerAxisSource;
	pointerListener.axis_stop = &Linux::PointerAxisStop;
	pointerListener.axis_discrete = &Linux::PointerAxisDiscrete;
	pointerListener.axis_relative_direction = &Linux::PointerAxisRelativeDirection;
	pointerListener.axis_value120 = &Linux::PointerAxisValue120;

	xdgWmBaseListener.ping = &Linux::XdgWmBasePing;
	
	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registryListener, this);

	wl_display_roundtrip(display);

	surface = reinterpret_cast<wl_surface*>(
		SDL_GetPointerProperty(
			SDL_GetWindowProperties(app->GetSdlWindow()),
			SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER,
			NULL
		)
	);

	if (!xdgTopLevel) {
		xdgSurface = xdg_wm_base_get_xdg_surface(xdgWmBase, surface);

		xdgTopLevel = xdg_surface_get_toplevel(xdgSurface);

		xdgSurfaceListener.configure = &Linux::XdgSurfaceConfigure;

		xdg_surface_add_listener(xdgSurface, &xdgSurfaceListener, this);

		xdgTopLevelListener.configure = &Linux::XdgTopLevelConfigure;
		xdgTopLevelListener.close = &Linux::XdgTopLevelClose;

		xdg_toplevel_add_listener(xdgTopLevel, &xdgTopLevelListener, this);
		xdg_toplevel_set_title(xdgTopLevel, SDL_GetWindowTitle(app->GetSdlWindow()));

		wl_surface_commit(surface);

		auto start = std::chrono::system_clock::now();

		while (!configured) {
			wl_display_dispatch(display);

			// 5s timeout
			if (std::chrono::system_clock::now() - start >= 5s)
				break;
		}
	}
}

void Linux::DestroyWindow(SDL_Window *window) {
	if (miniPlayer) {
		if (xdgTopLevel) {
			xdg_toplevel_destroy(xdgTopLevel);
			xdgTopLevel = nullptr;
		}

		if (xdgSurface) {
			xdg_surface_destroy(xdgSurface);
			xdgSurface = nullptr;
		}

		if (xdgWmBase) {
			xdg_wm_base_destroy(xdgWmBase);
			xdgWmBase = nullptr;
		}

		if (registry) {
			wl_registry_destroy(registry);
			registry = nullptr;
		}

		if (compositor) {
			wl_compositor_destroy(compositor);
			compositor = nullptr;
		}

		if (pointer) {
			wl_pointer_release(pointer);
			pointer = nullptr;
		}

		if (seat) {
			wl_seat_destroy(seat);
			seat = nullptr;
		}

		surface = nullptr;
		display = nullptr;
	}

	SDL_DestroyWindow(window);
}

void Linux::ShowDialogBox(const std::string &title, const std::string &message) {
	SDL_ShowSimpleMessageBox(
		SDL_MESSAGEBOX_ERROR,
		title.c_str(),
		message.c_str(),
		app->GetSdlWindow()
	);
}

bool Linux::HandleExistingWindow() {
	return false;
}

// =====================================================
// ===================== Wayland =======================
// =====================================================
void Linux::WaylandRegistryGlobal(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
	auto platform = reinterpret_cast<Linux*>(data);

	if (strcmp(interface, wl_compositor_interface.name) == 0) {
        platform->SetCompositor(
			reinterpret_cast<wl_compositor*>(
				wl_registry_bind(registry, name, &wl_compositor_interface, version)
			)
		);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        platform->SetSeat(
			reinterpret_cast<wl_seat*>(
				wl_registry_bind(registry, name, &wl_seat_interface, version)
			)
		);
        
		wl_seat_add_listener(
			platform->GetSeat(),
			&platform->GetSeatListener(),
			data
		);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		platform->SetXdgWmBase(
			reinterpret_cast<struct xdg_wm_base*>(
				wl_registry_bind(registry, name, &xdg_wm_base_interface, 1)
			)
		);
		
		xdg_wm_base_add_listener(
			platform->GetXdgWmBase(),
			&platform->GetXdgWmBaseListener(),
			data
		);
	}
}

void Linux::WaylandRegistryGlobalRemove(void *data, struct wl_registry *registry, uint32_t name) {
}

void Linux::SeatName(void *data, wl_seat *seat, const char *name) {
}

void Linux::SeatCapabilities(void *data, struct wl_seat *seat, uint32_t caps) {
    if (caps & WL_SEAT_CAPABILITY_POINTER) {
        struct wl_pointer *pointer = wl_seat_get_pointer(seat);

		auto platform = reinterpret_cast<Linux*>(data);
		platform->SetPointer(pointer);
		wl_pointer_add_listener(pointer, &platform->GetPointerListener(), data);
    }
}

void Linux::PointerEnter(void *data, wl_pointer *pointer, uint serial, wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	auto platform = reinterpret_cast<Linux*>(data);

	if (platform->IsMoving())
		platform->SetMoving(false);
	
	platform->SetPointerX(wl_fixed_to_int(surface_x));
	platform->SetPointerY(wl_fixed_to_int(surface_y));

	platform->SetInWindow(true);
}

void Linux::PointerLeave(void *data, wl_pointer *pointer, uint serial, wl_surface *surface) {
	auto platform = reinterpret_cast<Linux*>(data);
	
	if (!platform->IsMoving())
		platform->SetInWindow(false);
}

void Linux::PointerMotion(void *data, wl_pointer *pointer, uint time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	auto platform = reinterpret_cast<Linux*>(data);
	
	platform->SetPointerX(wl_fixed_to_int(surface_x));
	platform->SetPointerY(wl_fixed_to_int(surface_y));
}

void Linux::PointerButton(void *data, wl_pointer *pointer, uint serial, uint time, uint button, uint state) {
	auto platform = reinterpret_cast<Linux*>(data);

	if (button == BTN_LEFT) {
		if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
			CApp::MouseDownState state = CApp::MouseDownState::None;

			platform->GetApp()->OnMouseDown({platform->GetPointerX(), platform->GetPointerY()}, &state);

			if (state == CApp::MouseDownState::Dragging) {
				platform->LogDebug("Handing window moving to Wayland");

				platform->SetMoving(true);
				xdg_toplevel_move(
					platform->GetXdgTopLevel(),
					platform->GetSeat(),
					serial
				);
			} else if (state == CApp::MouseDownState::Resizing && 
				platform->GetApp()->GetAlbumArt().GetActiveOutline() == AlbumArt::Outline::Visualizer) {
				platform->LogDebug("Handing window resizing to Wayland");

				xdg_toplevel_resize_edge edge = XDG_TOPLEVEL_RESIZE_EDGE_NONE;

				switch(platform->GetApp()->GetAlbumArt().GetCursor()) {
				case SDL_SYSTEM_CURSOR_N_RESIZE:
					edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP;
					break;
				case SDL_SYSTEM_CURSOR_S_RESIZE:
					edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
					break;
				case SDL_SYSTEM_CURSOR_E_RESIZE:
					edge = XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
					break;
				case SDL_SYSTEM_CURSOR_W_RESIZE:
					edge = XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
					break;
				case SDL_SYSTEM_CURSOR_NE_RESIZE:
					edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
					break;
				case SDL_SYSTEM_CURSOR_NW_RESIZE:
					edge = XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
					break;
				case SDL_SYSTEM_CURSOR_SE_RESIZE:
					edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
					break;
				case SDL_SYSTEM_CURSOR_SW_RESIZE:
					edge = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
					break;
				default:
					edge = XDG_TOPLEVEL_RESIZE_EDGE_NONE;
					break;
				}

				if (edge != XDG_TOPLEVEL_RESIZE_EDGE_NONE) {
					platform->SetResizing(true);
					xdg_toplevel_resize(
						platform->GetXdgTopLevel(),
						platform->GetSeat(),
						serial,
						edge
					);
				}
			}
		} else if (state == WL_POINTER_BUTTON_STATE_RELEASED) {
			platform->SetMoving(false);
			platform->SetResizing(false);
		}
    } else if (button == BTN_RIGHT && state == WL_POINTER_BUTTON_STATE_PRESSED) {
		xdg_toplevel_show_window_menu(
			platform->GetXdgTopLevel(),
			platform->GetSeat(),
			serial,
			platform->GetPointerX(),
			platform->GetPointerY()
		);
	}
}

void Linux::PointerAxis(void *data, wl_pointer *pointer, uint time, uint axis, wl_fixed_t value) {

}

void Linux::PointerFrame(void *data, wl_pointer *pointer) {

}

void Linux::PointerAxisSource(void *data, wl_pointer *pointer, uint axis_source) {

}

void Linux::PointerAxisStop(void *data, wl_pointer *pointer, uint time, uint axis) {

}

void Linux::PointerAxisDiscrete(void *data, wl_pointer *pointer, uint axis, int discrete) {

}

void Linux::PointerAxisRelativeDirection(void *data, wl_pointer *pointer, uint32_t axis, uint32_t direction) {

}

void Linux::PointerAxisValue120(void *data, wl_pointer *pointer, uint32_t axis, int32_t value120) {

}

void Linux::XdgSurfaceConfigure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
	auto platform = reinterpret_cast<Linux*>(data);

	if (platform->GetConfigured()) {
		xdg_surface_ack_configure(xdg_surface, serial);
		wl_surface_commit(platform->GetSurface());
	} else {
		platform->SetConfigureSerial(serial);
		platform->SetConfigured(true);
	}
}

void Linux::XdgWmBasePing(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
	xdg_wm_base_pong(xdg_wm_base, serial);
}

void Linux::XdgTopLevelConfigure(void *data, struct xdg_toplevel *toplevel, int32_t width, int32_t height, struct wl_array *states) {
	auto platform = reinterpret_cast<Linux*>(data);

	uint32_t *state = reinterpret_cast<uint32_t*>(states->data);
	bool hadResize = false;
	for (std::size_t i = 0; i < states->size / sizeof(uint32_t); ++i, ++state) {
		if (*state == XDG_TOPLEVEL_STATE_RESIZING) {

			hadResize = true;

			SDL_SetWindowSize(platform->GetApp()->GetSdlWindow(), width, height);

			const auto min = std::min(width, height);

			if (platform->GetApp()->GetAlbumArt().GetActiveOutline() == AlbumArt::Outline::Art) {
				/*
				const auto radius = min / Settings::settings.GetMiniPlayerVisualizerRatio();

				platform->GetApp()->GetAlbumArt().SetRadius(radius, true);
				platform->GetApp()->GetControls().UpdateFontSize(radius);
				platform->GetApp()->GetControls().OnRadiusChanged(*platform->GetApp()->GetContext(), radius);

				Settings::settings.Save();
				*/
			} else {
				const auto radius = platform->GetApp()->GetAlbumArt().GetRadius(true);
				const auto ratio = min / radius;

				Settings::settings.SetMiniPlayerVisualizerRatio(ratio);

				platform->GetApp()->GetAlbumArt().SetRadius(radius, true);
			}
			break;
		}
	}

	if (!hadResize && platform->IsResizing()) {
		platform->SetResizing(false);
		platform->GetApp()->OnMouseUp({0, 0});
		platform->GetApp()->GetAlbumArt().OnMouseUp({0, 0});
	} 
}

void Linux::XdgTopLevelClose(void *data, struct xdg_toplevel *topLevel) {
	SDL_Event quitEvent;
	quitEvent.type = SDL_EVENT_QUIT;
	SDL_PushEvent(&quitEvent);
}

// =====================================================
// ==================== PipeWire =======================
// =====================================================
void Linux::PipeWireProcess(void *data) {
	struct Linux *platform = reinterpret_cast<Linux*>(data);

	struct pw_buffer *b;
	struct spa_buffer *buf;

	int frames, stride;
	float *dst, val;

	if ((b = pw_stream_dequeue_buffer(platform->GetPipeWireData().stream)) == NULL) {
		pw_log_warn("out of buffers: %m");
		return;
	}

	buf = b->buffer;
	if ((dst = reinterpret_cast<float *>(buf->datas[0].data)) == NULL)
		return;

	stride = sizeof(float) * platform->GetExclusiveChannelInfo().chans;
	frames = buf->datas[0].maxsize / stride;
	if (b->requested)
			frames = SPA_MIN(b->requested, frames);

	const auto length = frames * stride;

	DWORD c = 0;
	bool done = false;
	if (BASS_ChannelIsActive(platform->GetApp()->GetStreamHandle()))
		c = BASS_ChannelGetData(platform->GetApp()->GetStreamHandle(), dst, length);

	if (c < length) {
		if (auto next = platform->GetApp()->GetNextStreamHandle(); next && BASS_ChannelIsActive(next)) {
			c += BASS_ChannelGetData(next, &reinterpret_cast<uint8_t*>(dst)[c], length - c - 1);

			if (!BASS_ChannelIsActive(next)) {
				done = true;

				platform->GetApp()->StopExclusive();
			} else {
				// Update the UI on the next loop
				platform->GetApp()->AdvanceToNextTrack();
			}
		} else if (!c) {
			platform->GetApp()->StopExclusive();

			done = true;
		}
	}

	if (!done) {
		buf->datas[0].chunk->offset = 0;
		buf->datas[0].chunk->stride = stride;
		buf->datas[0].chunk->size = frames * stride;

		pw_time time;
		pw_stream_get_time_n(platform->GetPipeWireData().stream, &time, sizeof(pw_time));

		const int64_t elapsed = 
			(time.rate.denom * (pw_stream_get_nsec(platform->GetPipeWireData().stream) - time.now)) /
			(time.rate.num * SPA_NSEC_PER_SEC);

		platform->SetLastQueueTime((time.ticks + elapsed) * 1000 * time.rate.num / time.rate.denom);
		platform->SetQueueSize(frames * 1000.0 / platform->GetExclusiveChannelInfo().freq);

		pw_stream_queue_buffer(platform->GetPipeWireData().stream, b);
	}
}

void Linux::PipeWireStateChanged(
	void *data,
	pw_stream_state old,
	pw_stream_state state,
	const char *error
) {
	if (old != state) {
		auto platform = reinterpret_cast<Linux*>(data);

		std::stringstream stateStream;

		stateStream << "State changed to ";

		switch(state) {
		case PW_STREAM_STATE_ERROR: {
			const auto errorString = std::string(error, error + strlen(error));

			stateStream << "PW_STREAM_STATE_ERROR: " << errorString;
			platform->ShowDialogBox("Could not initialize exclusive mode!", "Error: " + errorString);
			break;
		} case PW_STREAM_STATE_UNCONNECTED:
			stateStream << "PW_STREAM_STATE_UNCONNECTED";
			break;
		case PW_STREAM_STATE_CONNECTING:
			stateStream << "PW_STREAM_STATE_CONNECTING";
			break;
		case PW_STREAM_STATE_PAUSED:
			stateStream << "PW_STREAM_STATE_PAUSED";
			break;
		case PW_STREAM_STATE_STREAMING:
			stateStream << "PW_STREAM_STATE_STREAMING";
			break;
		default:
			break;
		}

		platform->LogDebug(stateStream.str());
		platform->SetStreamState(state);
	}
}

void Linux::PulseAudioContextStateCallback(pa_context *c, void *data) {
    int *state = reinterpret_cast<int*>(data);
    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_READY:
            *state = 1;
            break;
        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            *state = -1;
            break;
        default:
            break;
    }
}

void Linux::PulseAudioGetServerInfoCallback(
	pa_context *c,
	const pa_server_info *i,
	void *data
) {
	auto [platform, mainloop] = *reinterpret_cast<std::pair<Linux*, pa_mainloop*>*>(data);

    if (i && i->default_sink_name) {
		platform->GetDefaultSinkName() = 
			std::string(
				i->default_sink_name, 
				i->default_sink_name + strlen(i->default_sink_name)
			);
    }
    pa_mainloop_quit(mainloop, 0);
}

void Linux::GetDefaultDevice() {
	pa_mainloop *mainloop = pa_mainloop_new();
	pa_mainloop_api *api = pa_mainloop_get_api(mainloop);
	
	int contextState = 0;
	pa_context *context = pa_context_new(api, "PipeWireDefaultSinkFinder");
	pa_context_set_state_callback(context, PulseAudioContextStateCallback, &contextState);
	
	pa_context_connect(context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL);
	
	while (contextState == 0)
		pa_mainloop_iterate(mainloop, 1, NULL);
	
	if (contextState == 1) {
		std::pair<Linux*, pa_mainloop*> data(this, mainloop);
		pa_context_get_server_info(
			context,
			PulseAudioGetServerInfoCallback,
			&data
		);

		pa_mainloop_iterate(mainloop, 1, NULL);
	}

	while(defaultSinkName.empty())
		pa_mainloop_iterate(mainloop, 1, NULL);
	
	pa_context_disconnect(context);
	pa_context_unref(context);
	pa_mainloop_free(mainloop);
}

void Linux::StopPipeWire() {
	if (loop) {
		loop = nullptr;

		pw_core_disconnect(core);
		pw_context_destroy(context);
		pw_stream_destroy(pwData.stream);
		pw_main_loop_destroy(pwData.loop);

		pw_deinit();

		context = nullptr;
		core = nullptr;
		
		pwData = {0};
	}
}

#endif