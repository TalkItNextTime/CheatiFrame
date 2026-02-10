#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <TlHelp32.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#include "GameState.h"
#include "OffsetsData.h"

namespace Cheats
{
	struct BoneIndex
	{
		int head = 6, spine = 4, hip = 0;
		int hand_l = 11, lowerarm_l = 9, upperarm_l = 8, spine2 = 4;
		int upperarm_r = 13, lowerarm_r = 14, hand_r = 16;
		int calf_l = 24, thigh_l = 23, hip2 = 0;
		int thigh_r = 26, calf_r = 27;
	};

	class Game
	{
	public:
		// 初始化作弊模块：打开进程、绑定模块、加载偏移并启动工作线程。
		bool CheatInit();
		// 每帧主入口：更新配置快照并执行绘制层逻辑。
		void CheatTick();
		// 关闭并清理资源：停止线程并释放进程句柄。
		void Shutdown();

		// 返回是否出现初始化失败（用于UI提示错误信息）。
		bool HasInitError() const;
		// 获取初始化失败的错误字符串。
		const std::string& GetInitError() const;

	private:
		// 创建并启动读取、ESP、自瞄三个工作线程。
		void StartThreads();
		// 通知并等待所有工作线程安全退出。
		void StopThreads();

		// 内存读取线程：周期性采集玩家、骨骼、矩阵等原始数据。
		void ReadWorker();
		// ESP线程：将世界坐标转换为屏幕坐标并构建可绘制数据。
		void EspWorker();
		// 自瞄线程：选择目标并执行鼠标移动/扳机逻辑。
		void AimWorker();

		// 从菜单读取当前配置并写入线程共享快照。
		void UpdateSettingsSnapshot();
		// 按当前配置决定各线程是否需要运行并唤醒它们。
		void UpdateThreadEnableFlags();
		// 线程安全地读取配置快照副本。
		SettingsSnapshot SnapshotSettings() const;

		// 根据ESP状态与设置绘制所有可视化元素。
		void RenderEsp(const EspState& esp, const SettingsSnapshot& settings) const;
		// 绘制后坐力补偿点等自瞄辅助信息。
		void RenderAim(const AimState& aim) const;
		// 绘制准星命中实体等调试信息。
		void RenderCrosshairInfo(const RawState& raw, const SettingsSnapshot& settings) const;

		// 通过模块名查找目标进程模块基址。
		uintptr_t BindModule(DWORD pid, std::wstring_view name);

		template <typename T>
		// 读取目标进程指定地址并按模板类型返回结果。
		T Read(uintptr_t address)
		{
			T buffer{};
			ReadProcessMemory(gamehandle, reinterpret_cast<void*>(address), &buffer, sizeof(T), nullptr);
			return buffer;
		}

		template <typename T>
		// 向目标进程指定地址写入模板类型数据。
		void Write(uintptr_t address, const T& value)
		{
			WriteProcessMemory(gamehandle, reinterpret_cast<void*>(address), &value, sizeof(T), nullptr);
		}

	private:
		HANDLE gamehandle = nullptr;
		ptrdiff_t client = 0;
		OffsetsData offsets{};
		BoneIndex boneindex{};
		SharedState shared{};

		std::atomic<bool> running{ false };
		std::atomic<bool> readEnabled{ false };
		std::atomic<bool> espEnabled{ false };
		std::atomic<bool> aimEnabled{ false };

		std::thread readThread{};
		std::thread espThread{};
		std::thread aimThread{};

		std::mutex readMutex{};
		std::mutex espMutex{};
		std::mutex aimMutex{};
		std::condition_variable readCv{};
		std::condition_variable espCv{};
		std::condition_variable aimCv{};

		bool initAttempted = false;
		bool initOk = false;
		std::string initError{};
	};

	inline Game gameName;

	// 全局作弊主函数：处理热键、菜单和每帧执行。
	void CheatMain();
}
