#pragma once

namespace am2017s
{
	struct Engine
	{
		Engine(Engine const&) = delete;
		Engine& operator =(Engine const&) = delete;

		virtual int execute() = 0;
		virtual ~Engine() = default;

	protected:
		Engine() = default;
	};
}
