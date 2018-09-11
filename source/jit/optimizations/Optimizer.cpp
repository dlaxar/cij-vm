#include "Optimizer.hpp"

namespace am2017s { namespace jit {


Optimizer::Optimizer(bytecode::Function const& _function) : function(_function) {}

std::vector<bool> Optimizer::run()&& {
	return purgeUnusedInstructions();
}

std::vector<bool> Optimizer::purgeUnusedInstructions() {
	std::vector<bool> used(function.temporyCount, false);
	std::vector<bool> skip(function.instructions.size(), false);

	// todo disabled for debugging purposes
	// todo this won't work for loops (i think)
//	for(auto instruction = function.instructions.rbegin(); instruction != function.instructions.rend(); ++instruction) {
//		if(instruction->dstIdx() && instruction->isPure()) {
//			if(!used[instruction->dstIdx().value()]) {
//				skip[instruction->id] = true;
//			}
//		}
//
//		if(!skip[instruction->id]) {
//			for(u16 temporary : instruction->inputOperands()) {
//				used[temporary] = true;
//			}
//		}
//	}

	return skip;
}


}}