#include "MT19937.hpp"

template<>
bool MT19937<unsigned int>::registered = PRNGFactory<unsigned int>::Register("MT19937", [](void *source) {
	return std::make_unique<MT19937<unsigned int>>();
});