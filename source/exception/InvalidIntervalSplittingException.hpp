#pragma once

#include <exception>

namespace am2017s { namespace  jit {

class InvalidIntervalSplittingException : public std::exception {
	const char* what() const noexcept override {
		return "One part of the splitted interval is empty";
	}
};

}}