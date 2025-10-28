#include <iostream>

#include "MVT/Application.hpp"
#include "MVT/SlangCompiler.hpp"

#include <iostream>
#include <stdexcept>
#include <cstdlib>

int main() {
	MVT::SlangCompiler::AddPath(std::filesystem::current_path() / "EngineAssets/Shaders");
	MVT::SlangCompiler::Initialize();

	try {
		std::unique_ptr<MVT::Application> app = std::make_unique<MVT::Application>();
		app->run();
		app.reset();
	} catch (const std::exception &e) {
		std::cerr << e.what() << std::endl;
		MVT::SlangCompiler::Shutdown();
		return EXIT_FAILURE;
	}

	MVT::SlangCompiler::Shutdown();

	return EXIT_SUCCESS;
}
