#include <jit/architecture/Architecture.hpp>

namespace am2017s { namespace jit { namespace internal {

template<typename RegisterType>
std::vector<RegisterType> AMD64registers() {
	throw std::runtime_error("No specialization for the given register type");
}

template<>
std::vector<RegOp> AMD64registers() {
	return {
			RAX,
			RCX,
			RDX,
			RBX,

//			RSP,
//			RBP,

			RSI,
			RDI,

			R8,
			R9,
			R10,
			R11,
			R12,
			R13,
			R14,
			R15
	};
}

template<>
std::vector<XMMOp> AMD64registers() {
	return {
		XMM0,
		XMM1,
		XMM2,
		XMM3,
		XMM4,
		XMM5,
//		XMM6,
//		XMM7,
//		XMM8,
//		XMM9,
//		XMM10,
//		XMM11,
//		XMM12,
//		XMM13,
//		XMM14
	};
}

}}}