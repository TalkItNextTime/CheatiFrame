#pragma once
#include "../imgui/imconfig.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx11.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_internal.h"
#include "../imgui/imstb_rectpack.h"
#include "../imgui/imstb_textedit.h"
#include "../imgui/imstb_truetype.h"
#include <tchar.h>
#include <d3d11.h>

#pragma comment (lib,"d3d11.lib")

#include <iostream>
#include <Windows.h>

#include <dwmapi.h>
#pragma comment (lib,"dwmapi.lib")

namespace Visual
{
	typedef void (*Cheat)();


	class External
	{
	public:
		struct windowInfo//窗口信息结构体
		{
			HWND hwnd; //窗口句柄
			DWORD pid; //窗口所属进程pid

			std::string ClassName;
			std::string WindowName;

			ImVec2 pos;		//窗口位置
			ImVec2 size;	//窗口尺寸

		};

		ID3D11Device* g_pd3dDevice = nullptr;
		ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
		IDXGISwapChain* g_pSwapChain = nullptr;
		bool                     g_SwapChainOccluded = false;
		UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
		ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
		WNDCLASSEXW wc;//窗口类
		windowInfo overlaywindow;//透明窗口的窗口信息
		windowInfo gamewindow;//游戏窗口的窗口信息
		Cheat cheeto;	//回调函数指针 该函数用于显示菜单及其他功能

		void AttachWindow(std::string class_name, std::string window_name, Cheat cheat);
	private:



		bool CreateOvelayWindow();//创建透明窗口
		bool InitImgui(); //初始化imgui
		void MessageLoop();//消息循环
		bool UpdateWindow();//更新窗口

		bool CreateDeviceD3D(HWND hWnd);
		void CleanupDeviceD3D();
		void CreateRenderTarget();
		void CleanupRenderTarget();
	};
	inline External external;
}
	

