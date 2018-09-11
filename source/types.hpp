#pragma once

#include <cstdint>
#include <experimental/optional>

namespace std { inline namespace literals { inline namespace string_literals {}}}

namespace am2017s
{
	using namespace std::string_literals;

	template <typename T>
	using Optional = std::experimental::optional<T>;

	using u8  = std::uint8_t;
	using u16 = std::uint16_t;
	using u32 = std::uint32_t;
	using u64 = std::uint64_t;

	using i8  = std::int8_t;
	using i16 = std::int16_t;
	using i32 = std::int32_t;
	using i64 = std::int64_t;
}
