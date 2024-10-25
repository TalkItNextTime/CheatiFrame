#include "Cheats.h"
#include "../Visuals/Menu.h"


	void Cheats::CheatMain()
	{
		//获取当下时间
		static std::chrono::time_point time1 = std::chrono::steady_clock::now();
		auto time2 = std::chrono::steady_clock::now();
		//time2 - time1 >= std::chrono::milliseconds(500)

		if (GetAsyncKeyState(VK_INSERT) && time2 - time1 >= std::chrono::milliseconds(200))//菜单呼出热键
		{
			Menu::DisplayToggle = !Menu::DisplayToggle;
			time1 = time2;

		}

		//显示菜单
		if (Menu::DisplayToggle)
		{
			Menu::ShowMenu();
		}

		//功能函数调用
		//初始化一次
		static bool b = true;
		if (b)
		{
			if (!Cheats::gameName.CheatInit())
				return;
			b = !b;
		}

		Cheats::gameName.CheatLoop();

	}

	bool Cheats::Game::CheatInit()
	{
		gamehandle = OpenProcess(PROCESS_ALL_ACCESS, false, Visual::external.gamewindow.pid);
		if (gamehandle == NULL)
		{
			printf("打开游戏进程句柄失败，尝试管理员运行！\r\n");
			return false;
		}

		//获取模块地址
		ClientDllAddr = bind_modules(Visual::external.gamewindow.pid, "client.dll");
		if (ClientDllAddr == NULL)
		{
			CloseHandle(gamehandle);
			return false;
		}
		//entityList = ReadMem<uintptr_t>((uintptr_t)ClientDllAddr + cheat::dwEntityList);

		//if (entityList == NULL)
		//{
		//	printf("没读到entityList\r\n");
		//	return false;
		//}




		return true;
	}

	void Cheats::Game::CheatLoop() //遍历信息
	{


	}











	uintptr_t Cheats::Game::bind_modules(DWORD pid, std::string_view mname)
	{
		HANDLE _handle = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);//遍历句柄
		MODULEENTRY32 mod;
		mod.dwSize = sizeof mod;
		for (Module32First(_handle, &mod); Module32Next(_handle, &mod);)//遍历模块
		{
			if (mname.compare(mod.szModule) == 0)//对比模块名是否符合目标模块名
			{
				
				printf("client.dll Address = %X \r\n", mod.modBaseAddr);
				return (uintptr_t)mod.modBaseAddr;
			}
		}
		if (mod.modBaseAddr == 0x0)
		{
			printf("没找到%s模块,请检查模块名是否正确\r\n", mname);
			throw std::exception("模块基址为空！");
		}
	}