//
// Created by ianpo on 13/10/2025.
//

#include "MVT/SlangCompiler.hpp"

#include <cassert>
#include <format>
#include <array>
#include <memory>
#include <string>

#include <slang/slang-com-ptr.h>
#include <slang/slang-com-helper.h>

#include "MVT/Expected.hpp"

#define MVT_ARG1(arg1, ...) arg1
#define MVT_ARG2(arg1, arg2, ...) arg2
#define MVT_SLANG_RETURN_ON_FAIL(...) \
{ \
	SlangResult _res = MVT_ARG1(__VA_ARGS__); \
	if (SLANG_FAILED(_res)) \
	{ \
		std::string message = MVT_ARG2(__VA_ARGS__, "Slang ERR: Slang command failed."); \
		return MVT::Expected<std::vector<char>, std::string>::unexpected(std::move(message)); \
	} \
}

namespace MVT {
	std::optional<std::string> checkDiagnostics(Slang::ComPtr<slang::IBlob> diagnosticsBlob) {
		if (diagnosticsBlob != nullptr) {
			return (const char *) diagnosticsBlob->getBufferPointer();
		}

		return std::nullopt;
	}

	void diagnoseIfNeeded(Slang::ComPtr<slang::IBlob> diagnosticsBlob, const char *preMsg = "Slang Diag ERR: ", const char *postMsg = "") {
		if (diagnosticsBlob != nullptr) {
			std::cerr << preMsg << (const char *) diagnosticsBlob->getBufferPointer() << postMsg << std::endl;
		}
	}

	void diagnoseIfNeeded(slang::IBlob *diagnosticsBlob, const char *preMsg = "Slang Diag ERR: ", const char *postMsg = "") {
		if (diagnosticsBlob != nullptr) {
			std::cerr << preMsg << (const char *) diagnosticsBlob->getBufferPointer() << postMsg << std::endl;
		}
	}

	void SlangCompiler::Initialize() {
		s_GlobalSessionResult = slang::createGlobalSession(&s_GlobalSession);
		if (SLANG_FAILED(s_GlobalSessionResult)) {
			std::cerr << "Slang ERR: Failed to create the global session. (Facility :" << SLANG_GET_RESULT_FACILITY(s_GlobalSessionResult) << " ; Code :" << SLANG_GET_RESULT_CODE(s_GlobalSessionResult) << ")" << std::endl;
		}

		s_MainCompiler = std::make_unique<SlangCompiler>();
	}

	void SlangCompiler::Initialize(const SlangGlobalSessionDesc &desc) {
		s_Desc = desc;
		Initialize();
	}

	void SlangCompiler::Shutdown() {
		s_MainCompiler.reset();

		const uint64_t compilerInUse = s_SlangCompilersInUse.load(std::memory_order::acquire);
		if (compilerInUse > 0) {
			std::cerr << "Slang ERR: Trying to shutdown but " << compilerInUse << " compilers are still in use." <<
					std::endl;
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

	Expected<std::vector<char>, std::string> SlangCompiler::s_Compile(const char *shaderName, const char *entryPoint) {
		return s_MainCompiler->Compile(shaderName, entryPoint);
	}

	Expected<std::vector<char>, std::string> SlangCompiler::s_CompileByPath(const std::filesystem::path &shaderPath, const char *entryPoint) {
		return s_MainCompiler->CompileByPath(shaderPath, entryPoint);
	}

	Expected<std::vector<char>, std::string> SlangCompiler::s_Compile(const char *shaderName) {
		return s_MainCompiler->Compile(shaderName);
	}

	Expected<std::vector<char>, std::string> SlangCompiler::s_CompileByPath(const std::filesystem::path &shaderPath) {
		return s_MainCompiler->CompileByPath(shaderPath);
	}

	Expected<std::vector<char>, std::string> SlangCompiler::s_OneShotCompile(const char *shaderName, const char *entryPoint) {
		SlangCompiler compiler{};
		return compiler.Compile(shaderName, entryPoint);
	}

	Expected<std::vector<char>, std::string> SlangCompiler::s_OneShotCompileByPath(const std::filesystem::path &shaderPath, const char *entryPoint) {
		SlangCompiler compiler{};
		return compiler.CompileByPath(shaderPath, entryPoint);
	}

	Expected<std::vector<char>, std::string> SlangCompiler::s_OneShotCompile(const char *shaderName) {
		SlangCompiler compiler{};
		return compiler.Compile(shaderName);
	}

	Expected<std::vector<char>, std::string> SlangCompiler::s_OneShotCompileByPath(const std::filesystem::path &shaderPath) {
		SlangCompiler compiler{};
		return compiler.CompileByPath(shaderPath);
	}

	void SlangCompiler::ResetCompiler() {
		s_MainCompiler = std::make_unique<SlangCompiler>();
	}

	SlangCompiler::SlangCompiler() : m_SessionDescription{} {
		assert(s_GlobalSession);

		static SlangProfileID spirv_1_4 = MVT::SlangCompiler::FindProfile("spirv_1_4");

		// Change the profile depending on the target.
		slang::TargetDesc targetDesc{};
		targetDesc.format = SlangCompileTarget::SLANG_SPIRV;
		targetDesc.profile = spirv_1_4;
		m_SessionDescription.targets = &targetDesc;
		m_SessionDescription.targetCount = 1;

		std::array<slang::PreprocessorMacroDesc, 1> preprocessorMacroDesc =
		{
			slang::PreprocessorMacroDesc{"MVT", "1"},
		};
		m_SessionDescription.preprocessorMacros = preprocessorMacroDesc.data();
		m_SessionDescription.preprocessorMacroCount = preprocessorMacroDesc.size();

		std::array<slang::CompilerOptionEntry, 1> options =
		{
			{
				slang::CompilerOptionName::EmitSpirvDirectly,
				{slang::CompilerOptionValueKind::Int, 1, 0, nullptr, nullptr}
			}
		};
		m_SessionDescription.compilerOptionEntries = options.data();
		m_SessionDescription.compilerOptionEntryCount = options.size();

		const uint32_t count = s_SearchPaths.size();
		std::vector<const char *> searchPaths{count};
		for (uint32_t i = 0; i < count; ++i) {
			searchPaths[i] = s_SearchPaths[i].c_str();
		}
		m_SessionDescription.searchPaths = searchPaths.data();
		m_SessionDescription.searchPathCount = count;

		m_SessionResult = s_GlobalSession->createSession(m_SessionDescription, m_Session.writeRef());
		if (SLANG_SUCCEEDED(m_SessionResult)) {
			s_SlangCompilersInUse.fetch_add(1, std::memory_order::release);
		}
	}

	SlangCompiler::~SlangCompiler() {
		if (SLANG_SUCCEEDED(m_SessionResult)) {
			s_SlangCompilersInUse.fetch_sub(1, std::memory_order::release);
		}
	}

	Expected<std::vector<char>, std::string> SlangCompiler::Compile(const char *shaderName, const char *entryPointName) {
		assert(SLANG_SUCCEEDED(m_SessionResult));

		Slang::ComPtr<slang::IBlob> diagnostics;
		Slang::ComPtr<slang::IModule> slangModule;
		auto mdl = m_Session->loadModule(shaderName, diagnostics.writeRef());
		slangModule.attach(mdl);


		auto msg = checkDiagnostics(diagnostics);
		if (msg) {
			std::cerr << std::format("Compile Error [{0}] [{1}]\n", shaderName, entryPointName) << msg.value() << std::endl;
			return std::move(msg.value());
		}

		if (!mdl) {
			std::string error = std::format("Slang ERR: module '{}' not found.", shaderName);
			std::cerr << error << std::endl;
			return Expected<std::vector<char>, std::string>::unexpected(std::move(error));
		}

		return CompileModule(std::move(slangModule), shaderName, entryPointName);
	}

	Expected<std::vector<char>, std::string> SlangCompiler::CompileByPath(const std::filesystem::path &shaderPath, const char *entryPointName) {
		assert(SLANG_SUCCEEDED(m_SessionResult));

		const auto pStr = shaderPath.string();
		if (!std::filesystem::exists(shaderPath)) {
			std::string error = std::format("Slang ERR: The shader '{0}' doesn't exist", pStr);
			std::cerr << error << std::endl;
			return std::move(error);
		}

		const std::uintmax_t file_size = std::filesystem::file_size(shaderPath);
		if (file_size == static_cast<std::uintmax_t>(-1)) {
			std::string error = std::format("File ERR: The shader '{0}' couldn't be sized.", pStr);
			std::cerr << error << std::endl;
			return std::move(error);
		}

		std::string content(file_size, '\0'); {
			std::ifstream shaderFile;
			shaderFile.open(shaderPath);

			if (!shaderFile) {
				std::string error = std::format("Slang ERR: The shader '{0}' couldn't be opened.", pStr);
				std::cerr << error << std::endl;
				return std::move(error);
			}

			shaderFile.read(content.data(), file_size);
		}

		const std::string moduleName = shaderPath.filename().string();

		Slang::ComPtr<slang::IBlob> diagnostics;
		Slang::ComPtr<slang::IModule> slangModule;

		// auto mdl = m_Session->loadModule(moduleName.c_str(), diagnostics.writeRef());

		auto mdl = m_Session->loadModuleFromSourceString(moduleName.c_str(), pStr.c_str(), content.c_str(), diagnostics.writeRef());
		slangModule.attach(mdl);

		auto msg = checkDiagnostics(diagnostics);
		if (msg) {
			std::cerr << std::format("Compile Error [{0}] [{1}]\n", pStr, entryPointName) << msg.value() << std::endl;
			return std::move(msg.value());
		}

		if (!mdl) {
			std::string error = std::format("Slang ERR: module '{}' not found.", moduleName);
			std::cerr << error << std::endl;
			return Expected<std::vector<char>, std::string>::unexpected(std::move(error));
		}

		return CompileModule(std::move(slangModule), moduleName.c_str(), entryPointName);
	}

	Expected<std::vector<char>, std::string> SlangCompiler::Compile(const char *shaderName) {
		assert(SLANG_SUCCEEDED(m_SessionResult));

		Slang::ComPtr<slang::IBlob> diagnostics;
		Slang::ComPtr<slang::IModule> slangModule;
		auto mdl = m_Session->loadModule(shaderName, diagnostics.writeRef());
		slangModule.attach(mdl);


		auto msg = checkDiagnostics(diagnostics);
		if (msg) {
			std::cerr << std::format("Compile Error [{0}]\n", shaderName) << msg.value() << std::endl;
			return std::move(msg.value());
		}

		if (!mdl) {
			std::string error = std::format("Slang ERR: module '{}' not found.", shaderName);
			std::cerr << error << std::endl;
			return Expected<std::vector<char>, std::string>::unexpected(std::move(error));
		}

		return CompileModule(std::move(slangModule), shaderName);
	}

	Expected<std::vector<char>, std::string> SlangCompiler::CompileByPath(const std::filesystem::path &shaderPath) {
		assert(SLANG_SUCCEEDED(m_SessionResult));

		const auto pStr = shaderPath.string();
		if (!std::filesystem::exists(shaderPath)) {
			std::string error = std::format("Slang ERR: The shader '{0}' doesn't exist", pStr);
			std::cerr << error << std::endl;
			return std::move(error);
		}

		const std::uintmax_t file_size = std::filesystem::file_size(shaderPath);
		if (file_size == static_cast<std::uintmax_t>(-1)) {
			std::string error = std::format("File ERR: The shader '{0}' couldn't be sized.", pStr);
			std::cerr << error << std::endl;
			return std::move(error);
		}

		std::string content(file_size, '\0'); {
			std::ifstream shaderFile;
			shaderFile.open(shaderPath);

			if (!shaderFile) {
				std::string error = std::format("Slang ERR: The shader '{0}' couldn't be opened.", pStr);
				std::cerr << error << std::endl;
				return std::move(error);
			}

			shaderFile.read(content.data(), file_size);
		}

		const std::string moduleName = shaderPath.filename().string();

		Slang::ComPtr<slang::IBlob> diagnostics;
		Slang::ComPtr<slang::IModule> slangModule;

		// auto mdl = m_Session->loadModule(moduleName.c_str(), diagnostics.writeRef());

		auto mdl = m_Session->loadModuleFromSourceString(moduleName.c_str(), pStr.c_str(), content.c_str(), diagnostics.writeRef());
		slangModule.attach(mdl);

		auto msg = checkDiagnostics(diagnostics);
		if (msg) {
			std::cerr << std::format("Compile Error [{0}]\n", pStr) << msg.value() << std::endl;
			return std::move(msg.value());
		}

		if (!mdl) {
			std::string error = std::format("Slang ERR: module '{}' not found.", moduleName);
			std::cerr << error << std::endl;
			return Expected<std::vector<char>, std::string>::unexpected(std::move(error));
		}
		return CompileModule(std::move(slangModule), moduleName.c_str());
	}

	void SlangCompiler::AddPath(const std::filesystem::path &path) {
		s_SearchPaths.emplace_back(path.generic_string());
	}

	Expected<std::vector<char>, std::string> SlangCompiler::CompileModule(Slang::ComPtr<slang::IModule> slangModule, const char *moduleName, const char *entryPointName) {
		if (!slangModule) {
			std::string error = std::format("Slang ERR: module '{}' not found.", moduleName);
			std::cerr << error << std::endl;
			return Expected<std::vector<char>, std::string>::unexpected(std::move(error));
		}

		Slang::ComPtr<slang::IEntryPoint> entryPoint;
		const auto epRes = slangModule->findEntryPointByName(entryPointName, entryPoint.writeRef()); {
			SlangResult _res = epRes;
			if (((_res) < 0)) {
				std::string message = std::format("Slang ERR: Error getting entry point {} in module {}", entryPointName, moduleName);
				return MVT::Expected<std::vector<char>, std::string>::unexpected(std::move(message));
			}
		}
		// MVT_SLANG_RETURN_ON_FAIL(epRes)

		// For the reflection
		// ReflectModule(program->getLayout());

		std::array<slang::IComponentType *, 2> componentTypes =
		{
			slangModule,
			entryPoint
		};

		Slang::ComPtr<slang::IComponentType> composedProgram; {
			Slang::ComPtr<slang::IBlob> diagnosticsBlob;
			SlangResult result = m_Session->createCompositeComponentType(
				componentTypes.data(),
				componentTypes.size(),
				composedProgram.writeRef(),
				diagnosticsBlob.writeRef());
			diagnoseIfNeeded(diagnosticsBlob); {
				SlangResult _res = result;
				if (((_res) < 0)) {
					std::string message = "Slang ERR: Failed to create a composite component.";
					return MVT::Expected<std::vector<char>, std::string>::unexpected(std::move(message));
				}
			};
			// MVT_SLANG_RETURN_ON_FAIL(result);
		}

		Slang::ComPtr<slang::IComponentType> linkedProgram; {
			Slang::ComPtr<slang::IBlob> diagnosticsBlob;
			SlangResult result = composedProgram->link(
				linkedProgram.writeRef(),
				diagnosticsBlob.writeRef());
			diagnoseIfNeeded(diagnosticsBlob); {
				SlangResult _res = result;
				if (((_res) < 0)) {
					std::string message = "Slang ERR: Failed to link the program.";
					return MVT::Expected<std::vector<char>, std::string>::unexpected(std::move(message));
				}
			};
			// MVT_SLANG_RETURN_ON_FAIL(result);
		}


		Slang::ComPtr<slang::IBlob> spirvCode; {
			Slang::ComPtr<slang::IBlob> diagnosticsBlob;
			SlangResult result = linkedProgram->getEntryPointCode(
				0, // entryPointIndex
				0, // targetIndex
				spirvCode.writeRef(),
				diagnosticsBlob.writeRef());
			diagnoseIfNeeded(diagnosticsBlob); {
				SlangResult _res = result;
				if (((_res) < 0)) {
					std::string message = "Slang ERR: Failed to fetch the SpirV code.";
					return MVT::Expected<std::vector<char>, std::string>::unexpected(std::move(message));
				}
			};
			MVT_SLANG_RETURN_ON_FAIL(result);
		}

		std::vector<char> spirv(spirvCode->getBufferSize(), '/0');
		memcpy(spirv.data(), spirvCode->getBufferPointer(), spirvCode->getBufferSize());

		return Expected<std::vector<char>, std::string>::expected(std::move(spirv));
	}

	Expected<std::vector<char>, std::string> SlangCompiler::CompileModule(Slang::ComPtr<slang::IModule> slangModule, const char *moduleName) {
		if (!slangModule) {
			std::string error = std::format("Slang ERR: module '{}' not found.", moduleName);
			std::cerr << error << std::endl;
			return Expected<std::vector<char>, std::string>::unexpected(std::move(error));
		}

		// For the reflection
		// ReflectModule(program->getLayout());

		Slang::ComPtr<slang::IComponentType> linkedProgram; {
			Slang::ComPtr<slang::IBlob> diagnosticsBlob;
			SlangResult result = slangModule->link(
				linkedProgram.writeRef(),
				diagnosticsBlob.writeRef());
			diagnoseIfNeeded(diagnosticsBlob); {
				SlangResult _res = result;
				if (((_res) < 0)) {
					std::string message = "Slang ERR: Failed to link the program.";
					return MVT::Expected<std::vector<char>, std::string>::unexpected(std::move(message));
				}
			};
			// MVT_SLANG_RETURN_ON_FAIL(result);
		}


		Slang::ComPtr<slang::IBlob> spirvCode; {
			Slang::ComPtr<slang::IBlob> diagnosticsBlob;
			SlangResult result = linkedProgram->getTargetCode(
				0, // targetIndex
				spirvCode.writeRef(),
				diagnosticsBlob.writeRef());
			diagnoseIfNeeded(diagnosticsBlob); {
				SlangResult _res = result;
				if (((_res) < 0)) {
					std::string message = "Slang ERR: Failed to fetch the SpirV code.";
					return MVT::Expected<std::vector<char>, std::string>::unexpected(std::move(message));
				}
			};
			MVT_SLANG_RETURN_ON_FAIL(result);
		}

		std::vector<char> spirv(spirvCode->getBufferSize(), '/0');
		memcpy(spirv.data(), spirvCode->getBufferPointer(), spirvCode->getBufferSize());

		return Expected<std::vector<char>, std::string>::expected(std::move(spirv));
	}

	void SlangCompiler::ReflectModule(slang::ProgramLayout *programLayout) {
		if (!programLayout) {
			return;
		}

		return;
	}
} // MVT
