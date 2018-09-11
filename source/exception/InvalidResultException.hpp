#pragma once

#include <exception>

namespace am2017s {

class InvalidResultException : public std::exception {
	const char* what() const noexcept override {
		return "No valid result could be computed at the given circumstances";
	}
};

}