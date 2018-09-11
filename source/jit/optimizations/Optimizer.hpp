#pragma once

#include <bytecode.hpp>

namespace am2017s { namespace jit {

class Optimizer {
private:
	bytecode::Function const& function;

	std::vector<bool> purgeUnusedInstructions();

public:
	Optimizer(bytecode::Function const& _function);
	std::vector<bool> run() &&;

};

}}


