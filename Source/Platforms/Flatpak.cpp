#include "Flatpak.hpp"

// =====================================================
// ================ Factory Registration ===============
// =====================================================
bool Flatpak::Register() {
	PlatformFactory::Register("flatpak", [](CApp *app) {
		return std::make_unique<Flatpak>(app);
	});

	return true;
}
bool Flatpak::registered = Register();

// =====================================================
// =================== Implementation ==================
// =====================================================
Flatpak::Flatpak(CApp *app) : Linux(app) {

}

Flatpak::~Flatpak() {

}

// =====================================================
// =================== Pure Virtuals ===================
// =====================================================

// BASS
void Flatpak::LoadBassPlugins() {
	if (!BASS_PluginLoad((Utils::GetResourceFolder().parent_path() / "libbassflac.so").u8string().c_str(), 0))
		LogError("Could not load FLAC plugin! Error code ", BASS_ErrorGetCode());
	if (!BASS_PluginLoad((Utils::GetResourceFolder().parent_path() / "libbassape.so").u8string().c_str(), 0))
		LogError("Could not load APE plugin! Error code ", BASS_ErrorGetCode());
	if (!BASS_PluginLoad((Utils::GetResourceFolder().parent_path() / "libbasswv.so").u8string().c_str(), 0))
		LogError("Could not load WavPack plugin! Error code ", BASS_ErrorGetCode());
	if (!BASS_PluginLoad((Utils::GetResourceFolder().parent_path() / "libbassalac.so").u8string().c_str(), 0))
		LogError("Could not load ALAC plugin! Error code ", BASS_ErrorGetCode());
	if (!BASS_PluginLoad((Utils::GetResourceFolder().parent_path() / "libbass_aac.so").u8string().c_str(), 0))
		LogError("Could not load AAC plugin! Error code ", BASS_ErrorGetCode());
}