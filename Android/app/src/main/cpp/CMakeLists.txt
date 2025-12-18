# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
project("main")

set(MAIN_DIR "../../../../..")

add_subdirectory(freetype)
add_subdirectory(glm)
add_subdirectory(SDL)
add_subdirectory(SDL_image)

add_subdirectory(${MAIN_DIR}/third_party/SDL_net-main ${CMAKE_BINARY_DIR}/SDL_net)

find_package(Vulkan REQUIRED)

add_subdirectory(${MAIN_DIR}/Serial/src ${CMAKE_BINARY_DIR}/Serial)
add_subdirectory(${MAIN_DIR}/OpenGL ${CMAKE_BINARY_DIR}/OpenGL)
add_subdirectory(${MAIN_DIR}/Utils ${CMAKE_BINARY_DIR}/Utils)

set(_poprocks_cpp_headers
		${MAIN_DIR}/Source/AlbumArt.hpp
		${MAIN_DIR}/Source/APE.hpp
		${MAIN_DIR}/Source/AutoFader.hpp
		${MAIN_DIR}/Source/BeatDetect.hpp
		${MAIN_DIR}/Source/Bicubic.hpp
		${MAIN_DIR}/Source/CApp.h
		${MAIN_DIR}/Source/Circle.hpp
		${MAIN_DIR}/Source/ColorChangeListener.hpp
		${MAIN_DIR}/Source/Controls.hpp
		${MAIN_DIR}/Source/Cue.hpp
		${MAIN_DIR}/Source/DoubleClick.hpp
		${MAIN_DIR}/Source/DynamicGain.hpp
		${MAIN_DIR}/Source/ExclusiveIndicator.hpp
		${MAIN_DIR}/Source/FFTLineRenderer.hpp
		${MAIN_DIR}/Source/FFTRenderer.hpp
		${MAIN_DIR}/Source/FLAC.hpp
		${MAIN_DIR}/Source/FPSCounter.hpp
		${MAIN_DIR}/Source/Gaussian.hpp
		${MAIN_DIR}/Source/HDR.hpp
		${MAIN_DIR}/Source/ID3V2.hpp
		${MAIN_DIR}/Source/imconfig.h
		${MAIN_DIR}/Source/JPEG.hpp
		${MAIN_DIR}/Source/LightPack.hpp
		${MAIN_DIR}/Source/LineRenderer.hpp
		${MAIN_DIR}/Source/main.hpp
		${MAIN_DIR}/Source/Mappings.h
		${MAIN_DIR}/Source/Menu.hpp
		${MAIN_DIR}/Source/Metadata.hpp
		${MAIN_DIR}/Source/MetadataReader.hpp
		${MAIN_DIR}/Source/MP3.hpp
		${MAIN_DIR}/Source/MP4.hpp
		${MAIN_DIR}/Source/Next.hpp
		${MAIN_DIR}/Source/OGG.hpp
		${MAIN_DIR}/Source/OscilloscopeRenderer.hpp
		${MAIN_DIR}/Source/Pause.hpp
		${MAIN_DIR}/Source/Play.hpp
		${MAIN_DIR}/Source/Playlist.hpp
		${MAIN_DIR}/Source/Preset.hpp
		${MAIN_DIR}/Source/Previous.hpp
		${MAIN_DIR}/Source/RecordAudioStream.h
		${MAIN_DIR}/Source/Renderer.hpp
		${MAIN_DIR}/Source/Settings.hpp
		${MAIN_DIR}/Source/Symbol.hpp
		${MAIN_DIR}/Source/TagLoader.hpp
		${MAIN_DIR}/Source/Text.hpp
		${MAIN_DIR}/Source/Volume.hpp
		${MAIN_DIR}/Source/WV.hpp
		${MAIN_DIR}/Source/Platforms/Platform.hpp
		${MAIN_DIR}/Source/Platforms/Android.hpp
)
set(_poprocks_cpp_sources
		${MAIN_DIR}/Source/AlbumArt.cpp
		${MAIN_DIR}/Source/BeatDetect.cpp
		${MAIN_DIR}/Source/Bicubic.cpp
		${MAIN_DIR}/Source/main.cpp
		${MAIN_DIR}/Source/CApp.cpp
		${MAIN_DIR}/Source/CApp_Commands.cpp
		${MAIN_DIR}/Source/Controls.cpp
		${MAIN_DIR}/Source/Cue.cpp
		${MAIN_DIR}/Source/FFTRenderer.cpp
		${MAIN_DIR}/Source/Gaussian.cpp
		${MAIN_DIR}/Source/ID3V2.cpp
		${MAIN_DIR}/Source/LightPack.cpp
		${MAIN_DIR}/Source/Mappings.cpp
		${MAIN_DIR}/Source/Metadata.cpp
		${MAIN_DIR}/Source/MP4.cpp
		${MAIN_DIR}/Source/Playlist.cpp
		${MAIN_DIR}/Source/Preset.cpp
		${MAIN_DIR}/Source/Settings.cpp
		${MAIN_DIR}/Source/Text.cpp
		${MAIN_DIR}/Source/Volume.cpp
		${MAIN_DIR}/Source/Platforms/Android.cpp
		${MAIN_DIR}/Source/Platforms/Platform.cpp
)

# Beatroot source obtained from
#   https://code.soundsoftware.ac.uk/projects/beatroot-vamp
set(_beatroot_dir
		${MAIN_DIR}/third_party/beatroot)

set(_beatroot_headers
		${_beatroot_dir}/Agent.h
		${_beatroot_dir}/AgentList.h
		${_beatroot_dir}/BeatRootProcessor.h
		${_beatroot_dir}/BeatTracker.h
		${_beatroot_dir}/Event.h
		${_beatroot_dir}/Induction.h
		${_beatroot_dir}/Peaks.h
)
set(_beatroot_sources
		${_beatroot_dir}/Agent.cpp
		${_beatroot_dir}/AgentList.cpp
		${_beatroot_dir}/BeatRootProcessor.cpp
		${_beatroot_dir}/BeatTracker.cpp
		${_beatroot_dir}/Induction.cpp
		${_beatroot_dir}/Peaks.cpp
)

set(_imgui_dir
		${MAIN_DIR}/imgui)

set(_imgui_headers
		${_imgui_dir}/imgui.h
		${_imgui_dir}/imgui_internal.h
		${_imgui_dir}/imstb_rectpack.h
		${_imgui_dir}/imstb_textedit.h
		${_imgui_dir}/imstb_truetype.h
		${_imgui_dir}/misc/cpp/imgui_stdlib.h
		${_imgui_dir}/misc/freetype/imgui_freetype.h
)

set(_imgui_sources
		${_imgui_dir}/imgui.cpp
		${_imgui_dir}/imgui_demo.cpp
		${_imgui_dir}/imgui_draw.cpp
		${_imgui_dir}/imgui_tables.cpp
		${_imgui_dir}/imgui_widgets.cpp
		${_imgui_dir}/misc/cpp/imgui_stdlib.cpp
		${_imgui_dir}/misc/freetype/imgui_freetype.cpp
)

set(_imgui_backend_headers
		${_imgui_dir}/backends/imgui_impl_opengl3_loader.h
		${_imgui_dir}/backends/imgui_impl_opengl3.h
		${_imgui_dir}/backends/imgui_impl_sdl3.h
)

set(_imgui_backend_sources
		${_imgui_dir}/backends/imgui_impl_opengl3.cpp
		${_imgui_dir}/backends/imgui_impl_sdl3.cpp
)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
#
# In this top level CMakeLists.txt, ${CMAKE_PROJECT_NAME} is used to define
# the target library name; in the sub-module's CMakeLists.txt, ${PROJECT_NAME}
# is preferred for the same purpose.
#
# In order to load a library into your app from Java/Kotlin, you must call
# System.loadLibrary() and pass the name of the library defined here;
# for GameActivity/NativeActivity derived applications, the same library name must be
# used in the AndroidManifest.xml file.
add_library(${CMAKE_PROJECT_NAME} SHARED
		# List C/C++ source files with relative paths to this CMakeLists.txt.
		${_beatroot_headers}
		${_imgui_headers}
		${_imgui_backend_headers}
		${_poprocks_cpp_headers}
		${_beatroot_sources}
		${_imgui_sources}
		${_imgui_backend_sources}
		${_poprocks_cpp_sources}
		native-lib.cpp)

find_path(GLES3_INCLUDE_DIR GLES3/gl3.h
		HINTS ${ANDROID_NDK})
find_library(GLES3_LIBRARY libGLESv3.so
		HINTS ${GLES3_INCLUDE_DIR}/../lib)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
		${MAIN_DIR}
		${MAIN_DIR}/third_party/bass/include
		${MAIN_DIR}/third_party/bassflac/include
		${MAIN_DIR}/third_party/bassape/include
		${MAIN_DIR}/third_party/basswv/include
		${MAIN_DIR}/third_party/basstta/include
		${MAIN_DIR}/third_party/bassalac/include
		${MAIN_DIR}/third_party/bassaac/include
		${MAIN_DIR}/third_party/beatroot
		${MAIN_DIR}/imgui
		${MAIN_DIR}/imgui/backends
		${MAIN_DIR}/imgui/misc/cpp
		${MAIN_DIR}/imgui/misc/freetype
		${GLES3_INCLUDE_DIR}
		)

set(LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${CMAKE_BUILD_TYPE}/${CMAKE_ANDROID_ARCH_ABI})
set(BASS "${LIB_PATH}/libbass.so")
set(BASS_FLAC "${LIB_PATH}/libbassflac.so")
set(BASS_WV "${LIB_PATH}/libbasswv.so")
set(BASS_TTA "${LIB_PATH}/libbass_tta.so")
set(BASS_ALAC "${LIB_PATH}/libbassalac.so")
set(BASS_AAC "${LIB_PATH}/libbass_aac.so")
set(BASS_APE "${LIB_PATH}/libbassape.so")

# Specifies libraries CMake should link to your target library. You
# can link libraries from various origins, such as libraries defined in this
# build script, prebuilt third-party libraries, or Android system libraries.
target_link_libraries(${CMAKE_PROJECT_NAME}
		# List libraries link to the target library
		android
		log
		glm::glm
		Utils
		OpenGL
		serial
		freetype
		${BASS}
		${BASS_FLAC}
		${BASS_WV}
		${BASS_TTA}
		${BASS_ALAC}
		${BASS_AAC}
		${BASS_APE}
		EGL
		${GLES3_LIBRARY}
		Vulkan::Vulkan
		SDL3::SDL3
		SDL3_image::SDL3_image
		SDL3_net::SDL3_net )