#ifndef DYNAMICCPP_EXPORTS
#include "App.h"
#include "Camera.h"
#include "InputManager.h"
#include "PathFinder.h"
#include "DumpHandler.h"
#include "CoreWindow.h"
#include "DataSystem.h"
#include "DebugStreamBuf.h"
#include "EngineSetting.h"
#include "EffectProxyController.h"
#include "ResourceAllocator.h"
#include <imgui_impl_win32.h>
#include <ppltasks.h>
#include <ppl.h>

#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

MAIN_ENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
	PathFinder::Initialize();
	Log::Initialize();

	EngineSettingInstance->Initialize();

	//시작
	CoreWindow::RegisterCreateEventHandler([](HWND hWnd, WPARAM wParam, LPARAM lParam) -> LRESULT
	{
		return 0;
	});

    static DebugStreamBuf debugBuf(std::cout.rdbuf());
    std::cout.rdbuf(&debugBuf);

	{
		Core::App app;
		app.Initialize(hInstance, L"Creator Editor", 1920, 1080);
		app.Finalize();
	}

	EffectManager::Destroy();
	EffectProxyController::Destroy();
	InputManager::Destroy();
	DataSystem::Destroy();
	ResourceAllocator::Destroy();

	Log::Finalize();

	return 0;
}

void Core::App::Initialize(HINSTANCE hInstance, const wchar_t* title, int width, int height)
{
	EffectManager::GetInstance();
	EffectProxyController::GetInstance();
	DataSystem::GetInstance();
	ResourceAllocator::GetInstance();

    std::wstring loadingImgPath = PathFinder::IconPath() / L"Loading.bmp";
    g_progressWindow->Launch(ProgressWindowStyle::InitStyle, loadingImgPath);
    g_progressWindow->SetStatusText(L"Initializing Core...");

	CoreWindow coreWindow(hInstance, title, width, height);
	CoreWindow::SetDumpType(DUMP_TYPE::DUMP_TYPE_FULL);
    m_hWnd = coreWindow.GetHandle();

    g_progressWindow->SetProgress(10);

    g_progressWindow->SetStatusText(L"Initializing Dx11 Device...");
	m_deviceResources = std::make_shared<DirectX11::DeviceResources>();
    g_progressWindow->SetProgress(20);

    g_progressWindow->SetStatusText(L"Initializing Windows API...");
	SetWindow(coreWindow);
    g_progressWindow->SetProgress(30);
    RegisterHandler(coreWindow);
    g_progressWindow->SetProgress(40);
	Load();
	Run();
}

void Core::App::Finalize()
{
	m_main->Finalize();
	m_deviceResources->ReportLiveDeviceObjects();
}

void Core::App::SetWindow(CoreWindow& coreWindow)
{
	m_deviceResources->SetWindow(coreWindow);
}

void Core::App::RegisterHandler(CoreWindow& coreWindow)
{
    coreWindow.RegisterHandler(WM_INPUT,		this, &App::ProcessRawInput);
	coreWindow.RegisterHandler(WM_SIZE,			this, &App::HandleResizeEvent);
    coreWindow.RegisterHandler(WM_KEYDOWN,		this, &App::HandleCharEvent);
    coreWindow.RegisterHandler(WM_CLOSE,		this, &App::Shutdown);
    coreWindow.RegisterHandler(WM_DROPFILES,	this, &App::HandleDropFileEvent);
}

void Core::App::Load()
{
	if (nullptr == m_main)
	{
		m_main = std::make_unique<DirectX11::Dx11Main>(m_deviceResources);
	}
}

void Core::App::Run()
{
	CoreWindow::GetForCurrentInstance()->InitializeTask([&]
	{
		m_main->Initialize();
		g_progressWindow->SetStatusText(L"Initializing Input...");
        InputManagement->Initialize(m_hWnd);
		g_progressWindow->SetProgress(100);
		
		g_progressWindow->Close();
	})
	.Then([&]
	{
		// 메인 루프
		m_main->Update();
	});
}

LRESULT Core::App::Shutdown(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	m_windowClosed = true;
	PostQuitMessage(0);
	return 0;
}

LRESULT Core::App::ProcessRawInput(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	//InputManagement->ProcessRawInput(lParam); *****

	return 0;
}

LRESULT Core::App::ImGuiKeyDownHandler(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	ImGuiIO& io = ImGui::GetIO();
	ImGuiKey key = ImGuiKey(wParam);

	if (key >= 0 && key < ImGuiKey_COUNT)
	{
		io.AddKeyEvent(key, true);
	}

	return 0;
}

LRESULT Core::App::ImGuiKeyUpHandler(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	ImGuiIO& io = ImGui::GetIO();
	ImGuiKey key = ImGuiKey(wParam);

	if (key >= 0 && key < ImGuiKey_COUNT)
	{
		io.AddKeyEvent(key, false);
	}


	return 0;
}

LRESULT Core::App::HandleCharEvent(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	ImGuiIO& io = ImGui::GetIO();

	wchar_t wch = 0;
	static BYTE KeyState[256];
	GetKeyboardState(KeyState);
	// Virtual Key를 Unicode 문자로 변환
	if (ToUnicode((UINT)wParam, (UINT)lParam, KeyState, &wch, 1, 0) > 0)
	{
		io.AddInputCharacter(wch);
	}

	return 0;
}

LRESULT Core::App::HandleResizeEvent(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	if (wParam == SIZE_MINIMIZED)
	{
		EngineSettingInstance->SetMinimized(true);
		return 0; // 최소화된 경우 무시
	}

	if (EngineSettingInstance->IsMinimized())
	{
		if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)
		{
			EngineSettingInstance->SetMinimized(false);
			return 0; // 복원된 경우 무시
		}
	}

	m_main->InvokeResizeFlag();

	return 0;
}

LRESULT Core::App::HandleSettingWindowEvent(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	WNDCLASS wcSetting = { sizeof(WNDCLASS) };
	wcSetting.lpfnWndProc = CoreWindow::WndProc;

	return 0;
}

LRESULT Core::App::HandleDropFileEvent(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	// 드래그 앤 드롭 이벤트 처리
	HDROP hDrop = (HDROP)wParam;
	UINT nFiles = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);

	if (nFiles > 0)
	{
		std::vector<wchar_t> fileName(MAX_PATH);
		for (UINT i = 0; i < nFiles; ++i)
		{
			DragQueryFile(hDrop, i, fileName.data(), MAX_PATH);
			file::path filePath(fileName.data());
			// 파일 경로 처리
			if(".fbx" == filePath.extension() || ".gltf" == filePath.extension() ||	".obj" == filePath.extension())
			{
				DataSystems->LoadModel(filePath.string());
			}
			else if (
				".png" == filePath.extension() || 
				".dds" == filePath.extension() || 
				".jpg" == filePath.extension() ||
				".hdr" == filePath.extension()
			)
			{
				DataSystems->m_LoadTextureAssetQueue.push(filePath);
			}
            else if (".dmp" == filePath.extension())
            {
               file::path dumpGitHash = GetDumpGitHashADS(filePath);
               if (!dumpGitHash.empty())
               {
                   Debug->LogDebug("Git Hash in dump: " + dumpGitHash.string());
				   std::string command = "https://github.com/29thnight/LastProject/commit/" + dumpGitHash.string();
				   ShellExecuteA(nullptr, "open", command.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
				   DataSystems->OpenFile(filePath);
               }
               else
               {
                   Debug->LogWarning("No Git hash found in ADS stream.");
               }
            }
		}
	}

	return 0;
}

#endif // DYNAMICCPP_EXPORTS