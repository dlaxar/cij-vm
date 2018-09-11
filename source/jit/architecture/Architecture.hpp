#pragma once

#include <jit/Operands.hpp>
#include <vector>

namespace am2017s {
namespace jit {

class Architecture {
};

namespace internal {

template<typename RegisterType>
std::vector<RegisterType> AMD64registers();

template<>
std::vector<RegOp> AMD64registers();

template<>
std::vector<XMMOp> AMD64registers();

}

class AMD64 : public Architecture {
public:

	template<typename RegisterType>
	static std::vector<RegisterType> registers() {
		return internal::AMD64registers<RegisterType>();
	}

	static std::vector<RegOp> callerSaved() {
		// rax, rdi, rsi, rdx, rcx, r8, r9, r10, r11
		return {
				RAX, RDI, RSI, RDX, RCX, R8, R9, R10, R11
		};
	}

	static std::vector<RegOp> calleeSaved() {
		return {
				RBX,

				R12,
				R13,
				R14,
				R15,
		};
	}

	static std::vector<XMMOp> callerSavedFloat() {
		return {
			XMM0,
			XMM1,
			XMM2,
			XMM3,
			XMM4,
			XMM5,
			XMM6,
			XMM7,
			XMM8,
			XMM9,
			XMM10,
			XMM11,
			XMM12,
			XMM13,
			XMM14
		};
	}

	static std::vector<RegOp> parameters() {
		// RDI, RSI, RDX, RCX, R8, R9
		return {
				RDI, RSI, RDX, RCX, R8, R9
		};
	}

	static std::vector<XMMOp> parametersFloat() {
		return {
			XMM0,
			XMM1,
			XMM2,
			XMM3,
			XMM4,
			XMM5,
			XMM6,
			XMM7
		};
	};
};

}
}


