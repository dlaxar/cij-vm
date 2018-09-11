#pragma once

#include <exception>

namespace am2017s { namespace  jit { namespace allocator {

class StackModificationException : public std::exception {
	const char* what() const noexcept override {
		return "stack modified after it has been frozen";
	}
};

}}}