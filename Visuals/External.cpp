#include "External.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_CREATE:
	{
		MARGINS Margin = { -1 };
		DwmExtendFrameIntoClientArea(hWnd, &Margin);
		break;
	}


	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
			return 0;
		Visual::external.g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
		Visual::external.g_ResizeHeight = (UINT)HIWORD(lParam);
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}



void Visual::External::AttachWindow(std::string class_name, std::string window_name, Cheat cheat)
{
	if (class_name.empty() && window_name.empty() || !cheat)
	{
		printf("AttachWindow参数错误\r\n");
		return ;
	}

	this->cheeto = cheat;
	gamewindow.ClassName = class_name;
	gamewindow.WindowName = window_name;
	gamewindow.hwnd = FindWindowA((class_name.empty() ? NULL : class_name.c_str()), ((window_name.empty() ? NULL : window_name.c_str())));

	if (!gamewindow.hwnd)
	{
		printf("游戏没开呢\r\n");
		return ;
	}

	//2.获取进程pid
	if (GetWindowThreadProcessId(gamewindow.hwnd, &gamewindow.pid) == 0)
	{
		printf("取不到pid\r\n");
		return ;
	}
	printf("pid = %u\r\n", gamewindow.pid);
	//3.打开进程
	//HANDLE game_handle = OpenProcess(PROCESS_ALL_ACCESS, NULL, gamewindow.pid);
	//if (!game_handle)
	//{
	//	printf("打不开进程\r\n", GetLastError());
	//	return 0;
	//}

	//创建透明窗口
	if (!this->CreateOvelayWindow())
	{
		printf("创建透明窗口失败\r\n");
		return ;
	}
	
	//初始化imgui
	if (!this->InitImgui())
	{
		printf("初始化imgui失败\r\n");
		return;
	}

	//消息循环
	this->MessageLoop();
}

bool Visual::External::CreateOvelayWindow()
{

	wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"Cosmic", nullptr };
	::RegisterClassExW(&wc);
	overlaywindow.hwnd = ::CreateWindowExW(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW, wc.lpszClassName, L"Cosmic", WS_POPUP, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

	// Initialize Direct3D
	if (!CreateDeviceD3D(overlaywindow.hwnd))
	{
		printf("创建D3D设备失败\r\n");
		CleanupDeviceD3D();
		::UnregisterClassW(wc.lpszClassName, wc.hInstance);
		return false;
	}

	// Show the window
	::ShowWindow(overlaywindow.hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(overlaywindow.hwnd);
	return true;
}

bool Visual::External::InitImgui()
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();


	if (!ImGui_ImplWin32_Init(overlaywindow.hwnd) || !ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext))
		return false;

	

	//从内存加载字体
	//io.Fonts->AddFontFromMemoryTTF((void*)font_data, font_size, 22.f, nullptr, io.Fonts->GetGlyphRangesChineseFull());
	//从本地文件加载字体
	io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\Deng.ttf", 22.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull());


	return true;
}

void Visual::External::MessageLoop()
{

	ImVec4 clear_color = ImVec4(0.f, 0.f, 0.f, 0.f);
	bool done = false;
	while (!done)
	{

		MSG msg;
		while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				done = true;
		}
		if (done)
			exit(0);
		if (!UpdateWindow())
			exit(0);

		if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
		{
			::Sleep(10);
			continue;
		}
		g_SwapChainOccluded = false;

		if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
		{
			CleanupRenderTarget();
			g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
			g_ResizeWidth = g_ResizeHeight = 0;
			CreateRenderTarget();
		}

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		{
			//绘制区域
			cheeto();
		}



		// Rendering
		ImGui::Render();
		const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
		g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
		g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		// Present
		HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
		//HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
		g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
	}

	// Cleanup
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupDeviceD3D();
	::DestroyWindow(overlaywindow.hwnd);
	::UnregisterClassW(wc.lpszClassName, wc.hInstance);


}

bool Visual::External::UpdateWindow()
{
	POINT Point{};
	RECT Rect{};

	//查找目标窗口
	gamewindow.hwnd = FindWindowA((gamewindow.ClassName.empty() ? NULL : gamewindow.ClassName.c_str()),
		(gamewindow.WindowName.empty() ? NULL : gamewindow.WindowName.c_str()));
	if (gamewindow.hwnd == NULL)
		return false;

	//获取目标窗口位置
	GetClientRect(gamewindow.hwnd, &Rect);
	ClientToScreen(gamewindow.hwnd, &Point);

	//更新透明窗口位置和大小
	overlaywindow.pos = gamewindow.pos = ImVec2((float)Point.x, (float)Point.y);
	overlaywindow.size = gamewindow.size = ImVec2((float)Rect.right, (float)Rect.bottom);
	if (!SetWindowPos(overlaywindow.hwnd,HWND_TOPMOST,(int)overlaywindow.pos.x,(int)overlaywindow.pos.y,(int)overlaywindow.size.x,(int)overlaywindow.size.y,SWP_SHOWWINDOW))
	{
		printf("窗口位置更新失败\r\n");
		return false;
	}

	//获取鼠标位置 同步到imgui中
	POINT MousePos;
	GetCursorPos(&MousePos);
	ScreenToClient(overlaywindow.hwnd, &MousePos);
	ImGui::GetIO().MousePos.x = (float)MousePos.x;
	ImGui::GetIO().MousePos.y = (float)MousePos.y;

	//鼠标穿透
	if (ImGui::GetIO().WantCaptureMouse)
	{
		//如果鼠标在imgui菜单中 就不设置成分层窗口
		//printf("WantCaptureMouse=true\r\n");
		SetWindowLong(overlaywindow.hwnd, GWL_EXSTYLE, GetWindowLong(overlaywindow.hwnd, GWL_EXSTYLE) & (~WS_EX_LAYERED));
	}
	else
	{
		//如果鼠标不在imgui菜单中 就设置成分层窗口
		//printf("WantCaptureMouse=false\r\n");
		SetWindowLong(overlaywindow.hwnd, GWL_EXSTYLE, GetWindowLong(overlaywindow.hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);

	}


	return true;
}

bool Visual::External::CreateDeviceD3D(HWND hWnd)
{
	// Setup swap chain
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT createDeviceFlags = 0;
	//createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
	if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
		res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
	if (res != S_OK)
		return false;

	CreateRenderTarget();
	return true;
}

void Visual::External::CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void Visual::External::CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer;
	g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
	pBackBuffer->Release();
}

void Visual::External::CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

