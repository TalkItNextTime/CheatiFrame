#include "Cheats.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <limits>
#include <algorithm>
#include <direct.h>

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>

#include "../Visuals/Menu.h"
#include "../Visuals/External.h"
#include "OffsetsLoader.h"
#include "AlgorAim.h"

constexpr float kPi = 3.14159265358979323846f;

namespace
{
	using Cheats::ScreenSize;
	using Cheats::EspPlayer;
	using Cheats::GrenadeMapData;
	using Cheats::GrenadeSpot;
	using Cheats::kBoneCount;

	constexpr float kGrenadeEyeHeightDefault = 64.0f;

	const char* GrenadeTypeLabelByIndex(const int index)
	{
		switch (index)
		{
		case 0: return "Smoke";
		case 1: return "Flash";
		case 2: return "HE";
		case 3: return "Decoy";
		default: return "Unknown";
		}
	}

	const char* ThrowTypeLabelByIndex(const int index)
	{
		switch (index)
		{
		case 0: return "StandThrow";
		case 1: return "JumpThrow";
		case 2: return "RunThrow";
		default: return "StandThrow";
		}
	}

	bool ReadVectorField(const rapidjson::Value& obj, const char* key, Vector& out)
	{
		if (!obj.HasMember(key) || !obj[key].IsObject())
			return false;

		const auto& node = obj[key];
		if (!node.HasMember("x") || !node["x"].IsNumber() ||
			!node.HasMember("y") || !node["y"].IsNumber() ||
			!node.HasMember("z") || !node["z"].IsNumber())
		{
			return false;
		}

		out.x = static_cast<float>(node["x"].GetDouble());
		out.y = static_cast<float>(node["y"].GetDouble());
		out.z = static_cast<float>(node["z"].GetDouble());
		return true;
	}

	void WriteVectorField(rapidjson::Value& parent,
		const char* key,
		const Vector& vec,
		rapidjson::Document::AllocatorType& allocator)
	{
		rapidjson::Value node(rapidjson::kObjectType);
		node.AddMember("x", vec.x, allocator);
		node.AddMember("y", vec.y, allocator);
		node.AddMember("z", vec.z, allocator);
		parent.AddMember(rapidjson::Value(key, allocator), node, allocator);
	}

	std::string NormalizeMapName(std::string value)
	{
		if (value.rfind("maps/", 0) == 0)
			value = value.substr(5);
		const std::string bsp = ".bsp";
		if (value.size() > bsp.size() && value.substr(value.size() - bsp.size()) == bsp)
			value = value.substr(0, value.size() - bsp.size());
		return value;
	}

	// 判断指定屏幕坐标是否位于窗口范围内。
	bool InScreen(const ScreenSize& screen, float x, float y)
	{
		return x >= 0.0f && y >= 0.0f && x <= screen.x && y <= screen.y;
	}

	// 计算屏幕中心点（准星中心）坐标。
	Vector GetCross(const ScreenSize& screen)
	{
		return { screen.x * 0.5f, screen.y * 0.5f, 0.0f };
	}

	// 将3D世界坐标投影到2D屏幕坐标，失败时返回 false。
	bool WorldToScreen(const Vector& world, const view_matrix_t& matrix, const ScreenSize& screen, Vector& out)
	{
		float x = matrix[0][0] * world.x + matrix[0][1] * world.y + matrix[0][2] * world.z + matrix[0][3];
		float y = matrix[1][0] * world.x + matrix[1][1] * world.y + matrix[1][2] * world.z + matrix[1][3];
		float w = matrix[3][0] * world.x + matrix[3][1] * world.y + matrix[3][2] * world.z + matrix[3][3];

		if (w < 0.1f)
			return false;

		float inv = 1.0f / w;
		x *= inv;
		y *= inv;

		float screenX = screen.x * 0.5f;
		float screenY = screen.y * 0.5f;

		screenX += 0.5f * x * screen.x + 0.5f;
		screenY -= 0.5f * y * screen.y + 0.5f;

		out = { screenX, screenY, w };
		return true;
	}

	// 按骨骼数组区间连接线段，用于绘制骨架。
	void ConnectBones(const EspPlayer& player, const ScreenSize& screen, int begin, int end)
	{
		Vector oldPoint{};
		for (int i = begin; i <= end; ++i)
		{
			if (player.screenBones[i].z > 0.0f && InScreen(screen, player.screenBones[i].x, player.screenBones[i].y))
			{
				if (i != begin)
				{
					ImGui::GetBackgroundDrawList()->AddLine(
						{ oldPoint.x, oldPoint.y },
						{ player.screenBones[i].x, player.screenBones[i].y },
						ImColor(255, 255, 255));
				}
				oldPoint = { player.screenBones[i].x, player.screenBones[i].y };
			}
		}
	}

	// 绘制2D方框ESP。
	void DrawEsp2D(const EspPlayer& player, const ScreenSize& screen)
	{
		if (!player.hasBox2d)
			return;

		const float centerX = (player.esp1.x + player.esp2.x) * 0.5f;
		const float centerY = (player.esp1.y + player.esp2.y) * 0.5f;
		if (!InScreen(screen, centerX, centerY))
			return;

		ImGui::GetBackgroundDrawList()->AddRect(
			{ player.esp1.x, player.esp1.y },
			{ player.esp2.x, player.esp2.y },
			ImColor(255, 0, 0));
	}

	// 绘制血量条，并按血量切换颜色。
	void DrawHealth(const EspPlayer& player)
	{
		if (!player.hasBox2d)
			return;

		ImColor healthColor = ImColor(0, 255, 0);
		if (player.health < 60)
			healthColor = ImColor(255, 255, 0);
		if (player.health < 30)
			healthColor = ImColor(255, 0, 0);

		float height = (player.health / 100.0f) * (player.esp1.y - player.esp2.y);
		ImGui::GetBackgroundDrawList()->AddRect(
			{ player.esp1.x - 7, player.esp1.y },
			{ player.esp1.x - 2, player.esp2.y },
			ImColor(0, 0, 0));
		ImGui::GetBackgroundDrawList()->AddRectFilled(
			{ player.esp1.x - 3, player.esp2.y + height },
			{ player.esp1.x - 6, player.esp2.y },
			healthColor);
	}

	// 在目标下方绘制与本地玩家距离（米）。
	void DrawDistance(const EspPlayer& player, const ScreenSize& screen)
	{
		if (!player.hasBox2d)
			return;

		char buff[64];
		sprintf_s(buff, "%.f m", player.dis2LP / 75.0f);
		const float textX = (player.esp1.x + player.esp2.x) / 2.05f;
		const float textY = player.esp2.y;
		if (InScreen(screen, textX, textY))
			ImGui::GetBackgroundDrawList()->AddText({ textX, textY }, ImColor(255, 255, 255), buff);
	}

	// 绘制骨骼连线与头部圆圈。
	void DrawBones(const EspPlayer& player, const ScreenSize& screen)
	{
		ConnectBones(player, screen, 0, 2);
		ConnectBones(player, screen, 3, 9);
		ConnectBones(player, screen, 10, 14);

		if (player.headScreen.z > 0.0f && player.originScreen.z > 0.0f)
		{
			float headHeight = (player.originScreen.y - player.headScreen.y) / 8.0f;
			if (InScreen(screen, player.headScreen.x, player.headScreen.y))
			{
				ImGui::GetBackgroundDrawList()->AddCircle(
					{ player.headScreen.x, player.headScreen.y },
					headHeight - 3.0f,
					ImColor(255, 255, 255));
			}
		}
	}

	// 绘制3D包围盒（顶面/底面/侧边）。
	void Draw3DBox(const EspPlayer& player)
	{
		if (!player.has3dBox)
			return;

		ImColor color = ImColor(255, 0, 0);
		ImColor frontColor = ImColor(255, 255, 0);

		for (int i = 0; i < 4; ++i)
		{
			const ImColor lineColor = (i == 0 || i == 3) ? frontColor : color;
			ImGui::GetBackgroundDrawList()->AddLine(
				{ player.box3dBottom[i].x, player.box3dBottom[i].y },
				{ player.box3dTop[i].x, player.box3dTop[i].y },
				lineColor,
				1.2f);

			if (i)
			{
				ImGui::GetBackgroundDrawList()->AddLine(
					{ player.box3dBottom[i - 1].x, player.box3dBottom[i - 1].y },
					{ player.box3dBottom[i].x, player.box3dBottom[i].y },
					color);

				ImGui::GetBackgroundDrawList()->AddLine(
					{ player.box3dTop[i - 1].x, player.box3dTop[i - 1].y },
					{ player.box3dTop[i].x, player.box3dTop[i].y },
					color);

				if (i == 3)
				{
					ImGui::GetBackgroundDrawList()->AddLine(
						{ player.box3dBottom[0].x, player.box3dBottom[0].y },
						{ player.box3dBottom[i].x, player.box3dBottom[i].y },
						frontColor,
						1.2f);
					ImGui::GetBackgroundDrawList()->AddLine(
						{ player.box3dTop[0].x, player.box3dTop[0].y },
						{ player.box3dTop[i].x, player.box3dTop[i].y },
						frontColor,
						1.2f);
				}
			}
		}
	}

	// 绘制屏幕中心十字准星。
	void DrawCross(const Vector& cross)
	{
		ImGui::GetBackgroundDrawList()->AddLine({ cross.x - 10, cross.y }, { cross.x + 10, cross.y }, ImColor(255, 0, 0));
		ImGui::GetBackgroundDrawList()->AddLine({ cross.x, cross.y - 10 }, { cross.x, cross.y + 10 }, ImColor(255, 0, 0));
	}

	// 绘制自瞄FOV圆。
	void DrawFov(const Vector& center, float radius)
	{
		ImGui::GetBackgroundDrawList()->AddCircle({ center.x, center.y }, radius, ImColor(255, 255, 255), 18);
	}

	// 线程等待器：等待功能启用或程序退出。
	void WaitForEnable(std::atomic<bool>& enabled, std::condition_variable& cv, std::mutex& mutex, const std::atomic<bool>& running)
	{
		std::unique_lock lock(mutex);
		cv.wait(lock, [&] { return !running.load() || enabled.load(); });
	}
}

namespace Cheats
{
	// 全局入口：处理菜单热键、退出热键并驱动每帧逻辑。
	void CheatMain()
	{
		static auto lastToggle = std::chrono::steady_clock::now();
		auto now = std::chrono::steady_clock::now();

		if (GetAsyncKeyState(VK_INSERT) && now - lastToggle >= std::chrono::milliseconds(200))
		{
			Menu::DisplayToggle = !Menu::DisplayToggle;
			lastToggle = now;
		}

		if (Menu::DisplayToggle)
			Menu::ShowMenu();

		if (GetAsyncKeyState(VK_END) & 0x8000)
		{
			gameName.Shutdown();
			exit(0);
		}

		gameName.CheatTick();
	}

	// 一次性初始化：打开进程、定位模块、读取偏移并启动线程。
	bool Game::CheatInit()
	{
		if (initAttempted)
			return initOk;

		initAttempted = true;

		gamehandle = OpenProcess(PROCESS_ALL_ACCESS, false, Visual::external.gamewindow.pid);
		if (!gamehandle)
		{
			initError = "OpenProcess failed";
			return false;
		}

		client = BindModule(Visual::external.gamewindow.pid, L"client.dll");
		if (!client)
		{
			initError = "client.dll not found";
			CloseHandle(gamehandle);
			gamehandle = nullptr;
			return false;
		}

		std::string error;
		if (!LoadOffsetsFromDir("Offsets", offsets, error))
		{
			initError = error;
			CloseHandle(gamehandle);
			gamehandle = nullptr;
			return false;
		}

		initOk = true;
		running = true;
		StartThreads();
		return true;
	}

	// 每帧执行：确保初始化、更新状态并渲染输出。
	void Game::CheatTick()
	{
		if (!CheatInit())
		{
			if (HasInitError())
			{
				ImGui::GetBackgroundDrawList()->AddText({ 20, 20 }, ImColor(255, 0, 0), initError.c_str());
			}
			return;
		}

		UpdateSettingsSnapshot();
		UpdateThreadEnableFlags();

		SettingsSnapshot settings = SnapshotSettings();
		RawState raw{};
		EspState esp{};
		AimState aim{};

		{
			std::shared_lock lock(shared.rawMutex);
			raw = shared.raw;
		}
		{
			std::shared_lock lock(shared.espMutex);
			esp = shared.esp;
		}
		{
			std::shared_lock lock(shared.aimMutex);
			aim = shared.aim;
		}

		if (settings.helperRecordPending)
			TryRecordGrenadeSpot(raw, settings);

		if (settings.helperEnabled)
		{
			std::string currentMap = ReadCurrentMapName();
			if (currentMap.empty())
				currentMap = NormalizeMapName(Menu::helper地图名);
			EnsureGrenadeMapLoaded(currentMap);
		}

		RenderEsp(esp, settings);
		RenderAim(aim);
		if (settings.helperEnabled)
			RenderGrenadeHelper(raw, settings);
		RenderCrosshairInfo(raw, settings);
	}

	// 关闭模块：停止线程并释放系统句柄。
	void Game::Shutdown()
	{
		StopThreads();
		if (gamehandle)
		{
			CloseHandle(gamehandle);
			gamehandle = nullptr;
		}
	}

	// 是否存在初始化失败状态。
	bool Game::HasInitError() const
	{
		return initAttempted && !initOk;
	}

	// 获取初始化错误文本。
	const std::string& Game::GetInitError() const
	{
		return initError;
	}

	// 启动读取/ESP/自瞄三个工作线程。
	void Game::StartThreads()
	{
		readThread = std::thread(&Game::ReadWorker, this);
		espThread = std::thread(&Game::EspWorker, this);
		aimThread = std::thread(&Game::AimWorker, this);
	}

	// 通知并等待所有工作线程退出。
	void Game::StopThreads()
	{
		if (!running.exchange(false))
			return;

		readCv.notify_all();
		espCv.notify_all();
		aimCv.notify_all();

		if (readThread.joinable())
			readThread.join();
		if (espThread.joinable())
			espThread.join();
		if (aimThread.joinable())
			aimThread.join();
	}

	// 从菜单变量抓取快照，减少多线程直接访问全局菜单状态。
	void Game::UpdateSettingsSnapshot()
	{
		SettingsSnapshot snapshot{};
		snapshot.displayToggle = Menu::DisplayToggle;
		snapshot.utilTeamCheck = Menu::util判断阵营;
		snapshot.utilVisibleCheck = Menu::util可视检查;
		snapshot.utilDraw = Menu::util绘制总开关;
		snapshot.visBox2D = Menu::vis方框透视;
		snapshot.visBox3D = Menu::vis3DBox透视;
		snapshot.visBones = Menu::vis绘制骨骼;
		snapshot.visHealth = Menu::vis绘制血条;
		snapshot.visDistance = Menu::vis绘制距离;
		snapshot.visCross = Menu::vis绘制准心;
		snapshot.aimDrawFov = Menu::aim绘制FOV;
		snapshot.aimEnabled = Menu::aim自瞄;
		snapshot.aimRecoil = Menu::aim后座补偿;
		snapshot.aimTrigger = Menu::aim扳机;
		snapshot.aimbotFOV = Menu::aimbotFOV;
		snapshot.aimbotDis = Menu::aimbotDis;
		snapshot.aimLocation = Menu::AimLocation;
		snapshot.recoilX = Menu::recoil_X;
		snapshot.recoilY = Menu::recoil_Y;
		snapshot.aimKey = Menu::aimKey;
		snapshot.triggerKey = Menu::triggerKey;
		snapshot.mass = Menu::MASS;
		snapshot.spring = Menu::SPRING_CONSTANT;
		snapshot.damping = Menu::DAMPING_CONSTANT;
		snapshot.gravity = Menu::GRAVITY_CONSTANT;
		snapshot.helperEnabled = Menu::helper启用;
		snapshot.helperFilterByWeapon = Menu::helper按武器筛选;
		snapshot.helperDrawStand = Menu::helper绘制站位;
		snapshot.helperDrawAim = Menu::helper绘制瞄点;
		snapshot.helperManualTypeOverride = Menu::helper手动类型覆盖;
		snapshot.helperManualType = Menu::helper手动类型;
		snapshot.helperThrowType = Menu::helper投掷方式;
		snapshot.helperStandTolerance = Menu::helper站位容差;
		snapshot.helperFocusRadius = Menu::helper聚焦半径;
		snapshot.helperMaxStandDrawDistance = Menu::helper站位最远绘制;
		snapshot.helperLooseGuideDistance = Menu::helper非聚焦引导线距离;
		snapshot.helperRecordDistance = Menu::helper记录瞄点距离;
		snapshot.helperRecordPending = Menu::helper请求记录;
		Menu::helper请求记录 = false;
		snapshot.screen = { Visual::external.gamewindow.size.x, Visual::external.gamewindow.size.y };

		if (Menu::helper请求刷新)
		{
			std::string error;
			std::string mapName = NormalizeMapName(Menu::helper地图名);
			if (mapName.empty())
				mapName = ReadCurrentMapName();
			if (mapName.empty())
				mapName = "de_dust2";
			if (ReloadGrenadeMap(mapName, error))
				Menu::helper状态 = u8"已重载地图点位: " + mapName;
			else
				Menu::helper状态 = u8"重载失败: " + error;
			Menu::helper请求刷新 = false;
		}

		{
			std::scoped_lock grenadeLock(grenadeMutex);
			if (!grenadeStatus.empty())
				Menu::helper状态 = grenadeStatus;
		}

		std::unique_lock lock(shared.settingsMutex);
		shared.settings = snapshot;
	}

	// 线程安全读取配置快照。
	SettingsSnapshot Game::SnapshotSettings() const
	{
		std::shared_lock lock(shared.settingsMutex);
		return shared.settings;
	}

	// 按当前功能开关动态决定各线程是否需要工作。
	void Game::UpdateThreadEnableFlags()
	{
		const SettingsSnapshot settings = SnapshotSettings();

		const bool anyEspDraw = settings.utilDraw &&
			(settings.visBox2D || settings.visBox3D || settings.visBones || settings.visHealth || settings.visDistance);
		const bool anyEspAux = settings.visCross || (settings.utilDraw && settings.aimDrawFov);
		const bool espNeeded = anyEspDraw || anyEspAux || settings.aimEnabled || settings.helperEnabled;
		const bool aimNeeded = settings.aimEnabled || settings.aimTrigger || settings.aimRecoil;
		const bool readNeeded = anyEspDraw || aimNeeded || settings.helperEnabled;

		if (readEnabled.exchange(readNeeded) != readNeeded)
			readCv.notify_all();
		if (espEnabled.exchange(espNeeded) != espNeeded)
			espCv.notify_all();
		if (aimEnabled.exchange(aimNeeded) != aimNeeded)
			aimCv.notify_all();
	}

	// 通过ToolHelp遍历模块，返回指定模块基址。
	uintptr_t Game::BindModule(DWORD pid, std::wstring_view name)
	{
		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
		if (snapshot == INVALID_HANDLE_VALUE)
			return 0;

		MODULEENTRY32W mod{};
		mod.dwSize = sizeof(mod);
		for (BOOL ok = Module32FirstW(snapshot, &mod); ok; ok = Module32NextW(snapshot, &mod))
		{
			if (name == std::wstring_view{ mod.szModule })
			{
				CloseHandle(snapshot);
				return reinterpret_cast<uintptr_t>(mod.modBaseAddr);
			}
		}

		CloseHandle(snapshot);
		return 0;
	}

	// 读取线程：从目标进程采样玩家实体、骨骼与战斗状态。
	void Game::ReadWorker()
	{
		const int* boneIds = reinterpret_cast<const int*>(&boneindex);
		int readTick = 0;
		constexpr int kHeavyReadInterval = 10;

		while (running.load())
		{
			WaitForEnable(readEnabled, readCv, readMutex, running);
			if (!running.load())
				break;

			// Educational note: reading another process memory should only be used for learning.
			SettingsSnapshot settings = SnapshotSettings();

			RawState newRaw{};
			newRaw.entityList = Read<uintptr_t>(client + offsets.dwEntityList);
			if (!newRaw.entityList)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
				continue;
			}

			newRaw.matrix = Read<view_matrix_t>(client + offsets.dwViewMatrix);
			newRaw.hasMatrix = newRaw.matrix[0][0] != 0.0f;

			std::uintptr_t localAddr = Read<uintptr_t>(client + offsets.dwLocalPlayerPawn);
			if (!localAddr)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
				continue;
			}

			newRaw.local.valid = true;
			newRaw.local.pAddr = localAddr;
			newRaw.local.origin = Read<Vector>(localAddr + offsets.m_vOldOrigin);
			newRaw.local.viewOffset = Read<Vector>(localAddr + offsets.m_vecViewOffset);
			{
				Vector localEyeAngles = Read<Vector>(localAddr + offsets.m_angEyeAngles);
				newRaw.local.pitch = localEyeAngles.x;
				newRaw.local.yaw = localEyeAngles.y;
			}
			newRaw.local.team = Read<int>(localAddr + offsets.m_iTeamNum);

			const std::uintptr_t clippingWeapon = Read<std::uintptr_t>(localAddr + offsets.m_pClippingWeapon);
			if (clippingWeapon)
			{
				newRaw.local.activeWeaponSubclass = Read<int>(clippingWeapon + offsets.m_nSubclassID);
				const std::uintptr_t attributeManager = clippingWeapon + offsets.m_AttributeManager;
				const std::uintptr_t itemView = attributeManager + offsets.m_Item;
				newRaw.local.activeWeaponDefIndex = Read<std::uint16_t>(itemView + offsets.m_iItemDefinitionIndex);
			}

			newRaw.crosshairEnt = Read<int>(localAddr + offsets.m_iIDEntIndex);
			newRaw.shotsFired = Read<int>(localAddr + offsets.m_iShotsFired);
			newRaw.aimPunch = Read<Vector>(localAddr + offsets.m_aimPunchAngle);

			const bool needBones = settings.aimEnabled || (settings.utilDraw && settings.visBones);
			const bool needYaw = settings.utilDraw && settings.visBox3D;
			const bool shouldReadHeavy = ((++readTick) % kHeavyReadInterval) == 0;

			for (int index = 0; index < static_cast<int>(kMaxPlayers); ++index)
			{
				RawPlayer player{};

				std::uintptr_t listEntry1 = Read<uintptr_t>(newRaw.entityList + (8ull * (index & 0x7FFF) >> 9) + 16);
				if (!listEntry1)
					continue;

				std::uintptr_t playerController = Read<uintptr_t>(listEntry1 + 112ull * (index & 0x1FF));
				if (!playerController)
					continue;

				uint32_t playerPawn = Read<uint32_t>(playerController + offsets.m_hPlayerPawn);
				if (!playerPawn)
					continue;

				std::uintptr_t listEntry2 = Read<uintptr_t>(newRaw.entityList + 0x8ull * ((playerPawn & 0x7FFF) >> 9) + 16);
				if (!listEntry2)
					continue;

				std::uintptr_t pawnPtr = Read<uintptr_t>(listEntry2 + 112ull * (playerPawn & 0x1FF));
				if (!pawnPtr)
					continue;

				if (pawnPtr == localAddr)
					continue;

			player.pAddr = pawnPtr;
			player.team = Read<int>(player.pAddr + offsets.m_iTeamNum);
			if (settings.utilTeamCheck && player.team == newRaw.local.team)
				continue;
			player.lifeState = Read<int>(player.pAddr + offsets.m_lifeState);
				if (player.lifeState != 256)
					continue;

				if (settings.utilVisibleCheck)
					player.spotted = Read<bool>(player.pAddr + offsets.m_entitySpottedState + 0x08);
				else
					player.spotted = true;

				player.health = Read<int>(player.pAddr + offsets.m_iHealth);
				player.origin = Read<Vector>(player.pAddr + offsets.m_vOldOrigin);

				if (needYaw && shouldReadHeavy)
				{
					Vector ang = Read<Vector>(player.pAddr + offsets.m_angEyeAngles);
					player.pitch = ang.x;
					player.yaw = ang.y;
				}

				player.dis2LP = newRaw.local.origin.CalcDis2Point3D(player.origin);

				if (needBones && shouldReadHeavy)
				{
					player.sceneNode = Read<uintptr_t>(player.pAddr + offsets.m_pGameSceneNode);
					if (player.sceneNode)
					{
						player.boneArr = Read<uintptr_t>(player.sceneNode + offsets.m_modelState + 0x80);
						if (player.boneArr)
						{
							player.head = Read<Vector>(player.boneArr + static_cast<std::uintptr_t>(boneindex.head) * 32);
							for (size_t i = 0; i < kBoneCount; ++i)
							{
								const int boneId = boneIds[i];
								player.worldBones[i] = Read<Vector>(player.boneArr + static_cast<std::uintptr_t>(boneId) * 32);
							}
						}
					}
				}

				player.valid = true;
				newRaw.players[index] = player;
			}

			newRaw.hasLocal = true;

			{
				std::unique_lock lock(shared.rawMutex);
				shared.raw = newRaw;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
	}

	// ESP线程：将原始数据转换为屏幕绘制数据。
	void Game::EspWorker()
	{
		while (running.load())
		{
			WaitForEnable(espEnabled, espCv, espMutex, running);
			if (!running.load())
				break;

			SettingsSnapshot settings = SnapshotSettings();

			RawState raw{};
			{
				std::shared_lock lock(shared.rawMutex);
				raw = shared.raw;
			}

			EspState newEsp{};
			newEsp.screen = settings.screen;
			if (settings.screen.x <= 0.0f || settings.screen.y <= 0.0f)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}

			newEsp.cross = GetCross(settings.screen);
			newEsp.fovRadius = settings.aimbotFOV;
			newEsp.showFov = settings.utilDraw && settings.aimDrawFov;
			newEsp.showCross = settings.visCross;

			const bool needBoxes = settings.utilDraw && (settings.visBox2D || settings.visBox3D || settings.visHealth || settings.visDistance);
			const bool needBones = settings.aimEnabled || (settings.utilDraw && settings.visBones);
			const bool need3d = settings.utilDraw && settings.visBox3D;

			if (raw.hasMatrix)
			{
				for (int index = 0; index < static_cast<int>(kMaxPlayers); ++index)
				{
					const RawPlayer& rp = raw.players[index];
					if (!rp.valid)
						continue;

					EspPlayer ep{};
					ep.valid = true;
					ep.origin = rp.origin;
					ep.health = rp.health;
					ep.team = rp.team;
					ep.spotted = rp.spotted;
					ep.dis2LP = rp.dis2LP;
					ep.yaw = rp.yaw;

					if (needBoxes || need3d)
					{
						Vector originScreen{};
						Vector headScreen{};
						Vector headWorld = rp.origin;
						headWorld.z += 68.0f;
						if (WorldToScreen(rp.origin, raw.matrix, settings.screen, originScreen) &&
							WorldToScreen(headWorld, raw.matrix, settings.screen, headScreen))
						{
							ep.originScreen = originScreen;
							ep.headScreen = headScreen;
							ep.espWidth = (originScreen.y - headScreen.y) / 4.0f;
							ep.esp1 = { headScreen.x - ep.espWidth, headScreen.y, 0.0f };
							ep.esp2 = { originScreen.x + ep.espWidth, originScreen.y, 0.0f };
							ep.hasBox2d = true;
						}
					}

					if (needBones)
					{
						for (size_t i = 0; i < kBoneCount; ++i)
						{
							const Vector& worldBone = rp.worldBones[i];
							if (std::abs(worldBone.x) <= 10.0f || std::abs(worldBone.y) <= 10.0f || std::abs(worldBone.z) <= 10.0f)
								continue;

							Vector screenBone{};
							if (WorldToScreen(worldBone, raw.matrix, settings.screen, screenBone))
								ep.screenBones[i] = screenBone;
						}
					}

					if (need3d)
					{
						bool allOk = true;
						const float headZ = rp.origin.z + 68.0f;
						for (int i = 0; i < 4; ++i)
						{
							const int offset = 45 + i * 90;
							Vector bottomWorld{};
							Vector topWorld{};
							const float radians = (rp.yaw + offset) * (kPi / 180.0f);
							bottomWorld.x = rp.origin.x + std::cos(radians) * 25.0f;
							bottomWorld.y = rp.origin.y + std::sin(radians) * 25.0f;
							bottomWorld.z = rp.origin.z;
							topWorld = bottomWorld;
							topWorld.z = headZ;

							Vector bottomScreen{};
							Vector topScreen{};
							if (!WorldToScreen(bottomWorld, raw.matrix, settings.screen, bottomScreen) ||
								!WorldToScreen(topWorld, raw.matrix, settings.screen, topScreen))
							{
								allOk = false;
								break;
							}

							ep.box3dBottom[i] = bottomScreen;
							ep.box3dTop[i] = topScreen;
						}
						ep.has3dBox = allOk;
					}

					newEsp.players[index] = ep;
				}
			}

			newEsp.hasData = true;
			{
				std::unique_lock lock(shared.espMutex);
				shared.esp = newEsp;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(4));
		}
	}

	// 自瞄线程：执行目标筛选、平滑移动与扳机逻辑。
	void Game::AimWorker()
	{
		bool triggerHolding = false;

		while (running.load())
		{
			WaitForEnable(aimEnabled, aimCv, aimMutex, running);
			if (!running.load())
				break;

			SettingsSnapshot settings = SnapshotSettings();
			if (settings.screen.x <= 0.0f || settings.screen.y <= 0.0f)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
				continue;
			}

			RawState raw{};
			EspState esp{};

			{
				std::shared_lock lock(shared.rawMutex);
				raw = shared.raw;
			}
			{
				std::shared_lock lock(shared.espMutex);
				esp = shared.esp;
			}

			AimState newAim{};
			const Vector cross = GetCross(settings.screen);

			if (raw.shotsFired > 1)
			{
				const float alpha = 0.8f;
				newAim.recoilPos = cross;
				newAim.recoilPos.x = newAim.recoilPos.x * (1.0f - alpha) + (cross.x - raw.aimPunch.y * 10.0f) * alpha;
				newAim.recoilPos.y = newAim.recoilPos.y * (1.0f - alpha) + (cross.y + raw.aimPunch.x * 10.0f) * alpha;
				newAim.hasRecoil = true;
			}

			if (!settings.displayToggle)
			{
				bool triggerShouldHold = false;

				int bestIndex = -1;
				float bestDist = std::numeric_limits<float>::max();

				if (settings.aimEnabled)
				{
					for (int i = 0; i < static_cast<int>(kMaxPlayers); ++i)
					{
						const RawPlayer& rp = raw.players[i];
						const EspPlayer& ep = esp.players[i];
						if (!rp.valid || !ep.valid)
							continue;

						if (settings.utilTeamCheck && rp.team == raw.local.team)
							continue;
						if (settings.utilVisibleCheck && !rp.spotted)
							continue;
						if (rp.dis2LP > settings.aimbotDis * 75.0f)
							continue;

						const Vector target = ep.screenBones[settings.aimLocation];
						if (target.z <= 0.0f || !InScreen(settings.screen, target.x, target.y))
							continue;

						float dist = target.CalculateDistanceToPoint2D(cross);
						if (dist < settings.aimbotFOV && dist < bestDist)
						{
							bestDist = dist;
							bestIndex = i;
						}
					}
				}

				if (settings.aimEnabled && (GetAsyncKeyState(settings.aimKey) & 0x8000) && bestIndex != -1)
				{
					const EspPlayer& target = esp.players[bestIndex];
					const Vector targetPos = target.screenBones[settings.aimLocation];
					float moveX = 0.0f;
					float moveY = 0.0f;

					if (settings.aimRecoil && newAim.hasRecoil)
					{
						moveX = targetPos.x - newAim.recoilPos.x + static_cast<float>(settings.recoilX);
						moveY = targetPos.y - newAim.recoilPos.y + static_cast<float>(settings.recoilY);
					}
					else
					{
						moveX = targetPos.x - cross.x;
						moveY = targetPos.y - cross.y;
					}

					float currentMouseX = 0.0f;
					float currentMouseY = 0.0f;
					SpringAlgo(moveX, moveY, moveX, moveY, currentMouseX, currentMouseY,
						settings.spring, settings.damping, settings.gravity, settings.mass);
					mouse_event(MOUSEEVENTF_MOVE, static_cast<LONG>(currentMouseX), static_cast<LONG>(currentMouseY), 0, 0);
				}

				if (settings.aimTrigger && GetAsyncKeyState(settings.triggerKey))
				{
					if (raw.crosshairEnt > 0 && raw.crosshairEnt < static_cast<int>(kMaxPlayers))
					{
						const RawPlayer& target = raw.players[raw.crosshairEnt];
						if (target.valid && (!settings.utilTeamCheck || target.team != raw.local.team))
							triggerShouldHold = true;
					}
				}

				if (triggerShouldHold)
				{
					if (!triggerHolding)
					{
						mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
						triggerHolding = true;
					}
				}
				else if (triggerHolding)
				{
					mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
					triggerHolding = false;
				}
			}
			else if (triggerHolding)
			{
				mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
				triggerHolding = false;
			}

			{
				std::unique_lock lock(shared.aimMutex);
				shared.aim = newAim;
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		if (triggerHolding)
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
	}

	// 渲染所有ESP可视化元素。
	void Game::RenderEsp(const EspState& esp, const SettingsSnapshot& settings) const
	{
		if (esp.screen.x <= 0.0f || esp.screen.y <= 0.0f)
			return;

		if (settings.utilDraw && settings.aimDrawFov)
			DrawFov(esp.cross, esp.fovRadius);

		if (settings.visCross)
			DrawCross(esp.cross);

		if (!settings.utilDraw)
			return;

		for (const auto& player : esp.players)
		{
			if (!player.valid)
				continue;

			if (settings.visBones)
				DrawBones(player, esp.screen);
			if (settings.visBox2D)
				DrawEsp2D(player, esp.screen);
			if (settings.visBox3D)
				Draw3DBox(player);
			if (settings.visHealth)
				DrawHealth(player);
			if (settings.visDistance)
				DrawDistance(player, esp.screen);
		}
	}

	// 渲染自瞄辅助UI（如后坐力点）。
	void Game::RenderAim(const AimState& aim) const
	{
		if (!aim.hasRecoil)
			return;

		ImGui::GetBackgroundDrawList()->AddCircleFilled(
			{ aim.recoilPos.x - 2.0f, aim.recoilPos.y - 2.0f },
			6.0f,
			ImColor(255, 255, 0));
	}

	// 渲染准星命中实体信息（调试/学习用）。
	void Game::RenderCrosshairInfo(const RawState& raw, const SettingsSnapshot& settings) const
	{
		if (raw.crosshairEnt > 0)
		{
			char buff[128];
			sprintf_s(buff, "crosshair id:%d", raw.crosshairEnt);
			ImGui::GetBackgroundDrawList()->AddText(
				{ settings.screen.x * 0.5f - 120.0f, 150.0f },
				ImColor(255, 0, 0),
				buff);
			ImGui::GetBackgroundDrawList()->AddCircleFilled(
				{ settings.screen.x * 0.5f, 200.0f },
				10.0f,
				ImColor(255, 0, 0));
		}
	}

	std::string Game::ReadCurrentMapName() const
	{
		if (!gamehandle || !client || !offsets.dwGlobalVars)
			return {};

		const std::uintptr_t globalVars = Read<std::uintptr_t>(client + offsets.dwGlobalVars);
		if (!globalVars)
			return {};

		const std::uintptr_t mapNamePtr = Read<std::uintptr_t>(globalVars + 0x188);
		if (!mapNamePtr)
			return {};

		char mapBuffer[128]{};
		if (!ReadProcessMemory(gamehandle, reinterpret_cast<void*>(mapNamePtr), mapBuffer, sizeof(mapBuffer) - 1, nullptr))
			return {};

		mapBuffer[sizeof(mapBuffer) - 1] = '\0';
		return NormalizeMapName(std::string(mapBuffer));
	}

	std::string Game::ReadCurrentGrenadeType(const RawState& raw) const
	{
		if (raw.local.activeWeaponSubclass)
		{
			switch (raw.local.activeWeaponSubclass)
			{
			case 43: return "Flash";
			case 44: return "HE";
			case 45: return "Smoke";
			case 47: return "Decoy";
			default: break;
			}
		}

		switch (raw.local.activeWeaponDefIndex)
		{
		case 43: return "Flash";
		case 44: return "HE";
		case 45: return "Smoke";
		case 47: return "Decoy";
		default: return "Unknown";
		}
	}

	Vector Game::ComputeFarAimPoint(const Vector& eyePos, float pitchDeg, float yawDeg, float distance) const
	{
		const float pitch = pitchDeg * (kPi / 180.0f);
		const float yaw = yawDeg * (kPi / 180.0f);

		Vector forward{};
		forward.x = std::cos(pitch) * std::cos(yaw);
		forward.y = std::cos(pitch) * std::sin(yaw);
		forward.z = -std::sin(pitch);

		return {
			eyePos.x + forward.x * distance,
			eyePos.y + forward.y * distance,
			eyePos.z + forward.z * distance,
		};
	}

	void Game::TryRecordGrenadeSpot(const RawState& raw, const SettingsSnapshot& settings)
	{
		if (!raw.hasLocal)
		{
			std::scoped_lock grenadeLock(grenadeMutex);
			grenadeStatus = u8"记录失败：未获取本地玩家";
			return;
		}

		std::string mapName = NormalizeMapName(ReadCurrentMapName());
		if (mapName.empty())
			mapName = NormalizeMapName(Menu::helper地图名);
		if (mapName.empty())
			mapName = "de_dust2";

		std::string grenadeType;
		if (settings.helperManualTypeOverride)
			grenadeType = GrenadeTypeLabelByIndex(settings.helperManualType);
		else
			grenadeType = ReadCurrentGrenadeType(raw);

		if (grenadeType == "Unknown")
		{
			std::scoped_lock grenadeLock(grenadeMutex);
			grenadeStatus = u8"记录失败：当前不是可识别投掷物";
			return;
		}

		const Vector standPos = raw.local.origin;
		Vector eyePos = raw.local.origin;
		if (!raw.local.viewOffset.IsZero())
			eyePos = raw.local.origin + raw.local.viewOffset;
		else
			eyePos.z += kGrenadeEyeHeightDefault;

		const Vector aimPos = ComputeFarAimPoint(eyePos, raw.local.pitch, raw.local.yaw, settings.helperRecordDistance);

		GrenadeSpot spot{};
		spot.type = grenadeType;
		spot.name = Menu::helper备注;
		if (spot.name.empty())
			spot.name = "Unnamed";
		spot.throwType = ThrowTypeLabelByIndex(settings.helperThrowType);
		spot.standPos = standPos;
		spot.aimPos = aimPos;

		_mkdir("GrenadeData");
		const std::string filePath = std::string("GrenadeData\\") + mapName + ".json";

		std::string error;
		if (!AppendGrenadeSpotToJson(filePath, mapName, spot, error))
		{
			std::scoped_lock grenadeLock(grenadeMutex);
			grenadeStatus = u8"记录失败：" + error;
			return;
		}

		ReloadGrenadeMap(mapName, error);

		char statusBuffer[256]{};
		sprintf_s(statusBuffer, "已记录 [%s] %s", grenadeType.c_str(), spot.name.c_str());
		{
			std::scoped_lock grenadeLock(grenadeMutex);
			grenadeStatus = statusBuffer;
		}
	}

	bool Game::LoadGrenadeJsonFile(const std::string& filePath, GrenadeMapData& out, std::string& error) const
	{
		std::ifstream stream(filePath);
		if (!stream.is_open())
		{
			error = "无法打开文件: " + filePath;
			return false;
		}

		rapidjson::IStreamWrapper wrapper(stream);
		rapidjson::Document doc;
		doc.ParseStream(wrapper);
		if (doc.HasParseError() || !doc.IsObject())
		{
			error = "JSON 解析失败";
			return false;
		}

		if (!doc.HasMember("map_name") || !doc["map_name"].IsString())
		{
			error = "缺少 map_name";
			return false;
		}

		if (!doc.HasMember("grenades") || !doc["grenades"].IsArray())
		{
			error = "缺少 grenades 数组";
			return false;
		}

		out.mapName = NormalizeMapName(doc["map_name"].GetString());
		out.spots.clear();
		const auto& list = doc["grenades"];
		out.spots.reserve(list.Size());

		for (rapidjson::SizeType i = 0; i < list.Size(); ++i)
		{
			const auto& item = list[i];
			if (!item.IsObject())
				continue;

			GrenadeSpot spot{};
			if (item.HasMember("id") && item["id"].IsInt())
				spot.id = item["id"].GetInt();
			if (item.HasMember("type") && item["type"].IsString())
				spot.type = item["type"].GetString();
			if (item.HasMember("name") && item["name"].IsString())
				spot.name = item["name"].GetString();
			if (item.HasMember("throw_type") && item["throw_type"].IsString())
				spot.throwType = item["throw_type"].GetString();

			Vector stand{};
			Vector aim{};
			if (!ReadVectorField(item, "position", stand) || !ReadVectorField(item, "aim_target", aim))
				continue;

			spot.standPos = stand;
			spot.aimPos = aim;
			if (spot.name.empty())
				spot.name = "Unnamed";
			if (spot.type.empty())
				spot.type = "Unknown";
			out.spots.push_back(std::move(spot));
		}

		return true;
	}

	bool Game::AppendGrenadeSpotToJson(const std::string& filePath,
		const std::string& mapName,
		const GrenadeSpot& spot,
		std::string& error) const
	{
		rapidjson::Document doc;
		doc.SetObject();
		auto& allocator = doc.GetAllocator();

		if (std::ifstream test(filePath); test.good())
		{
			std::ifstream in(filePath);
			if (!in.is_open())
			{
				error = "打开已有文件失败";
				return false;
			}
			rapidjson::IStreamWrapper inWrapper(in);
			doc.ParseStream(inWrapper);
			if (doc.HasParseError() || !doc.IsObject())
			{
				error = "已有文件不是有效JSON";
				return false;
			}
		}

		if (!doc.HasMember("map_name") || !doc["map_name"].IsString())
			doc.AddMember("map_name", rapidjson::Value(mapName.c_str(), allocator), allocator);

		if (!doc.HasMember("grenades") || !doc["grenades"].IsArray())
			doc.AddMember("grenades", rapidjson::Value(rapidjson::kArrayType), allocator);

		auto& grenades = doc["grenades"];
		int nextId = 1;
		for (auto& it : grenades.GetArray())
		{
			if (it.IsObject() && it.HasMember("id") && it["id"].IsInt())
				nextId = std::max(nextId, it["id"].GetInt() + 1);
		}

		rapidjson::Value node(rapidjson::kObjectType);
		node.AddMember("id", nextId, allocator);
		node.AddMember("type", rapidjson::Value(spot.type.c_str(), allocator), allocator);
		node.AddMember("name", rapidjson::Value(spot.name.c_str(), allocator), allocator);
		WriteVectorField(node, "position", spot.standPos, allocator);
		WriteVectorField(node, "aim_target", spot.aimPos, allocator);
		node.AddMember("throw_type", rapidjson::Value(spot.throwType.c_str(), allocator), allocator);
		grenades.PushBack(node, allocator);

		std::ofstream out(filePath, std::ios::trunc);
		if (!out.is_open())
		{
			error = "写入文件失败";
			return false;
		}

		rapidjson::OStreamWrapper outWrapper(out);
		rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(outWrapper);
		writer.SetIndent(' ', 2);
		doc.Accept(writer);
		return true;
	}

	bool Game::ReloadGrenadeMap(const std::string& mapName, std::string& error)
	{
		GrenadeMapData loaded{};
		const std::string filePath = std::string("GrenadeData\\") + NormalizeMapName(mapName) + ".json";
		const bool hasFile = std::ifstream(filePath).good();

		if (!hasFile)
		{
			loaded.mapName = NormalizeMapName(mapName);
			loaded.spots.clear();
			loaded.revision = ++grenadeRevision;
			{
				std::scoped_lock grenadeLock(grenadeMutex);
				grenadeData = loaded;
				loadedGrenadeMap = loaded.mapName;
				grenadeStatus = u8"未找到点位文件，已创建空缓存: " + loaded.mapName;
			}
			error.clear();
			return true;
		}

		if (!LoadGrenadeJsonFile(filePath, loaded, error))
			return false;

		loaded.revision = ++grenadeRevision;
		{
			std::scoped_lock grenadeLock(grenadeMutex);
			grenadeData = loaded;
			loadedGrenadeMap = loaded.mapName;
			char buff[128]{};
			sprintf_s(buff, "已加载 %zu 个点位 (%s)", loaded.spots.size(), loaded.mapName.c_str());
			grenadeStatus = buff;
		}
		return true;
	}

	void Game::EnsureGrenadeMapLoaded(const std::string& mapName)
	{
		const std::string normalized = NormalizeMapName(mapName);
		if (normalized.empty())
			return;

		{
			std::scoped_lock grenadeLock(grenadeMutex);
			if (loadedGrenadeMap == normalized)
				return;
		}

		std::string error;
		if (!ReloadGrenadeMap(normalized, error))
		{
			std::scoped_lock grenadeLock(grenadeMutex);
			grenadeStatus = u8"加载点位失败: " + error;
		}
	}

	void Game::RenderGrenadeHelper(const RawState& raw, const SettingsSnapshot& settings) const
	{
		if (!raw.hasLocal || !raw.hasMatrix || settings.screen.x <= 0.0f || settings.screen.y <= 0.0f)
			return;

		GrenadeMapData data{};
		{
			std::scoped_lock grenadeLock(grenadeMutex);
			data = grenadeData;
		}

		if (data.spots.empty())
			return;

		const std::string currentWeapon = settings.helperManualTypeOverride
			? GrenadeTypeLabelByIndex(settings.helperManualType)
			: ReadCurrentGrenadeType(raw);
		if (settings.helperFilterByWeapon && currentWeapon == "Unknown")
			return;

		struct StandDrawItem
		{
			const GrenadeSpot* spot = nullptr;
			Vector standScreen{};
		};

		struct AimDrawItem
		{
			const GrenadeSpot* spot = nullptr;
			Vector aimScreen{};
			float distToCross = std::numeric_limits<float>::max();
			float distToMouse = std::numeric_limits<float>::max();
		};

		std::vector<StandDrawItem> standItems{};
		std::vector<AimDrawItem> aimItems{};
		standItems.reserve(data.spots.size());
		aimItems.reserve(data.spots.size());

		const Vector localPos = raw.local.origin;
		const Vector cross = GetCross(settings.screen);
		const ImVec2 mouse = ImGui::GetIO().MousePos;
		const Vector mousePos{ mouse.x, mouse.y, 0.0f };
		constexpr float kAimCircleRadius = 10.0f;
		constexpr float kAimHoverRadius = 16.0f;
		constexpr float kStandClusterRadius = 26.0f;

		const GrenadeSpot* closest = nullptr;
		float minCrossDist = std::numeric_limits<float>::max();

		for (const auto& spot : data.spots)
		{
			if (settings.helperFilterByWeapon && spot.type != currentWeapon)
				continue;

			const float standDist = localPos.CalcDis2Point3D(spot.standPos);
			if (standDist > settings.helperMaxStandDrawDistance)
				continue;

			if (settings.helperDrawStand)
			{
				Vector standScreen{};
				if (WorldToScreen(spot.standPos, raw.matrix, settings.screen, standScreen) &&
					InScreen(settings.screen, standScreen.x, standScreen.y))
				{
					standItems.push_back({ &spot, standScreen });
				}
			}

			if (!settings.helperDrawAim)
				continue;

			if (standDist > settings.helperStandTolerance)
				continue;

			Vector aimScreen{};
			if (!WorldToScreen(spot.aimPos, raw.matrix, settings.screen, aimScreen))
				continue;
			if (!InScreen(settings.screen, aimScreen.x, aimScreen.y))
				continue;

			const float distToCross = aimScreen.CalculateDistanceToPoint2D(cross);
			const float distToMouse = aimScreen.CalculateDistanceToPoint2D(mousePos);
			aimItems.push_back({ &spot, aimScreen, distToCross, distToMouse });

			if (distToCross < minCrossDist)
			{
				minCrossDist = distToCross;
				closest = &spot;
			}
		}

		if (standItems.empty() && aimItems.empty())
			return;

		const bool focusingByCrosshair = minCrossDist <= settings.helperFocusRadius;

		const GrenadeSpot* hoveredSpot = nullptr;
		float minMouseDist = std::numeric_limits<float>::max();
		for (const auto& item : aimItems)
		{
			if (item.distToMouse <= kAimHoverRadius && item.distToMouse < minMouseDist)
			{
				minMouseDist = item.distToMouse;
				hoveredSpot = item.spot;
			}
		}

		const bool focusingByMouse = (hoveredSpot != nullptr);
		const bool isolateMode = focusingByMouse;
		const GrenadeSpot* selectedSpot = focusingByMouse ? hoveredSpot : closest;

		auto* draw = ImGui::GetBackgroundDrawList();

		if (settings.helperDrawStand && !standItems.empty())
		{
			struct StandCluster
			{
				Vector center{};
				std::vector<const StandDrawItem*> items{};
			};

			std::vector<const StandDrawItem*> standRefs{};
			standRefs.reserve(standItems.size());
			for (const auto& item : standItems)
			{
				if (isolateMode && item.spot != selectedSpot)
					continue;
				standRefs.push_back(&item);
			}

			std::vector<StandCluster> clusters{};
			clusters.reserve(standRefs.size());

			for (const StandDrawItem* item : standRefs)
			{
				bool merged = false;
				for (auto& cluster : clusters)
				{
					if (item->standScreen.CalculateDistanceToPoint2D(cluster.center) > kStandClusterRadius)
						continue;

					cluster.items.push_back(item);
					const float count = static_cast<float>(cluster.items.size());
					cluster.center.x = (cluster.center.x * (count - 1.0f) + item->standScreen.x) / count;
					cluster.center.y = (cluster.center.y * (count - 1.0f) + item->standScreen.y) / count;
					merged = true;
					break;
				}

				if (!merged)
				{
					StandCluster cluster{};
					cluster.center = item->standScreen;
					cluster.items.push_back(item);
					clusters.push_back(std::move(cluster));
				}
			}

			for (const auto& cluster : clusters)
			{
				draw->AddCircleFilled({ cluster.center.x, cluster.center.y }, 4.0f, ImColor(255, 255, 0));

				if (cluster.items.empty())
					continue;

				if (cluster.items.size() == 1)
				{
					const GrenadeSpot* spot = cluster.items.front()->spot;
					std::string label = spot->name;
					if (!spot->throwType.empty())
						label += " [" + spot->throwType + "]";
					draw->AddText({ cluster.center.x + 6.0f, cluster.center.y + 8.0f }, ImColor(255, 255, 255), label.c_str());
					continue;
				}

				std::vector<std::string> rows{};
				rows.reserve(cluster.items.size());
				for (const StandDrawItem* rowItem : cluster.items)
				{
					std::string row = rowItem->spot->name;
					if (!rowItem->spot->throwType.empty())
						row += " [" + rowItem->spot->throwType + "]";
					rows.push_back(std::move(row));
				}

				std::sort(rows.begin(), rows.end());
				rows.erase(std::unique(rows.begin(), rows.end()), rows.end());

				float maxWidth = 0.0f;
				for (const auto& row : rows)
				{
					const ImVec2 size = ImGui::CalcTextSize(row.c_str());
					maxWidth = std::max(maxWidth, size.x);
				}

				const float lineHeight = ImGui::GetTextLineHeight();
				const float padding = 6.0f;
				const float boxWidth = maxWidth + padding * 2.0f;
				const float boxHeight = static_cast<float>(rows.size()) * lineHeight + padding * 2.0f;
				const ImVec2 boxPos{ cluster.center.x + 12.0f, cluster.center.y + 10.0f };

				draw->AddRectFilled(boxPos, { boxPos.x + boxWidth, boxPos.y + boxHeight }, ImColor(12, 12, 12, 200), 4.0f);
				draw->AddRect(boxPos, { boxPos.x + boxWidth, boxPos.y + boxHeight }, ImColor(255, 215, 0, 200), 4.0f);

				for (size_t i = 0; i < rows.size(); ++i)
				{
					draw->AddText(
						{ boxPos.x + padding, boxPos.y + padding + static_cast<float>(i) * lineHeight },
						ImColor(255, 255, 255),
						rows[i].c_str());
				}
			}
		}

		for (const auto& item : aimItems)
		{
			if (focusingByMouse && item.spot != hoveredSpot)
				continue;
			if (!focusingByMouse && focusingByCrosshair && item.spot != closest)
				continue;

			const bool isTarget = (item.spot == selectedSpot);
			const ImColor pointColor = isTarget ? ImColor(255, 80, 80) : ImColor(0, 255, 0);
			draw->AddCircle(
				{ item.aimScreen.x, item.aimScreen.y },
				kAimCircleRadius,
				pointColor,
				0,
				2.0f);

			if (focusingByMouse || focusingByCrosshair)
			{
				draw->AddLine(
					{ cross.x, cross.y },
					{ item.aimScreen.x, item.aimScreen.y },
					ImColor(255, 60, 60),
					2.0f);
			}
			else if (item.distToCross <= settings.helperLooseGuideDistance && isTarget)
			{
				draw->AddLine(
					{ cross.x, cross.y },
					{ item.aimScreen.x, item.aimScreen.y },
					ImColor(255, 255, 255, 120),
					1.0f);
			}

			std::string aimLabel = item.spot->name;
			if (!item.spot->throwType.empty())
				aimLabel += " [" + item.spot->throwType + "]";
			draw->AddText(
				{ item.aimScreen.x + 12.0f, item.aimScreen.y - 16.0f },
				isTarget ? ImColor(255, 245, 190) : ImColor(215, 215, 215),
				aimLabel.c_str());
		}

		if (selectedSpot)
		{
			std::string throwType = selectedSpot->throwType.empty() ? "StandThrow" : selectedSpot->throwType;
			std::string topText = std::string(u8"投掷方式: ") + throwType;
			if (!selectedSpot->name.empty())
				topText += "  |  " + selectedSpot->name;

			const float fontSize = 30.0f;
			const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, 2000.0f, 0.0f, topText.c_str());
			const ImVec2 textPos{ (settings.screen.x - textSize.x) * 0.5f, 28.0f };

			draw->AddText(ImGui::GetFont(), fontSize, { textPos.x + 1.0f, textPos.y + 1.0f }, ImColor(0, 0, 0, 220), topText.c_str());
			draw->AddText(ImGui::GetFont(), fontSize, textPos, ImColor(255, 220, 120), topText.c_str());
		}
	}
}
