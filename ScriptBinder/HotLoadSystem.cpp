#ifndef DYNAMICCPP_EXPORTS
#include "HotLoadSystem.h"
#include "LogSystem.h"
#include "GameObject.h"
#include "SceneManager.h"
#include "ModuleBehavior.h"
#include "pugixml.hpp"
#include "ReflectionYml.h"
#include "ProgressWindow.h"

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <winternl.h>                   //PROCESS_BASIC_INFORMATION


// warning C4996: 'GetVersionExW': was declared deprecated
#pragma warning (disable : 4996)
bool IsWindows8OrGreater()
{
	OSVERSIONINFO ovi = { 0 };
	ovi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	GetVersionEx(&ovi);
	if ((ovi.dwMajorVersion == 6 && ovi.dwMinorVersion >= 2) || ovi.dwMajorVersion > 6)
		return true;

	return false;
} //IsWindows8OrGreater
#pragma warning (default : 4996)



bool ReadMem(void* addr, void* buf, int size)
{
	BOOL b = ReadProcessMemory(GetCurrentProcess(), addr, buf, size, nullptr);
	return b != FALSE;
}

#ifdef _WIN64
#define BITNESS 1
#else
#define BITNESS 0
#endif

typedef NTSTATUS(NTAPI* pfuncNtQueryInformationProcess)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);


int GetModuleLoadCount(HMODULE hDll)
{
	// Not supported by earlier versions of windows.
	if (!IsWindows8OrGreater())
		return 0;

	PROCESS_BASIC_INFORMATION pbi = { 0 };

	HMODULE hNtDll = LoadLibraryA("ntdll.dll");
	if (!hNtDll)
		return 0;

	pfuncNtQueryInformationProcess pNtQueryInformationProcess = (pfuncNtQueryInformationProcess)GetProcAddress(hNtDll, "NtQueryInformationProcess");
	bool b = pNtQueryInformationProcess != nullptr;
	if (b) b = NT_SUCCESS(pNtQueryInformationProcess(GetCurrentProcess(), ProcessBasicInformation, &pbi, sizeof(pbi), nullptr));
	FreeLibrary(hNtDll);

	if (!b)
		return 0;

	char* LdrDataOffset = (char*)(pbi.PebBaseAddress) + offsetof(PEB, Ldr);
	char* addr;
	PEB_LDR_DATA LdrData;

	if (!ReadMem(LdrDataOffset, &addr, sizeof(void*)) || !ReadMem(addr, &LdrData, sizeof(LdrData)))
		return 0;

	LIST_ENTRY* head = LdrData.InMemoryOrderModuleList.Flink;
	LIST_ENTRY* next = head;

	do {
		LDR_DATA_TABLE_ENTRY LdrEntry;
		LDR_DATA_TABLE_ENTRY* pLdrEntry = CONTAINING_RECORD(head, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

		if (!ReadMem(pLdrEntry, &LdrEntry, sizeof(LdrEntry)))
			return 0;

		if (LdrEntry.DllBase == (void*)hDll)
		{
			//  
			//  http://www.geoffchappell.com/studies/windows/win32/ntdll/structs/ldr_data_table_entry.htm
			//
			int offDdagNode = (0x14 - BITNESS) * sizeof(void*);   // See offset on LDR_DDAG_NODE *DdagNode;

			ULONG count = 0;
			char* addrDdagNode = ((char*)pLdrEntry) + offDdagNode;

			//
			//  http://www.geoffchappell.com/studies/windows/win32/ntdll/structs/ldr_ddag_node.htm
			//  See offset on ULONG LoadCount;
			//
			if (!ReadMem(addrDdagNode, &addr, sizeof(void*)) || !ReadMem(addr + 3 * sizeof(void*), &count, sizeof(count)))
				return 0;

			return (int)count;
		} //if

		head = LdrEntry.InMemoryOrderLinks.Flink;
	} while (head != next);

	return 0;
} //GetModuleLoadCount

std::string AnsiToUtf8(const std::string& ansiStr)
{
    // ANSI → Wide
    int wideLen = MultiByteToWideChar(CP_ACP, 0, ansiStr.c_str(), -1, nullptr, 0);
    std::wstring wide(wideLen, 0);
    MultiByteToWideChar(CP_ACP, 0, ansiStr.c_str(), -1, &wide[0], wideLen);

    // Wide → UTF-8
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string utf8(utf8Len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &utf8[0], utf8Len, nullptr, nullptr);

    return utf8;
}

void RunMsbuildWithLiveLog(const std::wstring& commandLine)
{
    HANDLE hRead, hWrite;
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
    {
        __debugbreak();
    }

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = NULL;

    PROCESS_INFORMATION pi;

    std::wstring fullCommand = commandLine;

    if (!CreateProcessW(NULL, &fullCommand[0], NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        throw std::runtime_error("Build failed");
    }

    CloseHandle(hWrite);

    char buffer[4096]{};
    DWORD bytesRead;
    std::string leftover;

    while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr))
    {
        if (bytesRead == 0) break;
        buffer[bytesRead] = '\0';

        leftover += buffer;

        size_t pos;
        while ((pos = leftover.find('\n')) != std::string::npos)
        {
            std::string line = leftover.substr(0, pos);
            leftover.erase(0, pos + 1);

            if (line.empty())
            {
                continue;
            }

			if (line.find("error") != std::string::npos || line.find("error C") != std::string::npos)
			{
				Debug->LogError(line);
			}
			else if (line.find("오류") != std::string::npos || line.find("경고") != std::string::npos)
			{
				Debug->LogDebug(line);
			}
			//else
			//{
			//	std::string strLine = AnsiToUtf8(line);
			//	Debug->LogDebug(strLine);
			//}
		}
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hRead);
}

void HotLoadSystem::Initialize()
{
    std::wstring slnPath = PathFinder::DynamicSolutionPath("Dynamic_CPP.sln").wstring();
	msbuildPath = EngineSettingInstance->GetMsbuildPath();
	if (msbuildPath.empty())
	{
		Debug->LogError("MSBuild path is not set. Please check your Visual Studio installation.");
		return;
	}

#if defined(_DEBUG)
	command = std::wstring(L"cmd /c \"")
        + L"\"" + msbuildPath + L"\" "
        + L"\"" + slnPath + L"\" "
        + L"/m /t:Build /p:Configuration=Debug /p:Platform=x64 /nologo"
        + L"\"";
#else
	command = std::wstring(L"cmd /c \"")
		+ L"\"" + msbuildPath + L"\" "
		+ L"\"" + slnPath + L"\" "
		+ L"/m /t:Build /p:Configuration=Release /p:Platform=x64 /nologo"
		+ L"\"";
#endif

	{
		try
		{
			Compile();
		}
		catch (const std::exception& e)
		{
			Debug->LogError("Failed to compile script: " + std::string(e.what()));
			return;
		}
	}

	m_initModuleFunc();
	const char** scriptNames = nullptr;
	int scriptCount = 0;
	scriptNames = m_scriptNamesFunc(&scriptCount);

	for (int i = 0; i < scriptCount; ++i)
	{
		std::string scriptName = scriptNames[i];
		std::string scriptFileName = std::string(scriptName);
		m_scriptNames.push_back(scriptName);
	}

}

void HotLoadSystem::Shutdown()
{
	if (hDll)
	{
		FreeLibrary(hDll);
		hDll = nullptr;
	}
}

bool HotLoadSystem::IsScriptUpToDate()
{
	if (!m_isStartUp)
	{
		m_isStartUp = true;
	}

	file::path dllPath = PathFinder::RelativeToExecutable("Dynamic_CPP.dll");
	file::path funcMainPath = PathFinder::DynamicSolutionPath("funcMain.h");
	file::path slnPath = PathFinder::DynamicSolutionPath("Dynamic_CPP.sln");

	if (!file::exists(dllPath))
		return false; // DLL 파일이 존재하지 않으면 빌드 필요

	auto dllTimeStamp = file::last_write_time(dllPath);
	auto funcMainTimeStamp = file::last_write_time(funcMainPath);

	if (funcMainTimeStamp > dllTimeStamp) return false; // 빌드 필요함

	for (const auto& scriptName : m_scriptNames)
	{
		file::path scriptPath = PathFinder::Relative("Script\\" + scriptName + ".cpp");

		if (file::exists(scriptPath))
		{
			auto scriptTimeStamp = file::last_write_time(scriptPath);
			if (scriptTimeStamp > dllTimeStamp) return false; // 빌드 필요함 (스크립트가 더 최신임)
		}
	}

	return true; // 모든 스크립트가 DLL보다 오래됨 (업투데이트됨)
}

void HotLoadSystem::ReloadDynamicLibrary()
{
	if(true == m_isCompileEventInvoked)
	{
		g_progressWindow->Launch();
		g_progressWindow->SetStatusText(L"Compile Script Library...");
		g_progressWindow->SetProgress(100);
		try
		{
			Compile();
		}
		catch (const std::exception& e)
		{
			Debug->LogError("Failed to compile script: " + std::string(e.what()));
			m_isCompileEventInvoked = false;
			g_progressWindow->Close();
			return;
		}

		m_initModuleFunc();
		m_scriptNames.clear();
		const char** scriptNames = nullptr;
		int scriptCount = 0;
		scriptNames = m_scriptNamesFunc(&scriptCount);

		for (int i = 0; i < scriptCount; ++i)
		{
			std::string scriptName = scriptNames[i];
			std::string scriptFileName = std::string(scriptName);
			m_scriptNames.push_back(scriptName);
		}

		ReplaceScriptComponent();
		g_progressWindow->Close();
	}
}

void HotLoadSystem::ReplaceScriptComponent()
{
	if (true == m_isReloading && false == m_isCompileEventInvoked)
	{
		auto activeScene = SceneManagers->GetActiveScene();
		activeScene->m_selectedSceneObject = nullptr;

		auto& gameObjects = activeScene->m_SceneObjects;
		std::unordered_set<GameObject*> gameObjectSet;

		for(auto& gameObject : gameObjects)
			gameObjectSet.insert(gameObject.get());

		for (auto& [gameObject, index, name] : m_scriptComponentIndexs)
		{
			if (!gameObjectSet.contains(gameObject)) continue; // 게임 오브젝트가 씬에 존재하지 않으면 스킵

			auto* newScript = CreateMonoBehavior(name.c_str());
			if (nullptr == newScript)
			{
				Debug->LogError("Failed to create script: " + std::string(name));
				continue;
			}

			if(SceneManagers->m_isGameStart)
			{
				ScriptManager->BindScriptEvents(newScript, name);
			}
			newScript->SetOwner(gameObject);
			auto sharedScript = std::shared_ptr<Component>(newScript);
			gameObject->m_components[index].swap(sharedScript);
			newScript->m_scriptGuid = DataSystems->GetFilenameToGuid(name + ".cpp");
		}
		m_isReloading = false;
	}
}

void HotLoadSystem::CompileEvent()
{
	m_isCompileEventInvoked = true;
}

void HotLoadSystem::BindScriptEvents(ModuleBehavior* script, const std::string_view& name)
{
	auto activeScene = SceneManagers->GetActiveScene();
	//결국 이렇게되면 .meta파일을 클라이언트가 가지고 있어야 됨...
	std::string scriptMetaFile = "Assets\\Script\\" + std::string(name) + ".cpp" + ".meta";
	file::path scriptMetaFileName = PathFinder::DynamicSolutionPath(scriptMetaFile);

	if (file::exists(scriptMetaFileName))
	{
		MetaYml::Node scriptNode = MetaYml::LoadFile(scriptMetaFileName.string());
		std::vector<std::string> events;
		if (scriptNode["eventRegisterSetting"])
		{
			for (const auto& node : scriptNode["eventRegisterSetting"])
			{
				events.push_back(node.as<std::string>());
			}

			for (const auto& event : events)
			{
				if (event == "Awake")
				{
					if (script->m_awakeEventHandle.IsValid()) continue;

					script->m_awakeEventHandle = activeScene->AwakeEvent.AddRaw(script, &ModuleBehavior::AwakeInvoke);
				}
				else if (event == "OnEnable")
				{
					if (script->m_onEnableEventHandle.IsValid()) continue;

					script->m_onEnableEventHandle = activeScene->OnEnableEvent.AddRaw(script, &ModuleBehavior::OnEnableInvoke);
				}
				else if (event == "Start")
				{
					if (script->m_startEventHandle.IsValid()) continue;

					script->m_startEventHandle = activeScene->StartEvent.AddRaw(script, &ModuleBehavior::StartInvoke);
				}
				else if (event == "FixedUpdate")
				{
					if (script->m_fixedUpdateEventHandle.IsValid()) continue;

					script->m_fixedUpdateEventHandle = activeScene->FixedUpdateEvent.AddRaw(script, &ModuleBehavior::FixedUpdateInvoke);
				}
				else if (event == "OnTriggerEnter")
				{
					if (script->m_onTriggerEnterEventHandle.IsValid()) continue;

					script->m_onTriggerEnterEventHandle = activeScene->OnTriggerEnterEvent.AddRaw(script, &ModuleBehavior::OnTriggerEnterInvoke);
				}
				else if (event == "OnTriggerStay")
				{
					if (script->m_onTriggerStayEventHandle.IsValid()) continue;

					script->m_onTriggerStayEventHandle = activeScene->OnTriggerStayEvent.AddRaw(script, &ModuleBehavior::OnTriggerStayInvoke);
				}
				else if (event == "OnTriggerExit")
				{
					if (script->m_onTriggerExitEventHandle.IsValid()) continue;

					script->m_onTriggerExitEventHandle = activeScene->OnTriggerExitEvent.AddRaw(script, &ModuleBehavior::OnTriggerExitInvoke);
				}
				else if (event == "OnCollisionEnter")
				{
					if (script->m_onCollisionEnterEventHandle.IsValid()) continue;

					script->m_onCollisionEnterEventHandle = activeScene->OnCollisionEnterEvent.AddRaw(script, &ModuleBehavior::OnCollisionEnterInvoke);
				}
				else if (event == "OnCollisionStay")
				{
					if (script->m_onCollisionStayEventHandle.IsValid()) continue;

					script->m_onCollisionStayEventHandle = activeScene->OnCollisionStayEvent.AddRaw(script, &ModuleBehavior::OnCollisionStayInvoke);
				}
				else if (event == "OnCollisionExit")
				{
					if (script->m_onCollisionExitEventHandle.IsValid()) continue;

					script->m_onCollisionExitEventHandle = activeScene->OnCollisionExitEvent.AddRaw(script, &ModuleBehavior::OnCollisionExitInvoke);
				}
				else if (event == "Update")
				{
					if (script->m_updateEventHandle.IsValid()) continue;

					script->m_updateEventHandle = activeScene->UpdateEvent.AddRaw(script, &ModuleBehavior::UpdateInvoke);
				}
				else if (event == "LateUpdate")
				{
					if (script->m_lateUpdateEventHandle.IsValid()) continue;

					script->m_lateUpdateEventHandle = activeScene->LateUpdateEvent.AddRaw(script, &ModuleBehavior::LateUpdateInvoke);
				}
				else if (event == "OnDisable")
				{
					if (script->m_onDisableEventHandle.IsValid()) continue;

					script->m_onDisableEventHandle = activeScene->OnDisableEvent.AddRaw(script, &ModuleBehavior::OnDisableInvoke);
				}
				else if (event == "OnDestroy")
				{
					if (script->m_onDestroyEventHandle.IsValid()) continue;

					script->m_onDestroyEventHandle = activeScene->OnDestroyEvent.AddRaw(script, &ModuleBehavior::OnDestroyInvoke);
				}
			}
			
		}
	}
}

void HotLoadSystem::UnbindScriptEvents(ModuleBehavior* script, const std::string_view& name)
{
	auto activeScene = SceneManagers->GetActiveScene();

	if (script->m_awakeEventHandle.IsValid())
	{
		activeScene->AwakeEvent -= script->m_awakeEventHandle;
	}

	if (script->m_onEnableEventHandle.IsValid())
	{
		activeScene->OnEnableEvent -= script->m_onEnableEventHandle;
	}

	if (script->m_startEventHandle.IsValid())
	{
		activeScene->StartEvent -= script->m_startEventHandle;
	}

	if (script->m_fixedUpdateEventHandle.IsValid())
	{
		activeScene->FixedUpdateEvent -= script->m_fixedUpdateEventHandle;
	}

	if (script->m_onTriggerEnterEventHandle.IsValid())
	{
		activeScene->OnTriggerEnterEvent -= script->m_onTriggerEnterEventHandle;
	}

	if (script->m_onTriggerStayEventHandle.IsValid())
	{
		activeScene->OnTriggerStayEvent -= script->m_onTriggerStayEventHandle;
	}

	if (script->m_onTriggerExitEventHandle.IsValid())
	{
		activeScene->OnTriggerExitEvent -= script->m_onTriggerExitEventHandle;
	}

	if (script->m_onCollisionEnterEventHandle.IsValid())
	{
		activeScene->OnCollisionEnterEvent -= script->m_onCollisionEnterEventHandle;
	}

	if (script->m_onCollisionStayEventHandle.IsValid())
	{
		activeScene->OnCollisionStayEvent -= script->m_onCollisionStayEventHandle;
	}

	if (script->m_onCollisionExitEventHandle.IsValid())
	{
		activeScene->OnCollisionExitEvent -= script->m_onCollisionExitEventHandle;
	}

	if (script->m_updateEventHandle.IsValid())
	{
		activeScene->UpdateEvent -= script->m_updateEventHandle;
	}

	if (script->m_lateUpdateEventHandle.IsValid())
	{
		activeScene->LateUpdateEvent -= script->m_lateUpdateEventHandle;
	}

	if (script->m_onDisableEventHandle.IsValid())
	{
		activeScene->OnDisableEvent -= script->m_onDisableEventHandle;
	}

	if (script->m_onDestroyEventHandle.IsValid())
	{
		activeScene->OnDestroyEvent -= script->m_onDestroyEventHandle;
	}

}

void HotLoadSystem::CreateScriptFile(const std::string_view& name)
{
	std::string scriptHeaderFileName = std::string(name) + ".h";
	std::string scriptBodyFileName = std::string(name) + ".cpp";
	std::string scriptHeaderFilePath = PathFinder::Relative("Script\\" + scriptHeaderFileName).string();
	std::string scriptBodyFilePath = PathFinder::Relative("Script\\" + scriptBodyFileName).string();
	std::string scriptFactoryPath = PathFinder::DynamicSolutionPath("CreateFactory.h").string();
	std::string scriptFactoryFuncPath = PathFinder::DynamicSolutionPath("funcMain.h").string();
	std::string scriptProjPath = PathFinder::DynamicSolutionPath("Dynamic_CPP.vcxproj").string();
	std::string scriptFilterPath = PathFinder::DynamicSolutionPath("Dynamic_CPP.vcxproj.filters").string();
	//Create Script File
	std::ofstream scriptFile(scriptHeaderFilePath);
	if (scriptFile.is_open())
	{
		scriptFile 
			<< scriptIncludeString 
			<< name 
			<< scriptInheritString
			<< name
			<< scriptBodyString
			<< scriptEndString;

		scriptFile.close();
	}
	else
	{
		throw std::runtime_error("Failed to create script file");
	}

	std::ofstream scriptBodyFile(scriptBodyFilePath);
	if (scriptBodyFile.is_open())
	{
		scriptBodyFile
			<< scriptCppString
			<< name
			<< scriptCppEndString
			<< name
			<< scriptCppEndBodyString
			<< name
			<< scriptCppEndUpdateString;
		scriptBodyFile.close();
	}
	else
	{
		throw std::runtime_error("Failed to create script file");
	}
	//Factory Add
	std::ifstream scriptFactoryFile(scriptFactoryPath);
	if (scriptFactoryFile.is_open())
	{
		std::stringstream buffer;
		buffer << scriptFactoryFile.rdbuf();
		std::string content = buffer.str();
		scriptFactoryFile.close();

		size_t posHeader = content.find(markerFactoryHeaderString);
		if (posHeader != std::string::npos)
		{
			size_t endLine = content.find('\n', posHeader);
			if (endLine != std::string::npos)
			{
				content.insert(endLine + 1, scriptFactoryIncludeString + scriptHeaderFileName + "\"\n");
			}
		}
		else
		{
			throw std::runtime_error("Failed to find marker in script factory file");
		}

		std::ofstream scriptFactoryFileOut(scriptFactoryPath);
		if (scriptFactoryFileOut.is_open())
		{
			scriptFactoryFileOut << content;
			scriptFactoryFileOut.close();
		}
		else
		{
			throw std::runtime_error("Failed to create script factory file");
		}
	}
	else
	{
		throw std::runtime_error("Failed to create script factory file");
	}

	//Factory Func Add
	std::ifstream scriptFactoryFuncFile(scriptFactoryFuncPath);
	if (scriptFactoryFuncFile.is_open())
	{
		std::stringstream buffer;
		buffer << scriptFactoryFuncFile.rdbuf();
		std::string content = buffer.str();
		scriptFactoryFuncFile.close();

		size_t posFunc = content.find(markerFactoryFuncString);
		if (posFunc != std::string::npos)
		{
			size_t endLine = content.find('\n', posFunc);
			if (endLine != std::string::npos)
			{
				content.insert(endLine + 1, scriptFactoryFunctionString + name.data() + scriptFactoryFunctionLambdaString + name.data() + scriptFactoryFunctionEndString);
			}
		}
		else
		{
			throw std::runtime_error("Failed to find marker in script factory file");
		}

		std::ofstream scriptFactoryFuncFileOut(scriptFactoryFuncPath);
		if (scriptFactoryFuncFileOut.is_open())
		{
			scriptFactoryFuncFileOut << content;
			scriptFactoryFuncFileOut.close();
		}
		else
		{
			throw std::runtime_error("Failed to create script factory file");
		}
	}
	else
	{
		throw std::runtime_error("Failed to create script factory file");
	}

	{
		//Filter Add
		pugi::xml_document doc;
		if (!doc.load_file(scriptFilterPath.c_str(), pugi::parse_full, pugi::encoding_auto))
		{
			throw std::runtime_error("Failed to load XML file");
		}

		std::vector<pugi::xml_node> itemGroups;
		for (pugi::xml_node itemGroup = doc.child("Project").child("ItemGroup"); itemGroup; itemGroup = itemGroup.next_sibling("ItemGroup"))
		{
			itemGroups.push_back(itemGroup);
		}

		// 두 번째 ItemGroup (인덱스 1)에 헤더 파일 추가 (ClInclude)
		pugi::xml_node headerGroup = itemGroups[1];
		pugi::xml_node newHeader = headerGroup.append_child("ClInclude");
		newHeader.append_attribute("Include") = "Assets\\Script\\" + scriptHeaderFileName;
		pugi::xml_node filterNodeHeader = newHeader.append_child("Filter");
		filterNodeHeader.text().set("Script\\ScriptClass");

		// 세 번째 ItemGroup (인덱스 2)에 소스 파일 추가 (ClCompile)
		pugi::xml_node cppGroup = itemGroups[2];
		pugi::xml_node newSource = cppGroup.append_child("ClCompile");
		newSource.append_attribute("Include") = "Assets\\Script\\" + scriptBodyFileName;
		pugi::xml_node filterNodeSource = newSource.append_child("Filter");
		filterNodeSource.text().set("Script\\ScriptClass");

		if (!doc.save_file(scriptFilterPath.c_str(), PUGIXML_TEXT("\t"), 1U, pugi::encoding_auto))
		{
			throw std::runtime_error("Failed to save XML file");
		}
	}

	{
		//Proj Add
		pugi::xml_document doc;
		if (!doc.load_file(scriptProjPath.c_str(), pugi::parse_full, pugi::encoding_auto))
		{
			throw std::runtime_error("Failed to load XML file");
		}

		std::vector<pugi::xml_node> itemGroups;
		for (pugi::xml_node itemGroup = doc.child("Project").child("ItemGroup"); itemGroup; itemGroup = itemGroup.next_sibling("ItemGroup"))
		{
			itemGroups.push_back(itemGroup);
		}

		// 두 번째 ItemGroup (인덱스 1)에 헤더 파일 추가 (ClInclude)
		pugi::xml_node headerGroup = itemGroups[1];
		pugi::xml_node newHeader = headerGroup.append_child("ClInclude");
		newHeader.append_attribute("Include") = "Assets\\Script\\" + scriptHeaderFileName;

		// 세 번째 ItemGroup (인덱스 2)에 소스 파일 추가 (ClCompile)
		pugi::xml_node cppGroup = itemGroups[2];
		pugi::xml_node newSource = cppGroup.append_child("ClCompile");
		newSource.append_attribute("Include") = "Assets\\Script\\" + scriptBodyFileName;

		pugi::xml_node additionalOptionsDebug = newSource.append_child("AdditionalOptions");
		additionalOptionsDebug.append_attribute("Condition") = "'$(Configuration)|$(Platform)'=='Debug|x64'";
		additionalOptionsDebug.append_child(pugi::node_pcdata).set_value("/utf-8 %(AdditionalOptions)");

		pugi::xml_node additionalOptionsRelease = newSource.append_child("AdditionalOptions");
		additionalOptionsRelease.append_attribute("Condition") = "'$(Configuration)|$(Platform)'=='Release|x64'";
		additionalOptionsRelease.append_child(pugi::node_pcdata).set_value("/utf-8 %(AdditionalOptions)");

		if (!doc.save_file(scriptProjPath.c_str(), PUGIXML_TEXT("\t"), 1U, pugi::encoding_auto))
		{
			throw std::runtime_error("Failed to save XML file");
		}
	}
}

void HotLoadSystem::Compile()
{
	if (hDll)
	{
		for (auto& [gameObject, index, name] : m_scriptComponentIndexs)
		{
			auto* script = dynamic_cast<ModuleBehavior*>(gameObject->m_components[index].get());
			if (script)
			{
				UnbindScriptEvents(script, name);
				gameObject->m_components[index].reset();
			}
		}

		while(GetModuleLoadCount(hDll) > 0)
		{
			FreeLibrary(hDll);
		}
		m_scriptFactoryFunc		= nullptr;
		m_initModuleFunc		= nullptr;
		m_scriptNamesFunc		= nullptr;
		m_setSceneManagerFunc	= nullptr;
		hDll					= nullptr;
	}

	if (EngineSettingInstance->GetMSVCVersion() != MSVCVersion::None)
	{
		try
		{
			RunMsbuildWithLiveLog(command);
		}
		catch (const std::exception& e)
		{
			m_isReloading = false;
			g_progressWindow->SetStatusText(L"Build failed...");
			throw std::runtime_error("Build failed");
		}
	}
	else
	{
		Debug->LogError("MSBuild path is not set. Please check your Visual Studio installation.");
	}

	file::path dllPath = PathFinder::RelativeToExecutable("Dynamic_CPP.dll");
	hDll = LoadLibraryA(dllPath.string().c_str());
	if (!hDll)
	{
		m_isReloading = false;
		g_progressWindow->SetStatusText(L"Failed to load library...");
		throw std::runtime_error("Failed to load library");
	}

	m_scriptFactoryFunc = reinterpret_cast<ModuleBehaviorFunc>(GetProcAddress(hDll, "CreateModuleBehavior"));
	if (!m_scriptFactoryFunc)

	{
		m_isReloading = false;
		g_progressWindow->SetStatusText(L"Failed to get function address...");
		throw std::runtime_error("Failed to get function address");
	}

	m_initModuleFunc = reinterpret_cast<InitModuleFunc>(GetProcAddress(hDll, "InitModuleFactory"));
	if (!m_initModuleFunc)
	{
		m_isReloading = false;
		g_progressWindow->SetStatusText(L"Failed to get function address...");
		throw std::runtime_error("Failed to get function address");
	}

	m_scriptNamesFunc = reinterpret_cast<GetScriptNamesFunc>(GetProcAddress(hDll, "ListModuleBehavior"));
	if (!m_scriptNamesFunc)
	{
		m_isReloading = false;
		g_progressWindow->SetStatusText(L"Failed to get function address...");
		throw std::runtime_error("Failed to get function address");
	}

	m_setSceneManagerFunc = reinterpret_cast<SetSceneManagerFunc>(GetProcAddress(hDll, "SetSceneManager"));
	if (!m_setSceneManagerFunc)
	{
		m_isReloading = false;
		g_progressWindow->SetStatusText(L"Failed to get function address...");
		throw std::runtime_error("Failed to get function address");
	}

	m_isCompileEventInvoked = false;
	m_isReloading = true;
}
#endif // !DYNAMICCPP_EXPORTS