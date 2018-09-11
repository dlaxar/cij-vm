#include <cstring>
#include <utility>

#include <jit/CodeBuilder.hpp>

namespace am2017s { namespace jit
{
	// undefined instruction 'ud2'
	constexpr u8 const ud2[] = {0x0F, 0x0B};

	std::vector<u8> CodeBuilder::build()
	{
		auto size = _buf.size();
		_buf.resize(size + sizeof ud2);

		// guaranteed crash if no terminator at the end of the function
		std::memcpy(_buf.data() + size, ud2, sizeof ud2);

		return std::move(_buf);
	}
}}
