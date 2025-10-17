//
// Created by ianpo on 13/10/2025.
//

#include "MVT/SlangCompiler.hpp"

#include <cassert>
#include <format>
#include <slang/slang-com-ptr.h>
#include <slang/slang-com-helper.h>

#include "MVT/Expected.hpp"


namespace MVT
{
    void SlangCompiler::Initialize()
    {
        s_GlobalSessionResult = slang::createGlobalSession(&s_GlobalSession);
        if (SLANG_FAILED(s_GlobalSessionResult))
        {
            std::cerr << "Slang ERR: Failed to create the global session. (Facility :" << SLANG_GET_RESULT_FACILITY(s_GlobalSessionResult) << " ; Code :" << SLANG_GET_RESULT_CODE(s_GlobalSessionResult) << ")" << std::endl;
        }
    }

    void SlangCompiler::Initialize(const SlangGlobalSessionDesc& desc)
    {
        s_Desc = desc;
        Initialize();
    }

    void SlangCompiler::Shutdown()
    {
        const uint64_t compilerInUse = s_SlangCompilersInUse.load(std::memory_order::acquire);
        if (compilerInUse > 0)
        {
            std::cerr << "Slang ERR: Trying to shutdown but " << compilerInUse << " compilers are still in use." <<
                std::endl;
        }

        slang::shutdown();

        s_Desc = {};
        s_GlobalSession = nullptr;
        s_GlobalSessionResult = 0;
    }

    SlangProfileID SlangCompiler::FindProfile(const char* name)
    {
        assert(s_GlobalSession);
        return s_GlobalSession->findProfile(name);
    }

    SlangCompiler::SlangCompiler()
    {
        assert(s_GlobalSession);

        // Change the profile depending on the target.
        slang::TargetDesc targetDesc{};
        targetDesc.format = SLANG_SPIRV;
        targetDesc.profile = MVT::SlangCompiler::FindProfile("spirv_1_4");

        slang::SessionDesc sessionDesc{};
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
        static const auto c_StrDefaultShaderPath = c_DefaultShaderPath.generic_string();
        const char* searchPaths[] = {c_StrDefaultShaderPath.c_str()};
        sessionDesc.searchPaths = searchPaths;
        sessionDesc.searchPathCount = 1;

        slang::PreprocessorMacroDesc mvtFlag = {"MVT", "1"};
        sessionDesc.preprocessorMacros = &mvtFlag;
        sessionDesc.preprocessorMacroCount = 1;

        m_SessionResult = s_GlobalSession->createSession(m_SessionDescription, m_Session.writeRef());
        if (SLANG_SUCCEEDED(m_SessionResult))
        {
            s_SlangCompilersInUse.fetch_add(1, std::memory_order::release);
        }
    }

    SlangCompiler::~SlangCompiler()
    {
        if (SLANG_SUCCEEDED(m_SessionResult))
        {
            s_SlangCompilersInUse.fetch_sub(1, std::memory_order::release);
        }
    }

    Expected<std::vector<char>, std::string> SlangCompiler::Compile(const char* moduleName, const char* entryPointName)
    {
        assert(SLANG_SUCCEEDED(m_SessionResult));

        Slang::ComPtr<slang::IBlob> diagnostics;
        Slang::ComPtr<slang::IModule> slangModule;
        auto mdl = m_Session->loadModule(moduleName, diagnostics.writeRef());
        slangModule.attach(mdl);

        if (diagnostics)
        {
            std::string error = std::format("Slang ERR: {0}", diagnostics->getBufferPointer());
            std::cerr << error << std::endl;
            return Expected<std::vector<char>, std::string>::unexpected(std::move(error));
        }

        if (!mdl)
        {
            std::string error = std::format("Slang ERR: module '{}' not found.", moduleName);
            std::cerr << error << std::endl;
            return Expected<std::vector<char>, std::string>::unexpected(std::move(error));
        }

        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        const auto epRes = slangModule->findEntryPointByName(entryPointName, entryPoint.writeRef());
        if (SLANG_FAILED(epRes))
        {
            return Expected<std::vector<char>, std::string>::unexpected(
                std::format("Slang ERR: Error getting entry point {} in module {}", entryPointName, moduleName));
        }

        // For the reflection
        // slang::ProgramLayout* layout = program->getLayout();
        std::array<slang::IComponentType*, 2> componentTypes =
        {
            slangModule,
            entryPoint
        };
        Slang::ComPtr<slang::IComponentType> composedProgram;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            SlangResult result = m_Session->createCompositeComponentType(
                componentTypes.data(),
                componentTypes.size(),
                composedProgram.writeRef(),
                diagnosticsBlob.writeRef());
            // diagnoseIfNeeded(diagnosticsBlob);
            {
                SlangResult _res = (result);
                if (SLANG_FAILED(_res))
                {
                    SLANG_HANDLE_RESULT_FAIL(_res);
                    return Expected<std::vector<char>, std::string>::unexpected(
                        "Slang ERR: Failed to create a composite component.");
                }
            };
        }

        Slang::ComPtr<slang::IComponentType> linkedProgram;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            SlangResult result = composedProgram->link(
                linkedProgram.writeRef(),
                diagnosticsBlob.writeRef());
            // diagnoseIfNeeded(diagnosticsBlob);
            {
                SlangResult _res = (result);
                if (SLANG_FAILED(_res))
                {
                    SLANG_HANDLE_RESULT_FAIL(_res);
                    return Expected<std::vector<char>, std::string>::unexpected(
                        "Slang ERR: Failed to link the program.");
                }
            };
        }

        Slang::ComPtr<slang::IBlob> spirvCode;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            SlangResult result = linkedProgram->getEntryPointCode(
                0, // entryPointIndex
                0, // targetIndex
                spirvCode.writeRef(),
                diagnosticsBlob.writeRef());
            // diagnoseIfNeeded(diagnosticsBlob);
            {
                SlangResult _res = (result);
                if (SLANG_FAILED(_res))
                {
                    SLANG_HANDLE_RESULT_FAIL(_res);
                    return Expected<std::vector<char>, std::string>::unexpected(
                        "Slang ERR: Failed to get the spirV Code.");
                }
            };
        }

        std::vector<char> spirv(spirvCode->getBufferSize(), '/0');
        memcpy(spirv.data(), spirvCode->getBufferPointer(), spirvCode->getBufferSize());


        return Expected<std::vector<char>, std::string>::expected(std::move(spirv));
    }

    Expected<std::vector<char>, std::string> SlangCompiler::CompileByPath(const std::filesystem::path& shaderPath, const char* entryPointName)
    {
        assert(SLANG_SUCCEEDED(m_SessionResult));

        const auto pStr = shaderPath.string();
        if (!std::filesystem::exists(shaderPath))
        {
            std::string error = std::format("Slang ERR: The shader '{0}' doesn't exist", pStr);
            std::cerr << error << std::endl;
            return std::move(error);
        }

        const std::uintmax_t file_size = std::filesystem::file_size(shaderPath);
        if (file_size == static_cast<std::uintmax_t>(-1))
        {
            std::string error = std::format("File ERR: The shader '{0}' couldn't be sized.", pStr);
            std::cerr << error << std::endl;
            return std::move(error);
        }

        std::string content(file_size, '\0');
        {
            std::ifstream shaderFile;
            shaderFile.open(shaderPath);

            if (!shaderFile)
            {
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

        if (diagnostics)
        {
            std::string error = std::format("Slang ERR: {0}", diagnostics->getBufferPointer());
            std::cerr << error << std::endl;
            return Expected<std::vector<char>, std::string>::unexpected(std::move(error));
        }

        if (!mdl)
        {
            std::string error = std::format("Slang ERR: module '{}' not found.", moduleName);
            std::cerr << error << std::endl;
            return Expected<std::vector<char>, std::string>::unexpected(std::move(error));
        }

        Slang::ComPtr<slang::IEntryPoint> entryPoint;
        const auto epRes = slangModule->findEntryPointByName(entryPointName, entryPoint.writeRef());
        if (SLANG_FAILED(epRes))
        {
            return Expected<std::vector<char>, std::string>::unexpected(
                std::format("Slang ERR: Error getting entry point {} in module {}", entryPointName, moduleName));
        }

        // For the reflection
        // slang::ProgramLayout* layout = program->getLayout();
        std::array<slang::IComponentType*, 2> componentTypes =
        {
            slangModule,
            entryPoint
        };
        Slang::ComPtr<slang::IComponentType> composedProgram;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            SlangResult result = m_Session->createCompositeComponentType(
                componentTypes.data(),
                componentTypes.size(),
                composedProgram.writeRef(),
                diagnosticsBlob.writeRef());
            // diagnoseIfNeeded(diagnosticsBlob);
            {
                SlangResult _res = (result);
                if (SLANG_FAILED(_res))
                {
                    SLANG_HANDLE_RESULT_FAIL(_res);
                    return Expected<std::vector<char>, std::string>::unexpected(
                        "Slang ERR: Failed to create a composite component.");
                }
            };
        }

        Slang::ComPtr<slang::IComponentType> linkedProgram;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            SlangResult result = composedProgram->link(
                linkedProgram.writeRef(),
                diagnosticsBlob.writeRef());
            // diagnoseIfNeeded(diagnosticsBlob);
            {
                SlangResult _res = (result);
                if (SLANG_FAILED(_res))
                {
                    SLANG_HANDLE_RESULT_FAIL(_res);
                    return Expected<std::vector<char>, std::string>::unexpected(
                        "Slang ERR: Failed to link the program.");
                }
            };
        }

        Slang::ComPtr<slang::IBlob> spirvCode;
        {
            Slang::ComPtr<slang::IBlob> diagnosticsBlob;
            SlangResult result = linkedProgram->getTargetCode(
                0, // targetIndex
                spirvCode.writeRef(),
                diagnosticsBlob.writeRef());
            linkedProgram->getSp
            // diagnoseIfNeeded(diagnosticsBlob);
            {
                SlangResult _res = (result);
                if (SLANG_FAILED(_res))
                {
                    SLANG_HANDLE_RESULT_FAIL(_res);
                    return Expected<std::vector<char>, std::string>::unexpected(
                        "Slang ERR: Failed to get the spirV Code.");
                }
            };
        }

        std::vector<char> spirv(spirvCode->getBufferSize(), '/0');
        memcpy(spirv.data(), spirvCode->getBufferPointer(), spirvCode->getBufferSize());


        return Expected<std::vector<char>, std::string>::expected(std::move(spirv));
    }
} // MVT
