#pragma once

#include <string>
#include <exception>
#include <stdexcept>

namespace am2017s { namespace bytecode {

class TypeNotPackedException : public std::runtime_error {

public:
	TypeNotPackedException(std::string what) : std::runtime_error(what) {}
};

}}