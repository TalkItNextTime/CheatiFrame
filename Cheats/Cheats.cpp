#include "Cheats.h"

#include <chrono>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <limits>
#include <algorithm>
#include <direct.h>
#include <filesystem>

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
	constexpr float kTextShadowOffset = 1.0f;

	// 教学注释：为了提升叠加层在亮背景上的可读性，所有文字都先绘制一层半透明黑色阴影，
	// 再绘制前景色正文。这样在天空、墙面高亮、爆闪等场景里仍能清晰识别提示信息。
	void AddTextShadow(ImDrawList* draw, const ImVec2& pos, ImColor color, const char* text)
	{
		if (!draw || !text || text[0] == '\0')
			return;

		draw->AddText({ pos.x + kTextShadowOffset, pos.y + kTextShadowOffset }, ImColor(0, 0, 0, 220), text);
		draw->AddText(pos, color, text);
	}

	// 教学注释：该重载用于需要指定字号的文本（例如顶部的大号投掷方式提示），
	// 和普通文本一样先画阴影再画正文，避免大字在高亮背景下“发白”看不清。
	void AddTextShadow(ImDrawList* draw, ImFont* font, float fontSize, const ImVec2& pos, ImColor color, const char* text)
	{
		if (!draw || !font || !text || text[0] == '\0')
			return;

		draw->AddText(font, fontSize, { pos.x + kTextShadowOffset, pos.y + kTextShadowOffset }, ImColor(0, 0, 0, 220), text);
		draw->AddText(font, fontSize, pos, color, text);
	}

	// 教学注释：将英文投掷方式标准名转换为中文说明，便于在 HUD 提示和点位文本中直接阅读。
	// 这里使用不区分大小写比较，兼容历史 JSON 中的 StandThrow / standthrow 等写法。
	std::string LocalizeThrowType(std::string throwType)
	{
		std::string lower = throwType;
		std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

		if (lower == "standthrow")
			return u8"直投";
		if (lower == "jumpthrow")
			return u8"跳投";
		if (lower == "runthrow")
			return u8"W跳投";
		return throwType.empty() ? u8"直投" : throwType;
	}

	const char* GrenadeTypeLabelByIndex(const int index)
	{
		switch (index)
		{
		case 0: return "Smoke";
		case 1: return "Flash";
		case 2: return "HE";
		case 3: return "Decoy";
		case 4: return "Molotov";
		default: return "Unknown";
		}
	}

	const char* GrenadeTypeCnLabelByIndex(const int index)
	{
		switch (index)
		{
		case 0: return u8"烟雾弹";
		case 1: return u8"闪光弹";
		case 2: return u8"高爆雷";
		case 3: return u8"诱饵弹";
		case 4: return u8"燃烧弹";
		default: return u8"未知";
		}
	}

	std::string LocalizeGrenadeType(std::string grenadeType)
	{
		std::string lower = grenadeType;
		std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

		if (lower == "smoke")
			return u8"烟雾弹";
		if (lower == "flash")
			return u8"闪光弹";
		if (lower == "he")
			return u8"高爆雷";
		if (lower == "decoy")
			return u8"诱饵弹";
		if (lower == "molotov" || lower == "incendiary")
			return u8"燃烧弹";
		return grenadeType.empty() ? u8"未知" : grenadeType;
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

	int GrenadeTypeIndexByLabel(std::string type)
	{
		std::transform(type.begin(), type.end(), type.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
		if (type == "smoke")
			return 0;
		if (type == "flash")
			return 1;
		if (type == "he")
			return 2;
		if (type == "decoy")
			return 3;
		if (type == "molotov" || type == "incendiary")
			return 4;
		return -1;
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
		bool hasOldPoint = false;
		for (int i = begin; i <= end; ++i)
		{
			if (player.screenBones[i].z > 0.0f && InScreen(screen, player.screenBones[i].x, player.screenBones[i].y))
			{
				if (hasOldPoint)
				{
					ImGui::GetBackgroundDrawList()->AddLine(
						{ oldPoint.x, oldPoint.y },
						{ player.screenBones[i].x, player.screenBones[i].y },
						ImColor(255, 255, 255));
				}
				oldPoint = { player.screenBones[i].x, player.screenBones[i].y };
				hasOldPoint = true;
			}
			else
			{
				hasOldPoint = false;
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
			AddTextShadow(ImGui::GetBackgroundDrawList(), { textX, textY }, ImColor(255, 255, 255), buff);
	}

	// 绘制骨骼连线与头部圆圈。
	void DrawBones(const EspPlayer& player, const ScreenSize& screen)
	{
		ConnectBones(player, screen, 0, 2);
		ConnectBones(player, screen, 3, 9);
		ConnectBones(player, screen, 10, 14);

		if (player.headScreen.z > 0.0f && player.originScreen.z > 0.0f)
		{
			const float bodyHeight = std::max(1.0f, player.originScreen.y - player.headScreen.y);
			const float headRadius = std::clamp(bodyHeight / 11.0f, 3.0f, 28.0f);
			if (InScreen(screen, player.headScreen.x, player.headScreen.y))
			{
				ImGui::GetBackgroundDrawList()->AddCircle(
					{ player.headScreen.x, player.headScreen.y },
					headRadius,
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
			if (Menu::DisplayToggle)
			{
				Menu::helper本次菜单已自动同步 = false;
			}
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
				AddTextShadow(ImGui::GetBackgroundDrawList(), { 20, 20 }, ImColor(255, 0, 0), initError.c_str());
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

		if (settings.helperSyncGrenadeTypePending && !settings.helperManualTypeOverride)
		{
			const std::string grenadeType = ReadCurrentGrenadeType(raw);
			const int typeIndex = GrenadeTypeIndexByLabel(grenadeType);
			if (typeIndex >= 0)
			{
				Menu::helper手动类型 = typeIndex;
				Menu::helper状态 = std::string(u8"已自动识别手雷类型: ") + GrenadeTypeCnLabelByIndex(typeIndex);
			}
			else
			{
				Menu::helper状态 = u8"未识别到可用手雷，保持当前类型";
			}
		}

		HandleConfigRequests(settings);

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
		snapshot.readSleepMs = Menu::read线程休眠毫秒;
		snapshot.espSleepMs = Menu::esp线程休眠毫秒;
		snapshot.aimRetargetDelayMs = Menu::瞄准切换延时毫秒;
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
		strncpy_s(snapshot.helperMapName, Menu::helper地图名, _TRUNCATE);
		strncpy_s(snapshot.helperNote, Menu::helper备注, _TRUNCATE);
		snapshot.helperRecordPending = Menu::helper请求记录;
		snapshot.helperSyncGrenadeTypePending = Menu::helper请求同步手雷类型;
		Menu::helper请求记录 = false;
		Menu::helper请求同步手雷类型 = false;
		snapshot.configRefreshPending = Menu::config请求刷新列表;
		snapshot.configSavePending = Menu::config请求保存;
		snapshot.configLoadPending = Menu::config请求加载;
		strncpy_s(snapshot.configName, Menu::config名称, _TRUNCATE);
		snapshot.configSelectedIndex = Menu::config选择索引;
		Menu::config请求刷新列表 = false;
		Menu::config请求保存 = false;
		Menu::config请求加载 = false;
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

		while (running.load())
		{
			WaitForEnable(readEnabled, readCv, readMutex, running);
			if (!running.load())
				break;

			// Educational note: reading another process memory should only be used for learning.
			SettingsSnapshot settings = SnapshotSettings();
			const int readSleepMs = std::clamp(settings.readSleepMs, 1, 20);

			RawState newRaw{};
			newRaw.entityList = Read<uintptr_t>(client + offsets.dwEntityList);
			if (!newRaw.entityList)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(readSleepMs));
				continue;
			}

			newRaw.matrix = Read<view_matrix_t>(client + offsets.dwViewMatrix);
			newRaw.hasMatrix = newRaw.matrix[0][0] != 0.0f;

			std::uintptr_t localAddr = Read<uintptr_t>(client + offsets.dwLocalPlayerPawn);
			if (!localAddr)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(readSleepMs));
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
			player.sceneNode = Read<uintptr_t>(player.pAddr + offsets.m_pGameSceneNode);
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
				if (offsets.m_vecAbsOrigin && player.sceneNode)
				{
					const Vector absOrigin = Read<Vector>(player.sceneNode + offsets.m_vecAbsOrigin);
					if (!absOrigin.IsZero())
						player.origin = absOrigin;
				}

				// 关键修复说明：
				// 之前将 yaw/bone 放在“每 N 帧重读”的节流逻辑里，会导致骨骼每 N 帧才出现一次，
				// 直接表现为骨骼闪烁；而自瞄依赖 screenBones，目标点也会同频率“卡顿跳变”。
				// 这里改为按需实时读取，消除同频闪烁与一卡一卡的问题。
				if (needYaw)
				{
					Vector ang = Read<Vector>(player.pAddr + offsets.m_angEyeAngles);
					player.pitch = ang.x;
					player.yaw = ang.y;
				}

				player.dis2LP = newRaw.local.origin.CalcDis2Point3D(player.origin);

				if (needBones)
				{
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

			std::this_thread::sleep_for(std::chrono::milliseconds(readSleepMs));
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
			const int espSleepMs = std::clamp(settings.espSleepMs, 1, 30);

			RawState raw{};
			{
				std::shared_lock lock(shared.rawMutex);
				raw = shared.raw;
			}

			EspState newEsp{};
			newEsp.screen = settings.screen;
			if (settings.screen.x <= 0.0f || settings.screen.y <= 0.0f)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(espSleepMs));
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
						Vector headWorld = rp.head;
						if (headWorld.IsZero())
						{
							headWorld = rp.origin;
							headWorld.z += 68.0f;
						}
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

			std::this_thread::sleep_for(std::chrono::milliseconds(espSleepMs));
		}
	}

	// 自瞄线程：执行目标筛选、平滑移动与扳机逻辑。
	void Game::AimWorker()
	{
		bool triggerHolding = false;
		int lastBestIndex = -1;

		while (running.load())
		{
			WaitForEnable(aimEnabled, aimCv, aimMutex, running);
			if (!running.load())
				break;

			SettingsSnapshot settings = SnapshotSettings();
			const int retargetDelayMs = std::clamp(settings.aimRetargetDelayMs, 0, 1000);
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

				if (settings.aimTrigger && (GetAsyncKeyState(settings.triggerKey) & 0x8000))
				{
					if (raw.crosshairEnt > 0 && raw.crosshairEnt <= static_cast<int>(kMaxPlayers))
					{
						const int targetIndex = raw.crosshairEnt - 1;
						const RawPlayer& target = raw.players[targetIndex];
						if (target.valid && (!settings.utilTeamCheck || target.team != raw.local.team))
							triggerShouldHold = true;
					}
				}

				if (retargetDelayMs > 0)
				{
					if (bestIndex == -1)
					{
						if (lastBestIndex != -1)
							std::this_thread::sleep_for(std::chrono::milliseconds(retargetDelayMs));
					}
					else if (lastBestIndex != -1 && bestIndex != lastBestIndex)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(retargetDelayMs));
					}
				}

				lastBestIndex = bestIndex;

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
			if (settings.displayToggle)
				lastBestIndex = -1;

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
			AddTextShadow(
				ImGui::GetBackgroundDrawList(),
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
			case 46: return "Molotov";
			default: break;
			}
		}

		switch (raw.local.activeWeaponDefIndex)
		{
		case 43: return "Flash";
		case 44: return "HE";
		case 45: return "Smoke";
		case 47: return "Decoy";
		case 46: return "Molotov";
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
		// 教学注释：该函数负责把“当前玩家站位 + 当前视角瞄点”固化到 JSON。
		// 录制流程：
		// 1) 读取地图名与手雷类型；
		// 2) 计算站位点与远端瞄点（基于 pitch/yaw）；
		// 3) 追加写入地图文件并立即重载缓存；
		// 4) 回写状态字符串给菜单。
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
		const std::string grenadeTypeCn = LocalizeGrenadeType(grenadeType);
		sprintf_s(statusBuffer, u8"已记录 [%s] %s", grenadeTypeCn.c_str(), spot.name.c_str());
		{
			std::scoped_lock grenadeLock(grenadeMutex);
			grenadeStatus = statusBuffer;
		}
	}

	bool Game::LoadGrenadeJsonFile(const std::string& filePath, GrenadeMapData& out, std::string& error) const
	{
		// 教学注释：读取并解析单张地图的投掷点 JSON。
		// 为了增强健壮性，这里采用“跳过坏条目”策略：
		// 只要条目关键字段缺失，就忽略该条并继续解析下一条。
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
		// 教学注释：将新点位“增量追加”到地图 JSON。
		// 若文件已存在则先读再追加，保证历史点位不会被覆盖；
		// 同时自动分配递增 id，方便后续做删除/编辑功能。
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
		// 教学注释：重载某张地图的点位缓存。
		// 这是“文件层 -> 内存层”的同步入口：
		// - 文件不存在：创建空缓存并提示；
		// - 文件存在：解析后替换缓存并更新版本号。
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
			sprintf_s(buff, u8"已加载 %zu 个点位 (%s)", loaded.spots.size(), loaded.mapName.c_str());
			grenadeStatus = buff;
		}
		return true;
	}

	void Game::EnsureGrenadeMapLoaded(const std::string& mapName)
	{
		// 教学注释：仅在地图名变化时才触发重载，避免每帧都访问磁盘。
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

	// 教学注释：统一处理配置系统请求，避免在渲染线程中直接进行文件 IO。
	// 设计要点：
	// 1) 菜单只负责设置“请求标记”；
	// 2) 主逻辑在每帧读取快照后集中处理请求；
	// 3) 所有结果通过 Menu::config状态 回传给 UI。
	void Game::HandleConfigRequests(const SettingsSnapshot& settings)
	{
		if (!settings.configRefreshPending && !settings.configSavePending && !settings.configLoadPending)
			return;

		if (settings.configRefreshPending)
		{
			Menu::config列表 = ListConfigs();
			if (Menu::config列表.empty())
			{
				Menu::config选择索引 = 0;
				Menu::config状态 = u8"配置列表为空";
			}
			else
			{
				if (Menu::config选择索引 < 0 || Menu::config选择索引 >= static_cast<int>(Menu::config列表.size()))
					Menu::config选择索引 = 0;
				Menu::config状态 = u8"已刷新配置列表";
			}
		}

		if (settings.configSavePending)
		{
			std::string cfgName = settings.configName;
			if (cfgName.empty())
			{
				if (!Menu::config列表.empty() && settings.configSelectedIndex >= 0 && settings.configSelectedIndex < static_cast<int>(Menu::config列表.size()))
					cfgName = Menu::config列表[settings.configSelectedIndex];
			}

			std::string error;
			if (SaveConfig(cfgName, error))
			{
				Menu::config列表 = ListConfigs();
				auto it = std::find(Menu::config列表.begin(), Menu::config列表.end(), cfgName);
				if (it != Menu::config列表.end())
					Menu::config选择索引 = static_cast<int>(std::distance(Menu::config列表.begin(), it));
				Menu::config状态 = std::string(u8"保存成功: ") + cfgName;
			}
			else
			{
				Menu::config状态 = std::string(u8"保存失败: ") + error;
			}
		}

		if (settings.configLoadPending)
		{
			std::string cfgName = settings.configName;
			if ((cfgName.empty() || cfgName == "default") &&
				!Menu::config列表.empty() &&
				settings.configSelectedIndex >= 0 &&
				settings.configSelectedIndex < static_cast<int>(Menu::config列表.size()))
			{
				cfgName = Menu::config列表[settings.configSelectedIndex];
			}

			std::string error;
			if (LoadConfig(cfgName, error))
			{
				strncpy_s(Menu::config名称, cfgName.c_str(), _TRUNCATE);
				Menu::config列表 = ListConfigs();
				auto it = std::find(Menu::config列表.begin(), Menu::config列表.end(), cfgName);
				if (it != Menu::config列表.end())
					Menu::config选择索引 = static_cast<int>(std::distance(Menu::config列表.begin(), it));
				Menu::config状态 = std::string(u8"加载成功: ") + cfgName;
			}
			else
			{
				Menu::config状态 = std::string(u8"加载失败: ") + error;
			}
		}
	}

	// 教学注释：返回 Configs 目录下全部 .json 配置名称（不含扩展名）。
	std::vector<std::string> Game::ListConfigs() const
	{
		std::vector<std::string> names{};
		const std::filesystem::path cfgDir("Configs");

		if (!std::filesystem::exists(cfgDir))
			return names;

		for (const auto& entry : std::filesystem::directory_iterator(cfgDir))
		{
			if (!entry.is_regular_file())
				continue;
			if (entry.path().extension() != ".json")
				continue;
			names.push_back(entry.path().stem().string());
		}

		std::sort(names.begin(), names.end());
		names.erase(std::unique(names.begin(), names.end()), names.end());
		return names;
	}

	// 教学注释：将当前菜单中的关键开关与参数保存到 Configs/{name}.json。
	// 这里刻意保存“用户配置层”而不是运行时缓存层，便于教学和后续扩展。
	bool Game::SaveConfig(const std::string& name, std::string& error) const
	{
		if (name.empty())
		{
			error = u8"配置名不能为空";
			return false;
		}

		std::filesystem::create_directories("Configs");
		const std::filesystem::path path = std::filesystem::path("Configs") / (name + ".json");

		rapidjson::Document doc;
		doc.SetObject();
		auto& allocator = doc.GetAllocator();

		doc.AddMember("name", rapidjson::Value(name.c_str(), allocator), allocator);

		rapidjson::Value visual(rapidjson::kObjectType);
		visual.AddMember("team_check", Menu::util判断阵营, allocator);
		visual.AddMember("visible_check", Menu::util可视检查, allocator);
		visual.AddMember("draw_master", Menu::util绘制总开关, allocator);
		visual.AddMember("box_2d", Menu::vis方框透视, allocator);
		visual.AddMember("box_3d", Menu::vis3DBox透视, allocator);
		visual.AddMember("bones", Menu::vis绘制骨骼, allocator);
		visual.AddMember("health", Menu::vis绘制血条, allocator);
		visual.AddMember("distance", Menu::vis绘制距离, allocator);
		visual.AddMember("cross", Menu::vis绘制准心, allocator);
		doc.AddMember("visual", visual, allocator);

		rapidjson::Value aim(rapidjson::kObjectType);
		aim.AddMember("draw_fov", Menu::aim绘制FOV, allocator);
		aim.AddMember("enabled", Menu::aim自瞄, allocator);
		aim.AddMember("recoil", Menu::aim后座补偿, allocator);
		aim.AddMember("trigger", Menu::aim扳机, allocator);
		aim.AddMember("fov", Menu::aimbotFOV, allocator);
		aim.AddMember("distance", Menu::aimbotDis, allocator);
		aim.AddMember("location", Menu::AimLocation, allocator);
		aim.AddMember("recoil_x", Menu::recoil_X, allocator);
		aim.AddMember("recoil_y", Menu::recoil_Y, allocator);
		aim.AddMember("aim_key", Menu::aimKey, allocator);
		aim.AddMember("trigger_key", Menu::triggerKey, allocator);
		aim.AddMember("mass", Menu::MASS, allocator);
		aim.AddMember("spring", Menu::SPRING_CONSTANT, allocator);
		aim.AddMember("damping", Menu::DAMPING_CONSTANT, allocator);
		aim.AddMember("gravity", Menu::GRAVITY_CONSTANT, allocator);
		aim.AddMember("read_sleep_ms", Menu::read线程休眠毫秒, allocator);
		aim.AddMember("esp_sleep_ms", Menu::esp线程休眠毫秒, allocator);
		aim.AddMember("retarget_delay_ms", Menu::瞄准切换延时毫秒, allocator);
		doc.AddMember("aim", aim, allocator);

		rapidjson::Value helper(rapidjson::kObjectType);
		helper.AddMember("enabled", Menu::helper启用, allocator);
		helper.AddMember("filter_by_weapon", Menu::helper按武器筛选, allocator);
		helper.AddMember("draw_stand", Menu::helper绘制站位, allocator);
		helper.AddMember("draw_aim", Menu::helper绘制瞄点, allocator);
		helper.AddMember("manual_type_override", Menu::helper手动类型覆盖, allocator);
		helper.AddMember("manual_type", Menu::helper手动类型, allocator);
		helper.AddMember("throw_type", Menu::helper投掷方式, allocator);
		helper.AddMember("stand_tolerance", Menu::helper站位容差, allocator);
		helper.AddMember("focus_radius", Menu::helper聚焦半径, allocator);
		helper.AddMember("max_stand_draw_distance", Menu::helper站位最远绘制, allocator);
		helper.AddMember("loose_guide_distance", Menu::helper非聚焦引导线距离, allocator);
		helper.AddMember("record_distance", Menu::helper记录瞄点距离, allocator);
		helper.AddMember("map_name", rapidjson::Value(Menu::helper地图名, allocator), allocator);
		helper.AddMember("note", rapidjson::Value(Menu::helper备注, allocator), allocator);
		doc.AddMember("helper", helper, allocator);

		std::ofstream out(path, std::ios::trunc);
		if (!out.is_open())
		{
			error = u8"无法写入配置文件";
			return false;
		}

		rapidjson::OStreamWrapper outWrapper(out);
		rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(outWrapper);
		writer.SetIndent(' ', 2);
		doc.Accept(writer);
		return true;
	}

	// 教学注释：从命名配置读取数据并回填菜单变量。
	// 使用“字段存在才覆盖”的策略，保证旧版配置文件也能被兼容加载。
	bool Game::LoadConfig(const std::string& name, std::string& error) const
	{
		if (name.empty())
		{
			error = u8"配置名不能为空";
			return false;
		}

		const std::filesystem::path path = std::filesystem::path("Configs") / (name + ".json");
		std::ifstream in(path);
		if (!in.is_open())
		{
			error = u8"配置文件不存在";
			return false;
		}

		rapidjson::IStreamWrapper wrapper(in);
		rapidjson::Document doc;
		doc.ParseStream(wrapper);
		if (doc.HasParseError() || !doc.IsObject())
		{
			error = u8"配置文件 JSON 无效";
			return false;
		}

		if (doc.HasMember("visual") && doc["visual"].IsObject())
		{
			const auto& visual = doc["visual"];
			if (visual.HasMember("team_check") && visual["team_check"].IsBool()) Menu::util判断阵营 = visual["team_check"].GetBool();
			if (visual.HasMember("visible_check") && visual["visible_check"].IsBool()) Menu::util可视检查 = visual["visible_check"].GetBool();
			if (visual.HasMember("draw_master") && visual["draw_master"].IsBool()) Menu::util绘制总开关 = visual["draw_master"].GetBool();
			if (visual.HasMember("box_2d") && visual["box_2d"].IsBool()) Menu::vis方框透视 = visual["box_2d"].GetBool();
			if (visual.HasMember("box_3d") && visual["box_3d"].IsBool()) Menu::vis3DBox透视 = visual["box_3d"].GetBool();
			if (visual.HasMember("bones") && visual["bones"].IsBool()) Menu::vis绘制骨骼 = visual["bones"].GetBool();
			if (visual.HasMember("health") && visual["health"].IsBool()) Menu::vis绘制血条 = visual["health"].GetBool();
			if (visual.HasMember("distance") && visual["distance"].IsBool()) Menu::vis绘制距离 = visual["distance"].GetBool();
			if (visual.HasMember("cross") && visual["cross"].IsBool()) Menu::vis绘制准心 = visual["cross"].GetBool();
		}

		if (doc.HasMember("aim") && doc["aim"].IsObject())
		{
			const auto& aim = doc["aim"];
			if (aim.HasMember("draw_fov") && aim["draw_fov"].IsBool()) Menu::aim绘制FOV = aim["draw_fov"].GetBool();
			if (aim.HasMember("enabled") && aim["enabled"].IsBool()) Menu::aim自瞄 = aim["enabled"].GetBool();
			if (aim.HasMember("recoil") && aim["recoil"].IsBool()) Menu::aim后座补偿 = aim["recoil"].GetBool();
			if (aim.HasMember("trigger") && aim["trigger"].IsBool()) Menu::aim扳机 = aim["trigger"].GetBool();
			if (aim.HasMember("fov") && aim["fov"].IsNumber()) Menu::aimbotFOV = static_cast<float>(aim["fov"].GetDouble());
			if (aim.HasMember("distance") && aim["distance"].IsInt()) Menu::aimbotDis = aim["distance"].GetInt();
			if (aim.HasMember("location") && aim["location"].IsInt()) Menu::AimLocation = aim["location"].GetInt();
			if (aim.HasMember("recoil_x") && aim["recoil_x"].IsInt()) Menu::recoil_X = aim["recoil_x"].GetInt();
			if (aim.HasMember("recoil_y") && aim["recoil_y"].IsInt()) Menu::recoil_Y = aim["recoil_y"].GetInt();
			if (aim.HasMember("aim_key") && aim["aim_key"].IsInt()) Menu::aimKey = aim["aim_key"].GetInt();
			if (aim.HasMember("trigger_key") && aim["trigger_key"].IsInt()) Menu::triggerKey = aim["trigger_key"].GetInt();
			if (aim.HasMember("mass") && aim["mass"].IsNumber()) Menu::MASS = static_cast<float>(aim["mass"].GetDouble());
			if (aim.HasMember("spring") && aim["spring"].IsNumber()) Menu::SPRING_CONSTANT = static_cast<float>(aim["spring"].GetDouble());
			if (aim.HasMember("damping") && aim["damping"].IsNumber()) Menu::DAMPING_CONSTANT = static_cast<float>(aim["damping"].GetDouble());
			if (aim.HasMember("gravity") && aim["gravity"].IsNumber()) Menu::GRAVITY_CONSTANT = static_cast<float>(aim["gravity"].GetDouble());
			if (aim.HasMember("read_sleep_ms") && aim["read_sleep_ms"].IsInt()) Menu::read线程休眠毫秒 = aim["read_sleep_ms"].GetInt();
			if (aim.HasMember("esp_sleep_ms") && aim["esp_sleep_ms"].IsInt()) Menu::esp线程休眠毫秒 = aim["esp_sleep_ms"].GetInt();
			if (aim.HasMember("retarget_delay_ms") && aim["retarget_delay_ms"].IsInt()) Menu::瞄准切换延时毫秒 = aim["retarget_delay_ms"].GetInt();
		}

		if (doc.HasMember("helper") && doc["helper"].IsObject())
		{
			const auto& helper = doc["helper"];
			if (helper.HasMember("enabled") && helper["enabled"].IsBool()) Menu::helper启用 = helper["enabled"].GetBool();
			if (helper.HasMember("filter_by_weapon") && helper["filter_by_weapon"].IsBool()) Menu::helper按武器筛选 = helper["filter_by_weapon"].GetBool();
			if (helper.HasMember("draw_stand") && helper["draw_stand"].IsBool()) Menu::helper绘制站位 = helper["draw_stand"].GetBool();
			if (helper.HasMember("draw_aim") && helper["draw_aim"].IsBool()) Menu::helper绘制瞄点 = helper["draw_aim"].GetBool();
			if (helper.HasMember("manual_type_override") && helper["manual_type_override"].IsBool()) Menu::helper手动类型覆盖 = helper["manual_type_override"].GetBool();
			if (helper.HasMember("manual_type") && helper["manual_type"].IsInt()) Menu::helper手动类型 = helper["manual_type"].GetInt();
			if (helper.HasMember("throw_type") && helper["throw_type"].IsInt()) Menu::helper投掷方式 = helper["throw_type"].GetInt();
			if (helper.HasMember("stand_tolerance") && helper["stand_tolerance"].IsNumber()) Menu::helper站位容差 = static_cast<float>(helper["stand_tolerance"].GetDouble());
			if (helper.HasMember("focus_radius") && helper["focus_radius"].IsNumber()) Menu::helper聚焦半径 = static_cast<float>(helper["focus_radius"].GetDouble());
			if (helper.HasMember("max_stand_draw_distance") && helper["max_stand_draw_distance"].IsNumber()) Menu::helper站位最远绘制 = static_cast<float>(helper["max_stand_draw_distance"].GetDouble());
			if (helper.HasMember("loose_guide_distance") && helper["loose_guide_distance"].IsNumber()) Menu::helper非聚焦引导线距离 = static_cast<float>(helper["loose_guide_distance"].GetDouble());
			if (helper.HasMember("record_distance") && helper["record_distance"].IsNumber()) Menu::helper记录瞄点距离 = static_cast<float>(helper["record_distance"].GetDouble());
			if (helper.HasMember("map_name") && helper["map_name"].IsString()) strncpy_s(Menu::helper地图名, helper["map_name"].GetString(), _TRUNCATE);
			if (helper.HasMember("note") && helper["note"].IsString()) strncpy_s(Menu::helper备注, helper["note"].GetString(), _TRUNCATE);
		}

		return true;
	}

	void Game::RenderGrenadeHelper(const RawState& raw, const SettingsSnapshot& settings) const
	{
		// 教学注释：该函数是投掷物辅助的核心渲染入口。
		// 主要分为四步：
		// 1) 收集可绘制的站位点与瞄点；
		// 2) 进行准星聚焦/鼠标悬停判定；
		// 3) 绘制站位与聚类后的名称列表；
		// 4) 绘制瞄点引导与顶部投掷方式提示。
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

		// 教学注释：站位绘制数据缓存。先收集、后统一渲染，避免逻辑分散。
		struct StandDrawItem
		{
			const GrenadeSpot* spot = nullptr;
			Vector standScreen{};
		};

		// 教学注释：瞄点绘制数据缓存，额外保存到准星/鼠标的距离用于聚焦判定。
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

		// 教学注释：准星聚焦模式。准星足够接近瞄点时，优先高亮最近目标。
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

		// 教学注释：鼠标悬停模式。用于“只看一个目标”的极简视图。
		const bool focusingByMouse = (hoveredSpot != nullptr);
		const bool isolateMode = focusingByMouse;
		const GrenadeSpot* selectedSpot = focusingByMouse ? hoveredSpot : closest;

		auto* draw = ImGui::GetBackgroundDrawList();

		if (settings.helperDrawStand && !standItems.empty())
		{
			// 教学注释：聚类后的展示结构。将空间上相近的多个站位名称合并成“列表列”，
			// 解决密集投掷点位下名称重叠难读的问题。
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
						label += " [" + LocalizeThrowType(spot->throwType) + "]";
					AddTextShadow(draw, { cluster.center.x + 6.0f, cluster.center.y + 8.0f }, ImColor(255, 255, 255), label.c_str());
					continue;
				}

				std::vector<std::string> rows{};
				rows.reserve(cluster.items.size());
				for (const StandDrawItem* rowItem : cluster.items)
				{
					std::string row = rowItem->spot->name;
					if (!rowItem->spot->throwType.empty())
						row += " [" + LocalizeThrowType(rowItem->spot->throwType) + "]";
					rows.push_back(std::move(row));
				}

				std::sort(rows.begin(), rows.end());
				rows.erase(std::unique(rows.begin(), rows.end()), rows.end());

				const float lineHeight = ImGui::GetTextLineHeight();
				const float columnX = cluster.center.x + 12.0f;
				const float columnY = cluster.center.y + 10.0f;

				// 教学注释：按你的要求去掉黑底边框面板，改为“按列纯文本”展示。
				for (size_t i = 0; i < rows.size(); ++i)
				{
					AddTextShadow(
						draw,
						{ columnX, columnY + static_cast<float>(i) * lineHeight },
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
				aimLabel += " [" + LocalizeThrowType(item.spot->throwType) + "]";
			AddTextShadow(
				draw,
				{ item.aimScreen.x + 12.0f, item.aimScreen.y - 16.0f },
				isTarget ? ImColor(255, 245, 190) : ImColor(215, 215, 215),
				aimLabel.c_str());
		}

		if (selectedSpot)
		{
			std::string throwType = LocalizeThrowType(selectedSpot->throwType.empty() ? "StandThrow" : selectedSpot->throwType);
			std::string topText = std::string(u8"投掷方式: ") + throwType;
			if (!selectedSpot->name.empty())
				topText += "  |  " + selectedSpot->name;

			const float fontSize = 30.0f;
			const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, 2000.0f, 0.0f, topText.c_str());
			const ImVec2 textPos{ (settings.screen.x - textSize.x) * 0.5f, 28.0f };

			AddTextShadow(ImGui::GetBackgroundDrawList(), ImGui::GetFont(), fontSize, textPos, ImColor(255, 220, 120), topText.c_str());
		}
	}
}
