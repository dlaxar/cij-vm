#pragma once

#include <iostream>
#include <set>

namespace am2017s {

class NullBuffer : public std::streambuf {
public:
	int overflow(int c) override { return c; }
};

enum Topic {
	/**
	 * Output the result of the LIR compiler
	 */
	LIR_INSTRUCTIONS,

	/**
	 * General information about lifetime analysis
	 */
	LIFE_LOG,

	/**
	 * Life ranges
	 */
	LIFE_RANGES,

	/**
	 * Life lines
	 */
	LIFE_LINES,

	/**
	 * General information about register allocation
	 */
	REG_LOG,

	/**
	 * Information about register hints
	 */
	REG_HINTS,

	/**
	 * Information about lifetime splits
	 */
	REG_SPLIT,

	/**
	 * Information during machine compilation
	 */
	MACHINE,

	/**
	 * Allocations at runtime
	 */
	RUN_ALLOC,

	/**
	 * Runtime addresses
	 */
	ADDRESS,

	/**
	 * Compilation starts
	 */
	COMPILE,

	/**
	 * Output of the target programm
	 */
	RESULT,
};

class Logger {
public:
	static std::ostream *cout;
	static std::ostream *cerr;
	static std::ostream nullstream;

	static std::set<Topic> topics;

	static std::ostream& log(Topic l);
	static std::ostream& err();

	static void allTopics();
};

}

