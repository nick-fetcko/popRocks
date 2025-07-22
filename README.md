<img width="640" height="480" alt="Logo" src="https://github.com/user-attachments/assets/217703c1-fd27-4c57-9f08-2ed4e8278fae" />

## Preview Videos
https://github.com/user-attachments/assets/7d2a712b-d91a-4c54-96ef-f08989f107b9

https://github.com/user-attachments/assets/5a0743ee-ab15-4ea6-a5d0-0bd016124e37

## Features
- Support for most audio formats (WAV, MP3, M4A, FLAC, APE, WV)
- Support for audio input devices
- Exclusive mode (via WASAPI)
  - Gapless playback while in exclusive mode
- FFT / Oscilloscope display
- Loads ID3 / Vorbis / MP4 metadata
- Loads embedded / external album art and selects visualizer colors from the album art
  - Downscales large album art using bicubic interpolation
- Beat detection (using [BeatRoot](http://www.eecs.qmul.ac.uk/~simond/beatroot/)) cycles through visualizer colors to the beat
- Customizable FFT size, bin decay times, bin fade times, bin "pulsing" on new maximums, motion blur
  - Presets for these settings can be configured
- (Transient) playlists:
  - Entire albums (including multi-CD) can be loaded by dragging and dropping their folders into the popRocks window
  - .cue files (*excluding* multi-CD) can similarly be loaded by dragging and dropping them into the popRocks window
- [Lightpack](https://github.com/psieg/Lightpack) V1 support (for the *dozens* of us who still have functioning Lightpacks)

## What's **NOT** supported (yet)
- Operating systems other than Windows
  - Linux support is planned
  - macOS support is possible
- More niche audio formats (SACD, TTA, etc.)
- Persistent, user-defined playlists
- *All* Unicode characters (only a subset is currently supported)
- APE metadata
- Gapless playback in non-exclusive mode
- Multi-CD .cue files
- External lighting devices other than Lightpack V1

## Building
### Prerequisites
- [Visual Studio 2022](https://visualstudio.microsoft.com/vs/)
- [vcpkg](https://vcpkg.io/en/) (either Visual Studio's or external)
  - If external, set `cmakeToolchain` in [CMakeSettings](CMakeSettings.json)
  - If external, the following packages must be installed:
    - `vcpkg install sdl2:x64-windows`
    - `vcpkg install sdl2-mixer:x64-windows`
    - `vcpkg install libjpeg-turbo:x64-windows`
    - `vcpkg install sdl2-image[libjpeg-turbo]:x64-windows`
    - `vcpkg install sdl2-net:x64-windows`
    - `vcpkg install fftw3:x64-windows`
    - `vcpkg install sdl2-ttf:x64-windows`
### Instructions
1. `git clone https://github.com/nick-fetcko/popRocks`
2. `cd popRocks`
3. `git submodule update --init`
4. If using an external `vcpkg` installation, set `cmakeToolchain` in [CMakeSettings](CMakeSettings.json) to its `vckpg.cmake`
5. In Visual Studio 2022: File -> Open -> Folder
6. Select the same `popRocks` folder as in step 2
7. Wait for CMake generation to finish
8. Navigate to `popRocks/Build/x64-[Debug/Release]` and open `popRocks.sln`
9. Right-click on `popRocks` in Solution Explorer
10. Select "Set as Startup Project"
11. Build -> Build Solution or Debug -> Start Debugging / Start Without Debugging

## Using
Drag-and-drop any music / .cue file (or folder containing music / .cue files) into the popRocks window.

The included console interface supports the following commands:
|Command / arguments in []|Description|
|--|--|
|load [FILE/FOLDER]|Alternative to drag-and-drop|
|listen|Listens to the primary audio input device|
|fftline|Displays the FFT as a continuous line|
|fft|Displays the FTT bins as individual rectangles / triangles|
|osc|Displays the audio waveform as an oscilloscope|
|color [R (0-255)] [G (0-255)] [B (0-255)]|Sets / overrides the visualizer color|
|buffer [LENGTH]|Changes how much data is displayed on screen (default 2048 samples)|
|fft [LENGTH]|Changes how many bins are used for the FFT (default 8192 bins, with only 4096 of them containing *real* data)|
|rot|Toggles rotation of the center album art|
|rpm [RPM]|Changes the RPM of the rotation|
|decay [TIME (in seconds)]|Changes how fast the FFT bins return to 0 from their maximum value|
|fade [TIME (in seconds)]|Changes how quickly the FFT bins fade out after a new maximum value|
|tri|FFT bins will render as triangles|
|squ|FFT bins will render as rectangles|
|strobe [FREQUENCY (in seconds) (optional)]|Toggles consistent strobing of visualizer colors / sets the "strobe" speed|
|smooth [STEPS (0-100)]|Sets the number of smoothing steps for Prismatik to use|
|gamma [GAMMA (0.0-3.0)]|Sets the gamma for Prismatik to use|
|blur [INTENSITY (0.0-1.0) (optional)]|Toggles motion blur / sets the intensity of the motion blur|
|radius [RADIUS]|Sets the radius (in pixels) of the center album art|
|bpm|Toggles beat detection|
|width [WIDTH]|Sets the line width in oscilloscope / FFT line modes|
|rgb|Use RGB values for Lightpack integration|
|hsv|Use HSV values for Lightpack integration|
|sat [MULTIPLIER]|Change the saturation multiplier when Lightpack integration is in HSV mode|
|pulse [TIME (in seconds) (optional)]|Toggles bright "pulse" on new FFT bin maximums / changes how long the "pulse" lasts for|

To add your own presets, edit [Presets.json](Data/Presets.json).

## Contributing
Pull requests are welcomed! This project exists to make listening to music a more *fun*, active experience—so anything which furthers that goal is encouraged!

## Credits
- Logo design: Nick Fetcko
- Logo typeface: [Crumbled Pixels](https://www.1001fonts.com/crumbled-pixels-font.html) by Alyssa Diaz (under [CC BY license](https://creativecommons.org/licenses/by/4.0/))
- Application typeface: [Kurinto](https://kurinto.com/) Sans Regular (under [SIL Open Font License Version 1.1](https://openfontlicense.org/open-font-license-official-text/))
- Preview videos song: "Be Someone Forever" by [Red Vox](https://vine.bandcamp.com) (with [permission](https://www.twitch.tv/vinesauce/clip/PlainManlyChickenAMPEnergy-pXOElHJ0G4y0v6pM))
- Math and serialization libraries: [Matthew Albrecht](https://github.com/mattparks)
- [SDL2](https://www.libsdl.org/)
- [BASS libraries](https://www.un4seen.com/)
- [FFTW](https://www.fftw.org/)
- This software is based in part on the work of the Independent JPEG Group.
