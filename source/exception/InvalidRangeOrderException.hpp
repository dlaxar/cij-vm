#pragma once

#include <exception>

namespace am2017s { namespace  jit {

class InvalidRangeOrderException : public std::exception {
	const char* what() const noexcept override {
		return "Ranges need to be added in reverse order";
	}
};

}}