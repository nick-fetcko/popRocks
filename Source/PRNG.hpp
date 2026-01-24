#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

template<typename T>
class PRNG {
public:
	virtual T Next() = 0;
	virtual std::pair<T, T> NextTwo() = 0;
	virtual T Max() = 0;
};

template<typename T>
class PRNGFactory {
public:
	static bool Register(std::string &&prng, std::function<
		std::unique_ptr<PRNG<T>>(void *)
	> &&f) {
		return builders.emplace(std::make_pair(std::move(prng), std::move(f))).second;
	}

	static std::unique_ptr<PRNG<T>> Build(const std::string &prng, void *source) {
		return builders.at(prng)(source);
	}

	static std::vector<std::string> GetKeys() {
		std::vector<std::string> ret;
		for (const auto &[key, value] : builders)
			ret.emplace_back(key);
		return ret;
	}

private:
	static inline std::map<
		std::string,
		std::function<
			std::unique_ptr<PRNG<T>>(void *)
		>
	> builders;
};