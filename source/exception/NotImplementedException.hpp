#pragma once

#include <exception>

namespace am2017s {

class NotImplementedException : public std::exception {
	const char* what() const noexcept override {
		return "This combination is not implemented yet";
	}
};

}