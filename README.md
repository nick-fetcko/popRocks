<img width="812" height="377" alt="banner-transparent" src="https://github.com/user-attachments/assets/14b24229-eaf4-4b63-9331-75b0d7633d2a"  />
<img width="720" height="720" alt="preview" src="https://github.com/user-attachments/assets/0c0aa4bb-fe14-4161-8772-d65e3a843d75" />

## Installation
- Windows .exe file coming soon!
- Flathub package coming soon!

## Using
Drag-and-drop any music / .cue file (or folder containing music / .cue files) into the popRocks window.

On first launch, a help interface appears:

<img width="662" height="662" alt="popRocks-help" src="https://github.com/user-attachments/assets/66d82cf9-a172-4227-85e3-c72c555820ee" />

## What popRocks IS
- A desktop audio player for Windows and Linux
  - Supports lossy formats: MP3, M4A, OGG
  - Supports lossless formats: FLAC, Apple Lossless, Monkey's Audio, WavPack, TTA
  - Reads embedded / external album art—with user overrides available
  - Uses WASAPI / PipeWire for bit-perfect, exclusive audio playback
- A visual-first experience
  - FFT and oscilloscope options available for visualization
  - Colors found in album art theme the overall interface
  - BeatRoot beat detection (optionally) changes colors to the song's beat
  - Unique, circular design with transparent visualizer elements
  - Visualizer frames persist temporally, with shaders determining how old frames blend with new
  - Infinitely resizable / draggable
- Lightweight
  - 100% C++ / OpenGL
  - 0-2% CPU usage during normal playback (with visualization)
  - ~100MB RAM footprint (dependent on size of album / album art)
  - No Internet access
- Responsive
  - Designed to run at refresh rates ≥ 60Hz while maintaining low CPU usage
  - Uses operating system's features
    - Keyboard media key hooking 
    - Windows Taskbar status / live preview thumbnail
    - Unity LauncherEntity status / MPRIS
  - Discord integration

## What popRocks ISN'T (yet)
- A full-fledged visualizer editor
  - A **beta** editor interface does currently exist
  - Visualizers have full access to GLSL shaders, but those must be edited externally
- A visualizer for streaming music (YouTube, Spotify, Apple Music, Tidal, etc.)
  - Support is theoretically possible using a loopback audio device
  - BeatRoot algorithm will need to be modified to support real-time beat detection
  - Integrations with each streaming service's API need to be implemented
    - These integrations *will* violate "No Internet access" as listed above
- A music library manager
  - Only individual album playlists are supported
- Fully HDR compatible
  - **Beta** visualizer editor *does* support HDR (via **beta** DXGI / Vulkan interops), but player interface doesn't

## What popRocks ISN'T (and will never be)
- Compatible with other visualizers (e.g. Milkdrop)
  - These formats do not account for a song's beats or transparent interfaces
- An ID3 tag editor
  - A complex tag editing UI would be antithetical to popRocks' design
- Anything even adjacent to AI / GenAI / LLMs
  - popRocks could not have been made without *human* creativity

## Building (Windows)
### Prerequisites
- [Visual Studio 2022](https://visualstudio.microsoft.com/vs/)
- [vcpkg](https://vcpkg.io/en/) (either Visual Studio's or external)
  - If external, set `cmakeToolchain` in [CMakeSettings](CMakeSettings.json)
  - If external, the following packages must be installed:
    - `vcpkg install sdl3[vulkan]:x64-windows`
    - `vcpkg install sdl3-image[jpeg,png,webp,tiff]:x64-windows`
    - `vcpkg install glm:x64-windows`
    - `vcpkg install fftw3:x64-windows`
    - `vcpkg install freetype:x64-windows`
### Instructions
1. `git clone https://github.com/nick-fetcko/popRocks`
2. `cd popRocks`
3. `git submodule update --init --recursive`
4. If using an external `vcpkg` installation, set `cmakeToolchain` in [CMakeSettings](CMakeSettings.json) to its `vckpg.cmake`
5. In Visual Studio 2022: File -> Open -> Folder
6. Select the same `popRocks` folder as in step 2
7. Wait for CMake generation to finish
8. Navigate to `popRocks/Build/x64-[Debug/Release]` and open `popRocks.sln`
9. Right-click on `popRocks` in Solution Explorer
10. Select "Set as Startup Project"
11. Build -> Build Solution or Debug -> Start Debugging / Start Without Debugging

## Building (Linux Flatpak)
### Prerequisites
- [flatpak-builder](https://docs.flatpak.org/en/latest/flatpak-builder.html)
### Instructions
1. `git clone https://github.com/nick-fetcko/popRocks`
2. `cd popRocks`
3. `git submodule update --init --recursive`
4. `flatpak-builder --force-clean --user --install-deps-from=flathub --repo=repo --install builddir org.fetcko.popRocks.yml`
5. OR, if flatpak-builder is installed as a Flatpak: `flatpak run org.flatpak.Builder --force-clean --user --install-deps-from=flathub --repo=repo --install builddir org.fetcko.popRocks.yml`

## Troubleshooting (Windows)
- If using **Rectify11** / **Mica For Everyone** and player isn't transparent
  - Add a Process Rule for popRocks to **Mica For Everyone** and set "Backdrop Type" to "None"
- If using *beta* Vulkan interop on an NVIDIA GPU and player isn't transparent
  - Under "Manage 3D settings" in the NVIDIA Control Panel, find "Vulkan/OpenGL present method" at the bottom and change it to "Native"
- A "G-SYNC" indicator is visible next to the player on an NVIDIA GPU
  - Select "Change Resolution" in the NVIDIA Control Panel, find the "Display" menu up top and deselect "G-SYNC, G-SYNC Compatible Indicator"
  - OR, to add an individual exception for popRocks, use [NVIDIA Profile Inspector](https://github.com/Orbmu2k/nvidiaProfileInspector)

## Contributing
Pull requests are welcomed—so long as the requester is human! This project exists to make listening to music a more *fun*, active experience; anything which furthers that goal is encouraged!

## Credits
- Logo design: Nick Fetcko
- Application / logo typeface: [Kurinto](https://kurinto.com/) Sans Regular (under [SIL Open Font License Version 1.1](https://openfontlicense.org/open-font-license-official-text/))
- Math and serialization libraries: [Matthew Albrecht](https://github.com/mattparks)
- [SDL3](https://www.libsdl.org/)
- [BASS libraries](https://www.un4seen.com/)
- [FFTW](https://www.fftw.org/)
- This software is based in part on the work of the Independent JPEG Group.
