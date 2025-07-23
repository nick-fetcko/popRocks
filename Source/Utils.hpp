#pragma once

#include <filesystem>
#include <fstream>
#include <optional>
#include <typeindex>

namespace Fetcko {
class Utils {
public:
	static std::string GetStringFromFile(const std::filesystem::path &path);
	static std::filesystem::path GetResource(const std::filesystem::path &path);

	enum class BOM {UTF_8, UTF_16_BE, UTF_16_LE};

	template<typename C>
	static std::optional<BOM> GetBom(std::basic_istream<C> &in) {
		C chars[3];
		in.read(chars, 3);

		const auto bytes = reinterpret_cast<const uint8_t *>(chars);

		if (bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
			// FIXME: if C is wchar_t, we need to seek to character... 1.5
			return BOM::UTF_8;
		} else if (bytes[0] == 0xFE && bytes[1] == 0xFF) {
			if constexpr (std::is_same<C, wchar_t>::value)
				in.seekg(1);
			else
				in.seekg(-1, std::ios::cur);

			return BOM::UTF_16_BE;
		} else if (bytes[0] == 0xFF && bytes[1] == 0xFE) {
			if constexpr (std::is_same<C, wchar_t>::value)
				in.seekg(1);
			else
				in.seekg(-1, std::ios::cur);

			return BOM::UTF_16_LE;
		}

		in.seekg(0, std::ios::beg);

		return std::nullopt;
	}

	template<typename T>
	static std::vector<std::basic_string<T>> Split(const std::basic_string<T> &s, T delimiter) {
		std::vector<std::basic_string<T>> tokens;
		std::basic_stringstream<T> stream(s);
		std::basic_string<T> token;

		while (std::getline(stream, token, delimiter)) {
			tokens.emplace_back(std::move(token));
		}

		return tokens;
	}

	template<typename T>
	static std::vector<std::basic_string<T>> Split(const std::basic_string<T> &s, int(*f)(int)) {
		// The is...() functions (which are the expected 2nd argument)
		// only function on ASCII chars. In MSVC, at least, an assert
		// is triggered if we try to pass non-ASCII chars in debug mode.
		const auto wrapper = [f](int c) {
			if (c < -1 || c > 255)
				return false;

			return f(c) != 0;
		};

		std::vector<std::basic_string<T>> tokens;

		std::size_t index = 0;
		std::size_t lastIndex = 0;
		while (index < s.size()) {
			// Skip any preceding delimiter characters
			while (index < s.size() && wrapper(s.at(index)))
				lastIndex = ++index;

			while (!wrapper(s.at(index))) {
				if (++index >= s.size())
					break;
			}
			tokens.emplace_back(s.substr(lastIndex, index - lastIndex));

			// Skip any remaining delimiter characters
			while (index < s.size() && wrapper(s.at(index))) ++index;

			lastIndex = index;
		}

		return tokens;
	}

	// From https://stackoverflow.com/a/217605

	// trim from start (in place)
	template<typename C>
	static inline void ltrim(std::basic_string<C> &s) {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](C ch) {
			return !std::isspace(ch);
		}));
	}

	// trim from end (in place)
	template <typename C>
	static inline void rtrim(std::basic_string<C> &s) {
		s.erase(std::find_if(s.rbegin(), s.rend(), [](C ch) {
			return !std::isspace(ch);
		}).base(), s.end());
	}

	constexpr static std::size_t GetNumberOfDigits(std::size_t size) {
		std::size_t digits = 1;
		while (size /= 10) ++digits;

		return digits;
	}

	// From http://reedbeta.com/blog/python-like-enumerate-in-cpp17/
	template<typename T,
		typename TIter = decltype(std::begin(std::declval<T>())),
		typename = decltype(std::end(std::declval<T>()))>
	constexpr static auto Enumerate(T &&iterable) {
		struct iterator {
			size_t i;
			TIter iter;
			bool operator!=(const iterator &rhs) const { return iter != rhs.iter; }
			void operator++() { ++i; ++iter; }
			auto operator*() const { return std::tie(i, *iter); }
		};
		struct iterable_wrapper {
			T iterable;
			auto begin() { return iterator{ 0, std::begin(iterable) }; }
			auto end() { return iterator{ 0, std::end(iterable) }; }
		};
		return iterable_wrapper{ std::forward<T>(iterable) };
	}
};
}