#include "Logger.hpp"

#include <set>

namespace am2017s {

std::ostream* Logger::cout = &std::cout;
std::ostream* Logger::cerr = &std::cerr;

NullBuffer nullBuffer;
std::ostream Logger::nullstream(&nullBuffer);

std::set<Topic> Logger::topics{};

std::ostream& Logger::log(Topic l) {
	if(topics.count(l)) {
		return *cout;
	} else {
		return nullstream;
	}
}

std::ostream& Logger::err() {
	return *cerr;
}

}