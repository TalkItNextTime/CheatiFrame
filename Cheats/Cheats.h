#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <TlHelp32.h>

#include <atomic>
#include <condition_variable>
#include <array>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "GameState.h"
#include "OffsetsData.h"
#include "../Math/VisCheckCS2/VisCheckRuntime.h"

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

	struct GrenadeSpot
	{
		int id = 0;
		std::string type{};
		std::string name{};
		std::string throwType{};
		Vector standPos{};
		Vector aimPos{};
	};

	struct GrenadeMapData
	{
		std::string mapName{};
		std::vector<GrenadeSpot> spots{};
		std::uint64_t revision = 0;
	};

	class Game
	{
	public:
		~Game();

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
		static constexpr size_t kSmartBoneCandidateCount = 15;

		struct SmartBoneCandidate
		{
			size_t rawBoneIndex = 0;
		};

		// 返回用于智能部位选择的骨骼优先级数组（头->颈->胸->胃->骨盆->手臂）。
		const std::array<SmartBoneCandidate, kSmartBoneCandidateCount>& GetSmartBonePriority() const;
		// 带边缘容差的骨骼可视检测（主点失败时做偏移点补测）。
		bool IsBoneVisibleWithTolerance(const Vector& localEye, const Vector& worldBone) const;
		// 返回任意可视骨骼索引，若都不可视返回 -1。
		int GetFirstVisibleBoneIndex(const RawState& raw, const RawPlayer& rp) const;
		// 基于 ESP 线程已缓存的可视骨骼结果选择首个可视骨骼，失败返回 -1。
		int GetFirstVisibleBoneIndexFromEsp(const RawPlayer& rp, const EspPlayer& ep) const;
		// 根据 VPK 可视判定选择最佳骨骼索引，失败返回 -1。
		int GetBestVisibleAimBoneIndex(const RawState& raw,
			const RawPlayer& rp,
			const EspPlayer& ep,
			const SettingsSnapshot& settings) const;

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
		void RenderCrosshairInfo(int crosshairEnt, const SettingsSnapshot& settings) const;

		// 通过模块名查找目标进程模块基址。
		uintptr_t BindModule(DWORD pid, std::wstring_view name);
		// 读取CS2全局变量中的当前地图名。
		std::string ReadCurrentMapName() const;
		// 读取当前手持道具类型（Smoke/Flash/HE/Decoy/Unknown）。
		std::string ReadCurrentGrenadeType(const RawState& raw) const;
		// 将投掷角度转换为远点世界坐标（Method 2）。
		Vector ComputeFarAimPoint(const Vector& eyePos, float pitchDeg, float yawDeg, float distance) const;
		// 处理菜单触发的“记录点位”请求。
		void TryRecordGrenadeSpot(const RawState& raw, const SettingsSnapshot& settings);
		// 重载指定地图的点位缓存。
		bool ReloadGrenadeMap(const std::string& mapName, std::string& error);
		// 如地图变化则自动重载点位。
		void EnsureGrenadeMapLoaded(const std::string& mapName);
		// 处理点位列表面板的刷新/保存请求。
		void HandleGrenadeListRequests(const RawState& raw, const SettingsSnapshot& settings);
		// 响应菜单中的配置系统请求（刷新列表/保存/加载）。
		void HandleConfigRequests(const SettingsSnapshot& settings);
		// 枚举 Configs 目录中可用配置名。
		std::vector<std::string> ListConfigs() const;
		// 保存当前菜单设置到命名配置文件。
		bool SaveConfig(const std::string& name, std::string& error) const;
		// 从命名配置文件加载并回填菜单设置。
		bool LoadConfig(const std::string& name, std::string& error) const;
		// 渲染投掷物辅助引导。
		void RenderGrenadeHelper(const RawState& raw, const SettingsSnapshot& settings) const;
		// 读取地图JSON点位文件。
		bool LoadGrenadeJsonFile(const std::string& filePath, GrenadeMapData& out, std::string& error) const;
		// 追加点位到地图JSON文件。
		bool AppendGrenadeSpotToJson(const std::string& filePath,
			const std::string& mapName,
			const GrenadeSpot& spot,
			std::string& error) const;

		template <typename T>
		// 读取目标进程指定地址并按模板类型返回结果。
		T Read(uintptr_t address) const
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
		mutable std::mutex grenadeMutex{};
		std::condition_variable readCv{};
		std::condition_variable espCv{};
		std::condition_variable aimCv{};

		GrenadeMapData grenadeData{};
		std::string loadedGrenadeMap{};
		std::string grenadeStatus = u8"未加载点位";
		std::uint64_t grenadeRevision = 0;

		bool initAttempted = false;
		bool initOk = false;
		std::string initError{};

		std::unique_ptr<VisCheckRuntime> visRuntime{};
	};

	inline Game gameName;

	// 全局作弊主函数：处理热键、菜单和每帧执行。
	void CheatMain();
}
