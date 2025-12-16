#include <jni.h>
#include <string>

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "SDL3_image/SDL_image.h"

#include "../../../Source/CApp.h"
#include "../../../Source/HDR.hpp"
#include "../../../Source/Settings.hpp"
#include "../../../Utils/Utils.hpp"
#include "../../../Source/main.hpp"
#include "../../../Source/Platforms/Android.hpp"

#include <android/log.h>

JavaVM *vm;

CApp *pApp;

jmethodID openDirectoryMethod;
jobject openDirectoryListener;
int attachedStatus;
std::string libraryPath;

extern "C"
SDLMAIN_DECLSPEC int SDLCALL SDL_main(int argc, char *argv[]) {
	popRocks_main(&pApp, [&] {
		if (pApp) {
			dynamic_cast<Android*>(pApp->GetPlatform().get())->SetLibraryPath(libraryPath);
			pApp->GetMenu().SetFileOpenFunc([&] {
				JNIEnv *env;

				attachedStatus = vm->GetEnv((void **) &env, JNI_VERSION_1_6);

				if (attachedStatus == JNI_EDETACHED)
					vm->AttachCurrentThread(&env, nullptr);

				env->ExceptionClear();

				env->CallVoidMethod(openDirectoryListener, openDirectoryMethod);

				if (env->ExceptionOccurred())
					env->ExceptionClear();

				if (attachedStatus == JNI_EDETACHED)
					vm->DetachCurrentThread();
			});
		}
	});

	return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_org_fetcko_poprocks_MainActivity_setPath(JNIEnv *env, jobject thiz, jstring path) {
	jboolean isCopy = JNI_TRUE;
	auto cStr = env->GetStringUTFChars(path, &isCopy);

	auto str = std::string(cStr, cStr + strlen(cStr));
	Utils::SetResourceFolder(std::filesystem::path(str) / "assets");

	Settings::SetPath(str);

	env->ReleaseStringUTFChars(path, cStr);
}

extern "C"
JNIEXPORT void JNICALL
Java_org_fetcko_poprocks_MainActivity_setLibraryPath(JNIEnv *env, jobject thiz, jstring path) {
	jboolean isCopy = JNI_TRUE;
	auto cStr = env->GetStringUTFChars(path, &isCopy);

	libraryPath = std::string(cStr, cStr + strlen(cStr));

	env->ReleaseStringUTFChars(path, cStr);
}

extern "C" JNIEXPORT void JNICALL Java_org_fetcko_poprocks_MainActivity_setFileOpenListener(JNIEnv *env, jobject thiz, jobject listener) {
	env->GetJavaVM(&vm);

	auto objclass = env->GetObjectClass(listener);
	auto method = env->GetMethodID(objclass, "openDirectory", "()V");

	openDirectoryMethod = method;
	openDirectoryListener = env->NewGlobalRef(listener);
}

extern "C" JNIEXPORT void JNICALL Java_org_fetcko_poprocks_MainActivity_createEglSurface(JNIEnv *env, jobject thiz) {
	if (pApp)
		dynamic_cast<Android*>(pApp->GetPlatform().get())->RecreateEGLWindowSurface();
}

extern "C" JNIEXPORT void JNICALL Java_org_fetcko_poprocks_MainActivity_destroyEglSurface(JNIEnv *env, jobject thiz) {
	if (pApp)
		dynamic_cast<Android*>(pApp->GetPlatform().get())->DestroyEGLWindowSurface();
}

extern "C" JNIEXPORT void JNICALL Java_org_fetcko_poprocks_MainActivity_openFolder(JNIEnv *env, jobject thiz, jstring folder) {
	jboolean isCopy = JNI_TRUE;
	auto cStr = env->GetStringUTFChars(folder, &isCopy);

	auto str = std::string(cStr, cStr + strlen(cStr));

	if (pApp)
		dynamic_cast<Android*>(pApp->GetPlatform().get())->LoadFileNextLoop(str);

	env->ReleaseStringUTFChars(folder, cStr);
}

extern "C" JNIEXPORT void JNICALL Java_org_fetcko_poprocks_MainActivity_setHdr(JNIEnv *env, jobject thiz, jboolean hdr) {
	HDR::Capable = (hdr == JNI_TRUE);
	HDR::Enabled = (HDR::Capable && Settings::settings.GetHdr());
}