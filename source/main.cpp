#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include <bytecode.hpp>
#include <Options.hpp>
#include <jit/CodeBuilder.hpp>
#include <jit/FunctionManager.hpp>
#include <jit/JitEngine.hpp>
#include <interpreter/InterpretEngine.hpp>
#include <log/Logger.hpp>

using namespace am2017s;
using namespace jit;

void usage(std::string const& command)
{
	std::cout << "Usage: " << command << " (jit | version) [-d] [--log (logfile | -)] file\n";
}

bytecode::Program parseFile(std::vector<std::string> const& args, std::string const& file)
{
	if(args.size() < 3)
	{
		usage(args[0]);
		throw std::runtime_error("No input file");
	}

	return bytecode::loadBytecode(file);
}

bool startsWith(std::string const& who, std::string const& prefix)
{
	return who.length() >= prefix.length() && std::equal(prefix.begin(), prefix.end(), who.begin());
}

void configureLogger(std::vector<std::string> const& args) {
	auto all     = std::find(args.begin(), args.end(), "--log-all") != args.end();
	auto lir     = std::find(args.begin(), args.end(), "--log-lir") != args.end();
	auto llog    = std::find(args.begin(), args.end(), "--log-llog") != args.end();
	auto lrange  = std::find(args.begin(), args.end(), "--log-lrange") != args.end();
	auto llines  = std::find(args.begin(), args.end(), "--log-llines") != args.end();
	auto rlog    = std::find(args.begin(), args.end(), "--log-rlog") != args.end();
	auto rhints  = std::find(args.begin(), args.end(), "--log-rhints") != args.end();
	auto rsplit  = std::find(args.begin(), args.end(), "--log-rsplit") != args.end();
	auto machine = std::find(args.begin(), args.end(), "--log-machine") != args.end();
	auto alloc   = std::find(args.begin(), args.end(), "--log-alloc") != args.end();
	auto address = std::find(args.begin(), args.end(), "--log-address") != args.end();
	auto compile = std::find(args.begin(), args.end(), "--log-compile") != args.end();
	auto result  = std::find(args.begin(), args.end(), "--log-result") != args.end();

	if(all || lir) {
		Logger::topics.insert(Topic::LIR_INSTRUCTIONS);
	}

	if(all || llog) {
		Logger::topics.insert(Topic::LIFE_LOG);
	}

	if(all || lrange) {
		Logger::topics.insert(Topic::LIFE_RANGES);
	}

	if(all || llines) {
		Logger::topics.insert(Topic::LIFE_LINES);
	}

	if(all || rlog) {
		Logger::topics.insert(Topic::REG_LOG);
	}

	if(all || rhints) {
		Logger::topics.insert(Topic::REG_HINTS);
	}

	if(all || rsplit) {
		Logger::topics.insert(Topic::REG_SPLIT);
	}

	if(all || machine) {
		Logger::topics.insert(Topic::MACHINE);
	}

	if(all || alloc) {
		Logger::topics.insert(Topic::RUN_ALLOC);
	}

	if(all || address) {
		Logger::topics.insert(Topic::ADDRESS);
	}

	if(all || compile) {
		Logger::topics.insert(Topic::COMPILE);
	}

	if(all || result) {
		Logger::topics.insert(Topic::RESULT);
	}
}

int main(int argc, char** argv)
{
	std::vector<std::string> args(argv, argv + argc);
	std::ofstream logfile;
	NullBuffer nb;
	std::ostream nullStream(&nb);

	// no options given (only command name)
	if(args.size() <= 1)
	{
		usage(args[0]);
		return 2;
	}

	auto& mode = args[1];
	auto debug = std::find(args.begin(), args.end(), "-d") != args.end();

	auto log = std::find(args.begin(), args.end(), "--log");
	if(log != args.end()) {
		if(log + 1 == args.end()) {
			usage(args[0]);
		}

		std::string nextArg = *(log + 1);
		if(nextArg == "-") {
			Logger::cout = &std::cout;
		} else {
			logfile.open(nextArg, std::ios::out);
			Logger::cout = &logfile;
		}
	} else {
		Logger::cout = &Logger::nullstream;
	}

	configureLogger(args);

	auto& file = args.back();

	Options options;
	options.debug = debug;

	// "interpreter" starts with mode => start up interpreter
	if(startsWith("jit", mode) || startsWith("interpreter", mode))
	{
		try
		{
			auto program = parseFile(args, file);

			std::unique_ptr<Engine> engine;
			if(startsWith("jit", mode)) {
				engine = std::make_unique<JitEngine>(std::move(program), options);
			} else {
				engine = std::make_unique<interpreter::InterpretEngine>(program, options);
			}
			return engine->execute();
		}
		catch(std::exception const& e)
		{
			std::cerr << "error: " << e.what() << "\n";
			return 1;
		}
	}
	else if(startsWith("version", mode))
	{
		std::cout << argv[0] << " 0.1.0\n";
		std::cout << "This is work in progress by @dlaxar. Thanks to their contribution go out to @Paprikachu, @maxpeinhopf and @iFlow\n";
		return 0;
	}
	else
	{
		usage(argv[0]);
		return 2;
	}
}
