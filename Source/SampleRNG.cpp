#include "SampleRNG.hpp"

template<>
bool SampleRNG<unsigned int>::registered = PRNGFactory<unsigned int>::Register("Audio Samples", [](void *source) {
	return std::make_unique<SampleRNG<unsigned int>>(reinterpret_cast<HSTREAM*>(source));
});