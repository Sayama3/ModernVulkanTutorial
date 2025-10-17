//
// Created by ianpo on 13/10/2025.
//

#include "MVT/SlangCompiler.hpp"

#include <cassert>
#include <format>
#include <slang/slang-com-ptr.h>
#include <slang/slang-com-helper.h>

#include "MVT/Expected.hpp"


namespace MVT {
	void SlangCompiler::Initialize() {
		s_GlobalSessionResult = slang::createGlobalSession(&s_Desc, &s_GlobalSession);
	}

	void SlangCompiler::Initialize(const SlangGlobalSessionDesc &desc) {
		s_Desc = desc;
		Initialize();
	}

	void SlangCompiler::Shutdown() {
		const uint64_t compilerInUse = s_SlangCompilersInUse.load(std::memory_order::acquire);
		if (compilerInUse > 0) {
			std::cerr << "Slang ERR: Trying to shutdown but " << compilerInUse << " compilers are still in use." << std::endl;
		}

		slang::shutdown();

		s_Desc = {};
		s_GlobalSession = nullptr;
		s_GlobalSessionResult = 0;
	}

	SlangProfileID SlangCompiler::FindProfile(const char *name) {
		assert(s_GlobalSession);
		return s_GlobalSession->findProfile(name);
	}

	SlangCompiler::SlangCompiler() {
		assert(s_GlobalSession);

		// Change the profile depending on the target.
		slang::TargetDesc targetDesc{};
		targetDesc.format = SLANG_SPIRV;
		targetDesc.profile = MVT::SlangCompiler::FindProfile("spirv_1_4");

		slang::SessionDesc sessionDesc {};
		sessionDesc.targets = &targetDesc;
		sessionDesc.targetCount = 1;

		std::array<slang::CompilerOptionEntry, 1> options =
		{
			{
				slang::CompilerOptionName::EmitSpirvDirectly,
				{slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}
			}
		};
		sessionDesc.compilerOptionEntries = options.data();
		sessionDesc.compilerOptionEntryCount = options.size();

		// TODO: Add all path where there could be shaders
		static const auto c_DefaultShaderPath = (std::filesystem::current_path() / "EngineAssets/Shaders");
		static const auto c_StrDefaultShaderPath = c_DefaultShaderPath.string();
		const char* searchPaths[] = { c_StrDefaultShaderPath.c_str() };
		sessionDesc.searchPaths = searchPaths;
		sessionDesc.searchPathCount = 1;

		slang::PreprocessorMacroDesc mvtFlag = { "MVT", "1" };
		sessionDesc.preprocessorMacros = &mvtFlag;
		sessionDesc.preprocessorMacroCount = 1;

		m_SessionResult = s_GlobalSession->createSession(m_SessionDescription,&m_Session);
		if (SLANG_SUCCEEDED(m_SessionResult)) {
			s_SlangCompilersInUse.fetch_add(1, std::memory_order::release);
		}
	}

	SlangCompiler::~SlangCompiler() {
		if (SLANG_SUCCEEDED(m_SessionResult)) {
			s_SlangCompilersInUse.fetch_sub(1, std::memory_order::release);
		}
		m_Session->release(); // ?
		// m_Session->
	}

	Expected<std::vector<char>, std::string> SlangCompiler::Compile(const char* moduleName, const char* entryPointName) {
		assert(SLANG_SUCCEEDED(m_SessionResult));

		Slang::ComPtr<slang::IBlob> diagnostics;
		Slang::ComPtr<slang::IModule> module = m_Session->loadModule(moduleName, diagnostics.writeRef());

		if (diagnostics) {
			std::string error = std::format("Slang ERR: {0}", diagnostics->getBufferPointer());
			std::cerr << error << std::endl;
			return Expected<std::vector<char>, std::string>::unexpected(std::move(error));
		}

		Slang::ComPtr<slang::IEntryPoint> entryPoint;
		const auto epRes = module->findEntryPointByName(entryPointName, entryPoint.writeRef());
		assert(SLANG_SUCCEEDED(epRes));

		slang::IComponentType* components[] = { module, entryPoint };
		slang::IComponentType* program;
		const auto prgRes = m_Session->createCompositeComponentType(components, 2, &program);
		assert(SLANG_SUCCEEDED(prgRes));

		// For the reflection
		// slang::ProgramLayout* layout = program->getLayout();

		slang::IComponentType* linkedProgram = nullptr;
		slang::IBlob* diagnosticBlob = nullptr;
		const auto lknRes = program->link(&linkedProgram, &diagnosticBlob);
		assert(SLANG_SUCCEEDED(lknRes));
		if (diagnosticBlob) {
			std::cerr << "Slang Link Error: " << diagnosticBlob->getBufferPointer() << std::endl;
		}

		return Expected<std::vector<char>, std::string>::expected(std::vector<char>{});
		// linkedProgram->getTargetCode()
	}
} // MVT