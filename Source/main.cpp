#include <iostream>
#include <locale>
#include <codecvt>

#include <SDL.h>
#include <SDL_mixer.h>
#include <bass.h>
#include <bassflac.h>

#include <backends/imgui_impl_sdl2.h>

#include "MathCPP/Duration.hpp"

#include "CApp.h"
#include "FFTRenderer.hpp"

using namespace MathsCPP;

// See https://stackoverflow.com/questions/30412951/unresolved-external-symbol-imp-fprintf-and-imp-iob-func-sdl2
#pragma comment(lib, "legacy_stdio_definitions.lib")
extern "C" { FILE __iob_func[3] = { *stdin,*stdout,*stderr }; }

int main(int argc, char *argv[]) {
	SDL_Event event;
	bool running = true;

	CApp app;

	app.OnInit();

	LoggableClass loggableClass;
	Logger logger;
	logger.SetObject(&loggableClass);

	if(argc > 1) {
		logger.LogDebug("File prepared: ", argv[1]);
		auto ascii = std::string(argv[1]);
		app.LoadFile(std::wstring(ascii.begin(), ascii.end()));
	}

	Delta time;
	Vector2i mousePos{ 0, 0 };
	bool mouseButtonDown = false;

#if GUI
	auto &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
#endif
	
	while(running) {
		while(SDL_PollEvent(&event)) {
#if GUI
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (io.WantCaptureKeyboard || io.WantCaptureMouse)
				app.UpdateUi();
#endif

			switch(event.type) {
				case SDL_QUIT:
					running = false;
					break;
				case SDL_WINDOWEVENT:
					if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
						auto window = SDL_GetWindowFromID(event.window.windowID);
						int w = 0, h = 0;
						auto scale = app.GetScale(window, &w, &h);

						app.OnResize(w, h, scale);
					} else if (event.window.event == SDL_WINDOWEVENT_MOVED) {
						Settings::settings.SetWindowX(event.window.data1);
						Settings::settings.SetWindowY(event.window.data2);
						logger.LogDebug("Window moved to (", event.window.data1, ", ", event.window.data2, ")");
					}
					break;
				case SDL_KEYDOWN:
					if (io.WantCaptureKeyboard) break;

					else if (event.key.keysym.sym == SDLK_AUDIONEXT ||
						(event.key.keysym.sym == SDLK_d && (event.key.keysym.mod & KMOD_CTRL)) ||
						(event.key.keysym.sym == SDLK_RIGHT && (event.key.keysym.mod & KMOD_CTRL)))
						app.NextTrack();
					else if (event.key.keysym.sym == SDLK_AUDIOPREV ||
						(event.key.keysym.sym == SDLK_a && (event.key.keysym.mod & KMOD_CTRL)) ||
						(event.key.keysym.sym == SDLK_LEFT && (event.key.keysym.mod & KMOD_CTRL)))
						app.PreviousTrack();
					else if ((event.key.keysym.sym == SDLK_VOLUMEUP ||
						(event.key.keysym.sym == SDLK_w && (event.key.keysym.mod & KMOD_CTRL)) ||
						(event.key.keysym.sym == SDLK_UP && (event.key.keysym.mod & KMOD_CTRL))) &&
						app.GetControls().GetExclusiveIndicator().IsExclusive())
						app.GetControls().GetVolume().VolumeUp();
					else if ((event.key.keysym.sym == SDLK_VOLUMEDOWN ||
						(event.key.keysym.sym == SDLK_s && (event.key.keysym.mod & KMOD_CTRL)) ||
						(event.key.keysym.sym == SDLK_DOWN && (event.key.keysym.mod & KMOD_CTRL))) &&
						app.GetControls().GetExclusiveIndicator().IsExclusive())
						app.GetControls().GetVolume().VolumeDown();
					else if (event.key.keysym.sym == SDLK_RIGHT ||
						event.key.keysym.sym == SDLK_d)
						app.GetAlbumArt().NextBin();
					else if (event.key.keysym.sym == SDLK_LEFT ||
						event.key.keysym.sym == SDLK_a)
						app.GetAlbumArt().PreviousBin();
					else if (event.key.keysym.sym == SDLK_UP ||
						event.key.keysym.sym == SDLK_w) {
						auto lock = app.GetAlbumArt().Lock();
						app.GetAlbumArt().ResetBin();
					} else if (event.key.keysym.sym == SDLK_p)
						app.SaveBlurFBO();
					else if (event.key.keysym.sym == SDLK_SPACE || event.key.keysym.sym == SDLK_AUDIOPLAY)
						app.TogglePlaying();
					else if (event.key.keysym.sym == SDLK_RETURN && event.key.keysym.mod & KMOD_ALT)
						app.ToggleFullscreen();
					else if (event.key.keysym.sym == SDLK_ESCAPE)
						running = false;
					else if (event.key.keysym.sym == SDLK_v)
						app.GetControls().GetVolume().ToggleVolumeControl();
					else if (event.key.keysym.sym >= SDLK_F1 && event.key.keysym.sym <= SDLK_F12)
						app.LoadPreset(event.key.keysym.sym - SDLK_F1);
					else if (event.key.keysym.sym == SDLK_s)
						app.SyncToNearestBeat();
					else if (event.key.keysym.sym == SDLK_r)
						app.LoadPreset(Preset::Random());
					break;
				case SDL_MOUSEMOTION:
					mousePos.x = static_cast<int32_t>(event.motion.x * app.GetScale());
					mousePos.y = static_cast<int32_t>(event.motion.y * app.GetScale());

					if (mouseButtonDown) {
						app.OnMouseDragged(mousePos);
					}
					break;
				case SDL_MOUSEBUTTONDOWN:
					if (event.button.button == SDL_BUTTON_LEFT
#if GUI
						&& !io.WantCaptureMouse
#endif
						&& app.OnMouseDown(mousePos)) {
						mouseButtonDown = true;
					}
					break;
				case SDL_MOUSEBUTTONUP:
					if (event.button.button == SDL_BUTTON_LEFT
#if GUI
						&& !io.WantCaptureMouse
#endif
						) {
						if (!mouseButtonDown)
							app.OnMouseClicked(mousePos);
						else
							mouseButtonDown = false;
					}
					break;
				case SDL_DROPFILE: {
					app.LoadFile(Utils::ToUTF16(const_cast<const char*>(event.drop.file)));
					SDL_free(event.drop.file);
					break;
				}
				default:
					break;
			}

			app.FadeControls(true);
		}

		app.OnLoop(time.Update());
	}

	app.OnDestroy();

	return 0;
}