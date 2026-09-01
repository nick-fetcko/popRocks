#pragma once

#include "PRNG.hpp"

#include <memory>
#include <random>

template<typename T>
class MT19937 : public PRNG<T> {
public:
	MT19937() : impl(time(nullptr)) {

	}

	T Next() override {
		return static_cast<T>(impl());
	}

	std::pair<T, T> NextTwo() override {
		return { Next(), Next() };
	}

	T Max() override {
		return static_cast<T>(impl.max());
	}

private:
	static bool registered;

	std::mt19937 impl;
};