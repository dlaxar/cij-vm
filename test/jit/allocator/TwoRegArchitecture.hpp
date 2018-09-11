#pragma once

#include <jit/architecture/Architecture.hpp>

using namespace am2017s::jit;

class TwoRegArchitecture : public Architecture {
public:
	static std::vector<RegOp> registers() {
		return {RAX,
		        RCX,
		};
	}
};