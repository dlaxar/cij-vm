#pragma once

#include <memory>

#include <types.hpp>

namespace am2017s { namespace jit
{
	class CodeSegment
	{
		std::shared_ptr<void> _address;
		i64 _size;

	public:
		CodeSegment()
			: _address(nullptr), _size(0)
		{}

		CodeSegment(std::shared_ptr<void> const& address, i64 size)
			: _address(address), _size(size)
		{}

		void* address()
		{
			return _address.get();
		}

		void const* address() const
		{
			return _address.get();
		}

		i64 size() const
		{
			return _size;
		}

		void markWritable();
		void markExecutable();
	};
}}
