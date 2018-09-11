#pragma once

#include <Engine.hpp>
#include <bytecode.hpp>
#include <Options.hpp>
#include <chrono>

namespace am2017s { namespace interpreter {


union Value {
	bool b;
	i8 byte;
	i16 s;
	i32 i;
	i64 l;

	float f;
	double d;

	void* ref;
};

class InterpretEngine : public Engine {

private:

	using Clock = std::chrono::high_resolution_clock;

	bytecode::Program program;
	Options options;

	std::vector<Value> global;

	void executeFunction(u16 idx, u16 *args, Value *prevFrame, u16 retIdx);

public:
	Clock::time_point _beginReal;

	InterpretEngine(bytecode::Program program, Options const& options);
	virtual int execute() override final;

};

}}
