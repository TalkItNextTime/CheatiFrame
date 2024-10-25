#pragma once


#include <Windows.h>
#include <stdio.h>
#include <chrono> //时间库
#include"../Visuals/Menu.h"
#include"../Visuals/External.h"
#include <TlHelp32.h>


namespace Cheats
{
	class Game
	{
	public:
		HANDLE gamehandle;   //游戏进程句柄
		uintptr_t ClientDllAddr; //client.dll地址

		bool CheatInit(); //初始化
		void CheatLoop(); //遍历

	private:

	uintptr_t bind_modules(DWORD pid, std::string_view mname);


	};
	inline Game gameName ;

	void CheatMain();
}
