#pragma once

#include "Serial/Node.hpp"

using namespace serial;

template<typename T>
struct DynamicGain {
	T largeStep;
	T smallStep;

	T minReset;
	T maxReset;

	bool adjustMin;
	bool adjustMax;

	bool reset;

	friend const Node &operator>>(const Node &node, DynamicGain<T> &dynamicGain) {
		node["largeStep"]->get(dynamicGain.largeStep);
		node["smallStep"]->get(dynamicGain.smallStep);

		node["minReset"]->get(dynamicGain.minReset);
		node["maxReset"]->get(dynamicGain.maxReset);

		node["adjustMin"]->get(dynamicGain.adjustMin);
		node["adjustMax"]->get(dynamicGain.adjustMax);

		node["reset"]->get(dynamicGain.reset);

		return node;
	}

	friend Node &operator<<(Node &node, const DynamicGain<T> &dynamicGain) {
		node["largeStep"]->set(dynamicGain.largeStep);
		node["smallStep"]->set(dynamicGain.smallStep);

		node["minReset"]->set(dynamicGain.minReset);
		node["maxReset"]->set(dynamicGain.maxReset);

		node["adjustMin"]->set(dynamicGain.adjustMin);
		node["adjustMax"]->set(dynamicGain.adjustMax);

		node["reset"]->set(dynamicGain.reset);

		return node;
	}

	bool operator!=(const DynamicGain<T> &right) {
		return
			largeStep != right.largeStep || smallStep != right.smallStep ||
			minReset != right.minReset || maxReset != right.maxReset ||
			adjustMin != right.adjustMin || adjustMax != right.adjustMax ||
			reset != right.reset;
	}
};