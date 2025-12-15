//
// Created by ianpo on 13/10/2025.
//

#pragma once

#include <slang/slang-com-ptr.h>
#include <slang/slang.h>

#include "MVT/Expected.hpp"

namespace MVT {
	class SlangCompiler {
	public:
		static void Initialize();

		static void Initialize(const SlangGlobalSessionDesc &desc);

		static void Shutdown();

		static SlangProfileID FindProfile(const char *name);

	public:
		static Expected<std::vector<char>, std::string> s_Compile(const char *shaderName, const char *entryPoint);

		static Expected<std::vector<char>, std::string> s_CompileByPath(const std::filesystem::path &shaderPath, const char *entryPoint);

		static Expected<std::vector<char>, std::string> s_Compile(const char *shaderName);

		static Expected<std::vector<char>, std::string> s_CompileByPath(const std::filesystem::path &shaderPath);

		static Expected<std::vector<char>, std::string> s_OneShotCompile(const char *shaderName, const char *entryPoint);

		static Expected<std::vector<char>, std::string> s_OneShotCompileByPath(const std::filesystem::path &shaderPath, const char *entryPoint);

		static Expected<std::vector<char>, std::string> s_OneShotCompile(const char *shaderName);

		static Expected<std::vector<char>, std::string> s_OneShotCompileByPath(const std::filesystem::path &shaderPath);

		static void ResetCompiler();

	private:
		static inline SlangGlobalSessionDesc s_Desc{};
		static inline slang::IGlobalSession *s_GlobalSession = nullptr;
		static inline SlangResult s_GlobalSessionResult = 0;
		static inline std::atomic<uint64_t> s_SlangCompilersInUse{0};

	public:
		SlangCompiler(bool columnMajor = true);

		~SlangCompiler();

	public:
		Expected<std::vector<char>, std::string> Compile(const char *shaderName, const char *entryPoint);

		Expected<std::vector<char>, std::string> CompileByPath(const std::filesystem::path &shaderPath, const char *entryPoint);

		Expected<std::vector<char>, std::string> Compile(const char *shaderName);

		Expected<std::vector<char>, std::string> CompileByPath(const std::filesystem::path &shaderPath);

		static void AddPath(const std::filesystem::path &path);

	private:
		Expected<std::vector<char>, std::string> CompileModule(Slang::ComPtr<slang::IModule> slangModule, const char *moduleName, const char *entryPointName);

		Expected<std::vector<char>, std::string> CompileModule(Slang::ComPtr<slang::IModule> slangModule, const char *moduleName);

		void ReflectModule(slang::ProgramLayout *programLayout);

	private:
		Slang::ComPtr<slang::ISession> m_Session = nullptr;
		slang::SessionDesc m_SessionDescription{};
		SlangResult m_SessionResult = 0;

	private:
		static inline std::unique_ptr<SlangCompiler> s_MainCompiler{nullptr};
		static inline std::vector<std::string> s_SearchPaths{};
	};


	class SlangLifetime {
	public:
		SlangLifetime() {
			SlangCompiler::Initialize();
		}

		SlangLifetime(const SlangGlobalSessionDesc &desc) {
			SlangCompiler::Initialize(desc);
		}

		~SlangLifetime() {
			SlangCompiler::Shutdown();
		}

		SlangLifetime(const SlangLifetime &) = delete;

		SlangLifetime &operator=(const SlangLifetime &) = delete;
	};
} // MVT
