#include <iostream>

#include "MVT/SlangCompiler.hpp"

int main() {

	// For more slang info : https://docs.shader-slang.org/en/latest/external/slang/docs/user-guide/08-compiling.html
	SlangGlobalSessionDesc globalDesc{};
	globalDesc.apiVersion = SlangLanguageVersion::SLANG_LANGUAGE_VERSION_LATEST;
	globalDesc.enableGLSL = false;

	MVT::SlangCompiler::Initialize(globalDesc);


	std::vector<char> vertex;
	std::vector<char> fragment;
	{

		MVT::SlangCompiler compiler;

		auto vertex_spirv = compiler.CompileByPath("EngineAssets/Shaders/initial.slang", "vertMain");
		auto fragment_spirv = compiler.CompileByPath("EngineAssets/Shaders/initial.slang", "fragMain");

		if (vertex_spirv.has_value()) { vertex = vertex_spirv.value(); }
		if (fragment_spirv.has_value()) { fragment = fragment_spirv.value(); }
	}

	std::cout << "Vertex : " << vertex.size() << " bytes." << std::endl;
	std::cout << "Fragment : " << fragment.size() << " bytes." << std::endl;

	MVT::SlangCompiler::Shutdown();
	return 0;
}