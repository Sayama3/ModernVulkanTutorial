#include <iostream>

#include "MVT/SlangCompiler.hpp"

int main() {
	MVT::SlangCompiler::AddPath(std::filesystem::current_path() / "EngineAssets/Shaders");
	MVT::SlangCompiler::Initialize();

	std::vector<char> vertex;
	std::vector<char> fragment;
	{
		auto vertex_spirv = MVT::SlangCompiler::s_Compile("initial.slang", "vertMain");
		auto fragment_spirv = MVT::SlangCompiler::s_Compile("initial.slang", "fragMain");

		if (vertex_spirv.has_value()) { vertex = vertex_spirv.value(); }
		if (fragment_spirv.has_value()) { fragment = fragment_spirv.value(); }
	}

	std::cout << "Vertex : " << vertex.size() << " bytes." << std::endl;
	std::cout << "Fragment : " << fragment.size() << " bytes." << std::endl;

	MVT::SlangCompiler::Shutdown();
	return 0;
}