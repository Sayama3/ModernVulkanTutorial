#include <iostream>

#include "MVT/SlangCompiler.hpp"

int main() {

	// For more slang info : https://docs.shader-slang.org/en/latest/external/slang/docs/user-guide/08-compiling.html
	SlangGlobalSessionDesc globalDesc{};
	globalDesc.apiVersion = SlangLanguageVersion::SLANG_LANGUAGE_VERSION_LATEST;
	globalDesc.enableGLSL = false;

	MVT::SlangCompiler::Initialize(globalDesc);

	std::cout << "Hello, World!" << std::endl;


	MVT::SlangCompiler::Shutdown();
	return 0;
}
