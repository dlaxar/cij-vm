#pragma once

#include <chrono>
#include <time.h>

#include <bytecode.hpp>
#include <Engine.hpp>
#include <Options.hpp>
#include <jit/CodeHeap.hpp>
#include <jit/FunctionManager.hpp>
#include <jit/architecture/Architecture.hpp>

namespace am2017s { namespace jit
{
	class JitEngine : public Engine
	{
		using Clock = std::chrono::high_resolution_clock;

		bytecode::Program _program;
		FunctionManager _fmgr;
		std::vector<void*> _functionTable;

		Options _options;

	public:
		JitEngine(bytecode::Program program, Options const& options);
		virtual int execute() override final;
		void* compile(u16 index);

		clock_t _beginCpu;
		Clock::time_point _beginReal;

		static
		i32 specialFunctionIndex(u16);
	};
}}
