#include <iostream>
#include <locale>
#include <codecvt>

#include <SDL.h>
#include <SDL_mixer.h>
#include <bass.h>
#include <bassflac.h>

#include "MathCPP/Duration.hpp"

#include "CConsole.h"
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

	if(argc > 1) {
		CConsole::Console.Print(std::string("File prepared: ").append(std::string(argv[1])), MSG_DIAG);
		auto ascii = std::string(argv[1]);
		app.LoadFile(std::wstring(ascii.begin(), ascii.end()));
	}

	Delta time;
	Vector2i mousePos{ 0, 0 };
	
	while(running) {
		while(SDL_PollEvent(&event)) {
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
					}
					break;
				case SDL_KEYDOWN:
					if (event.key.keysym.sym == SDLK_RIGHT)
						app.GetAlbumArt().NextBin();
					else if (event.key.keysym.sym == SDLK_LEFT)
						app.GetAlbumArt().PreviousBin();
					else if (event.key.keysym.sym == SDLK_UP)
						app.GetAlbumArt().ResetBin();
					else if (event.key.keysym.sym == SDLK_p) {
						if (auto fftRenderer = dynamic_cast<const FFTRenderer *>(app.GetRenderer()))
							fftRenderer->PrintMax();
					} else if (event.key.keysym.sym == SDLK_AUDIONEXT)
						app.NextTrack();
					else if (event.key.keysym.sym == SDLK_AUDIOPREV)
						app.PreviousTrack();
					else if (event.key.keysym.sym == SDLK_VOLUMEUP && app.GetControls().GetExclusiveIndicator().IsExclusive())
						app.GetControls().GetVolume().VolumeUp();
					else if (event.key.keysym.sym == SDLK_VOLUMEDOWN && app.GetControls().GetExclusiveIndicator().IsExclusive())
						app.GetControls().GetVolume().VolumeDown();
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
					break;
				case SDL_MOUSEMOTION:
					mousePos.x = static_cast<int32_t>(event.motion.x * app.GetScale());
					mousePos.y = static_cast<int32_t>(event.motion.y * app.GetScale());
					break;
				case SDL_MOUSEBUTTONUP:
					if (event.button.button == SDL_BUTTON_LEFT) {
						app.OnMouseClicked(mousePos);
					}
					break;
				case SDL_DROPFILE: {
					// We only need to explicitly convert from UTF-8 to UTF-16 here
					std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
					app.LoadFile(converter.from_bytes(const_cast<const char*>(event.drop.file)));
					SDL_free(event.drop.file);
					break;
				}
				default:
					break;
			}

			app.FadeControls(true);
		}

		CConsole::Console.Read();
		app.OnLoop(time.Update());
	}

	app.OnDestroy();

	return 0;
}