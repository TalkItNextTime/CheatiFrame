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
#include <unordered_map>

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>

#include "../Visuals/Menu.h"
#include "../Visuals/External.h"
#include "OffsetsLoader.h"
#include "AimCurves.h"

constexpr float kPi = 3.14159265358979323846f;
constexpr uint64_t kVisRayIntervalMs = 50;
constexpr float kOneMeterUnits = 75.0f;
constexpr float kUnitsPerMeterInv = 1.0f / kOneMeterUnits;

namespace
{
	using Cheats::ScreenSize;
	using Cheats::EspPlayer;
	using Cheats::EspState;
	using Cheats::GrenadeMapData;
	using Cheats::GrenadeSpot;
	using Cheats::kBoneCount;
	using Cheats::InputBackend;

	const std::filesystem::path kConfigDir("Configs");
	const std::filesystem::path kLastLoadedConfigFile = kConfigDir / "last_loaded.txt";

	constexpr float kGrenadeEyeHeightDefault = 64.0f;
	constexpr float kTextShadowOffset = 1.0f;

	std::string TrimAsciiWhitespace(const std::string& value)
	{
		auto begin = value.begin();
		while (begin != value.end() && std::isspace(static_cast<unsigned char>(*begin)))
			++begin;

		auto end = value.end();
		while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1))))
			--end;

		return std::string(begin, end);
	}

	std::string NormalizeConfigName(const std::string& raw)
	{
		std::string name = TrimAsciiWhitespace(raw);
		if (name.empty())
			return {};

		for (char ch : name)
		{
			if (static_cast<unsigned char>(ch) < 32)
				return {};
			switch (ch)
			{
			case '\\':
			case '/':
			case ':':
			case '*':
			case '?':
			case '"':
			case '<':
			case '>':
			case '|':
				return {};
			default:
				break;
			}
		}

		return name;
	}

	std::string ReadLastLoadedConfigName()
	{
		std::ifstream in(kLastLoadedConfigFile);
		if (!in.is_open())
			return {};

		std::string name{};
		std::getline(in, name);
		return TrimAsciiWhitespace(name);
	}

	bool WriteLastLoadedConfigName(const std::string& name)
	{
		if (name.empty())
			return false;

		std::error_code ec{};
		std::filesystem::create_directories(kConfigDir, ec);

		std::ofstream out(kLastLoadedConfigFile, std::ios::trunc);
		if (!out.is_open())
			return false;

		out << name;
		return true;
	}

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
			return u8"站投";
		if (lower == "jumpthrow")
			return u8"跳投";
		if (lower == "runthrow")
			return u8"跑投";
		if (lower == "runjumpthrow" || lower == "runjump")
			return u8"跑跳投";
		return throwType.empty() ? u8"站投" : throwType;
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

	bool IsScopeCapableWeapon(int weaponDefIndex)
	{
		switch (weaponDefIndex)
		{
		case 8:   // AUG
		case 9:   // AWP
		case 11:  // G3SG1
		case 38:  // SCAR-20
		case 39:  // SG553
		case 40:  // SSG08
			return true;
		default:
			return false;
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

	Vector ClampToScreenEdge(const ScreenSize& screen, Vector point, float padding = 8.0f)
	{
		const float maxX = std::max(padding, screen.x - padding);
		const float maxY = std::max(padding, screen.y - padding);
		point.x = std::clamp(point.x, padding, maxX);
		point.y = std::clamp(point.y, padding, maxY);
		return point;
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
	void ConnectBones(const EspPlayer& player, const ScreenSize& screen, int begin, int end, ImColor boneColor)
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
						boneColor);
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
	void DrawEsp2D(const EspPlayer& player, const ScreenSize& screen, ImColor color2d)
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
			color2d);
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
		sprintf_s(buff, "%.f m", player.dis2LP * kUnitsPerMeterInv);
		const float textX = (player.esp1.x + player.esp2.x) / 2.05f;
		const float textY = player.esp2.y;
		if (InScreen(screen, textX, textY))
			AddTextShadow(ImGui::GetBackgroundDrawList(), { textX, textY }, ImColor(255, 255, 255), buff);
	}

	// 绘制骨骼连线与头部圆圈。
	void DrawBones(const EspPlayer& player,
		const ScreenSize& screen,
		bool drawSkeleton,
		ImColor boneColor,
		ImColor visibleBoneColor,
		bool drawVisibleBones)
	{
		if (drawSkeleton)
		{
			ConnectBones(player, screen, 0, 2, boneColor);
			ConnectBones(player, screen, 3, 9, boneColor);
			ConnectBones(player, screen, 10, 14, boneColor);

			if (player.headScreen.z > 0.0f && player.originScreen.z > 0.0f)
			{
				const float bodyHeight = std::max(1.0f, player.originScreen.y - player.headScreen.y);
				const float headRadius = std::clamp(bodyHeight / 11.0f, 3.0f, 28.0f);
				if (InScreen(screen, player.headScreen.x, player.headScreen.y))
				{
					ImGui::GetBackgroundDrawList()->AddCircle(
						{ player.headScreen.x, player.headScreen.y },
						headRadius,
						boneColor);
				}
			}
		}

		if (drawVisibleBones)
		{
			for (size_t i = 0; i < kBoneCount; ++i)
			{
				if (!player.visibleBones[i])
					continue;

				const Vector& screenBone = player.screenBones[i];
				if (screenBone.z <= 0.0f || !InScreen(screen, screenBone.x, screenBone.y))
					continue;

				ImGui::GetBackgroundDrawList()->AddCircleFilled(
					{ screenBone.x, screenBone.y },
					3.5f,
					visibleBoneColor);
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

	void DrawBombEsp(const EspState& esp)
	{
		if (!esp.bombVisible || esp.bombScreen.z <= 0.0f)
			return;

		const char* siteName = "?";
		switch (esp.bombSite)
		{
		case 0:
			siteName = "A";
			break;
		case 1:
			siteName = "B";
			break;
		default:
			siteName = "?";
			break;
		}

		char buff[96]{};
		sprintf_s(buff, "C4 [%s]%s", siteName, esp.bombBeingDefused ? " DEFUSING" : "");

		const ImVec2 pos(esp.bombScreen.x + 8.0f, esp.bombScreen.y - 14.0f);
		AddTextShadow(ImGui::GetBackgroundDrawList(), pos, ImColor(255, 120, 40), buff);
		ImGui::GetBackgroundDrawList()->AddCircleFilled({ esp.bombScreen.x, esp.bombScreen.y }, 4.0f, ImColor(255, 120, 40));
	}

	// 线程等待器：等待功能启用或程序退出。
	void WaitForEnable(std::atomic<bool>& enabled, std::condition_variable& cv, std::mutex& mutex, const std::atomic<bool>& running)
	{
		std::unique_lock lock(mutex);
		cv.wait(lock, [&] { return !running.load() || enabled.load(); });
	}

	std::uint64_t GetNowMs()
	{
		using namespace std::chrono;
		return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
	}

	void PreciseSleepMs(const double milliseconds)
	{
		if (milliseconds <= 0.0)
			return;

		using namespace std::chrono;
		auto start = high_resolution_clock::now();
		const auto target = duration<double, std::milli>(milliseconds);

		if (milliseconds > 1.5)
		{
			std::this_thread::sleep_for(duration<double, std::milli>(milliseconds - 0.8));
		}

		while (high_resolution_clock::now() - start < target)
		{
			std::this_thread::yield();
		}
	}
}

namespace Cheats
{
	Game::~Game() = default;

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
				Menu::helper列表等待本次菜单自动高亮 = true;
				Menu::helper列表高亮点位ID = Menu::helper当前瞄准点位ID;
				Menu::helper列表高亮待滚动 = (Menu::helper列表高亮点位ID > 0);
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

		Menu::config列表 = ListConfigs();
		visRuntime = std::make_unique<VisCheckRuntime>();
		{
			const std::string lastLoaded = ReadLastLoadedConfigName();
			if (!lastLoaded.empty())
			{
				std::string cfgError;
				if (LoadConfig(lastLoaded, cfgError))
				{
					strncpy_s(Menu::config名称, lastLoaded.c_str(), _TRUNCATE);
					auto it = std::find(Menu::config列表.begin(), Menu::config列表.end(), lastLoaded);
					if (it != Menu::config列表.end())
						Menu::config选择索引 = static_cast<int>(std::distance(Menu::config列表.begin(), it));
					Menu::config状态 = std::string(u8"启动自动加载: ") + lastLoaded;
				}
				else
				{
					Menu::config状态 = std::string(u8"自动加载失败: ") + cfgError;
				}
			}
			else
			{
				Menu::config状态 = u8"未找到上次加载配置";
			}
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

		// 教学注释：菜单里的“地图名”始终跟随当前实际地图，避免长期停留在默认 de_dust2。
		const std::uint64_t nowMs = GetNowMs();
		std::string currentMap{};
		if (!cachedMapName.empty() && (nowMs - cachedMapNameAtMs) < 1000)
			currentMap = cachedMapName;
		else
		{
			currentMap = NormalizeMapName(ReadCurrentMapName());
			if (!currentMap.empty())
			{
				cachedMapName = currentMap;
				cachedMapNameAtMs = nowMs;
			}
		}
		if (!currentMap.empty())
			strncpy_s(Menu::helper地图名, currentMap.c_str(), _TRUNCATE);
		else
			currentMap = NormalizeMapName(Menu::helper地图名);

		if (visRuntime)
		{
			Menu::utilVPK可视解析 = true;
			visRuntime->SetEnabled(true);
			visRuntime->UpdateMap(currentMap);
			Menu::vpk可视状态 = visRuntime->GetStatusText();
		}

		if (!currentMap.empty())
			EnsureGrenadeMapLoaded(currentMap);

		UpdateSettingsSnapshot();
		HandleMouseWarnings();
		SettingsSnapshot settings = SnapshotSettings();
		const SettingsSnapshot requestSettings = settings;
		HandleMouseInputRequests(settings);
		UpdateSettingsSnapshot();
		settings = SnapshotSettings();
		UpdateThreadEnableFlags();

		RawState raw{};
		AimState aim{};

		{
			std::shared_lock lock(shared.rawMutex);
			raw = shared.raw;
		}
		{
			std::shared_lock lock(shared.aimMutex);
			aim = shared.aim;
		}

		if (requestSettings.helperRecordPending)
			TryRecordGrenadeSpot(raw, requestSettings);

		if (requestSettings.helperSyncGrenadeTypePending && !requestSettings.helperManualTypeOverride)
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

		HandleGrenadeListRequests(raw, requestSettings);

		HandleConfigRequests(requestSettings);

			{
				std::shared_lock lock(shared.espMutex);
				RenderEsp(shared.esp, settings);
			}
				RenderAim(aim);
		if (settings.helperEnabled)
			RenderGrenadeHelper(raw, settings);
		else
			Menu::helper当前瞄准点位ID = 0;
		RenderCrosshairInfo(raw.crosshairEnt, settings);
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
		visRuntime.reset();
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
		Menu::utilVPK可视解析 = true;
		snapshot.utilVpkVisibilityParse = true;
		snapshot.utilDraw = Menu::util绘制总开关;
		snapshot.visBox2D = Menu::vis方框透视;
		snapshot.visBox3D = Menu::vis3DBox透视;
		snapshot.visBones = Menu::vis绘制骨骼;
		snapshot.visVisibleBones = Menu::vis绘制可视骨骼点;
		snapshot.visHealth = Menu::vis绘制血条;
		snapshot.visDistance = Menu::vis绘制距离;
		snapshot.visBombEsp = Menu::vis绘制C4;
		snapshot.visCross = Menu::vis绘制准心;
		for (int i = 0; i < 4; ++i)
		{
			snapshot.colorBones[i] = Menu::color骨骼[i];
			snapshot.colorVisibleBones[i] = Menu::color可视骨骼[i];
			snapshot.colorEsp2D[i] = Menu::color2DESP[i];
		}
		snapshot.aimDrawFov = Menu::aim绘制FOV;
		snapshot.aimEnabled = Menu::aim自瞄;
		snapshot.aimSmartBoneSelection = Menu::aim智能部位选择;
		snapshot.aimRecoil = Menu::aim后座补偿;
		snapshot.aimTrigger = Menu::aim扳机;
		snapshot.aimbotFOV = Menu::aimbotFOV;
		snapshot.aimbotDis = Menu::aimbotDis;
		snapshot.aimLocation = Menu::AimLocation;
		snapshot.recoilX = Menu::recoil_X;
		snapshot.recoilY = Menu::recoil_Y;
		snapshot.aimKey = Menu::aimKey;
		snapshot.triggerKey = Menu::triggerKey;
		snapshot.triggerIntervalMs = std::clamp(Menu::扳机间隔毫秒, 30, 400);
		snapshot.aimFrequencyHz = std::clamp(Menu::瞄准频率Hz, 90, 180);
		snapshot.aimCurveMode = std::clamp(Menu::瞄准曲线模式, 0, 3);
		snapshot.aimCurveSpeed = Menu::曲线速度;
		snapshot.aimCurveXSpeedScale = std::clamp(Menu::曲线X速度比例, 0.20f, 2.00f);
		snapshot.aimCurveYSpeedScale = std::clamp(Menu::曲线Y速度比例, 0.20f, 2.00f);
		snapshot.aimCurveSmoothing = std::clamp(Menu::曲线平滑, 0.30f, 0.95f);
		snapshot.scopedFovScale = std::clamp(Menu::开镜FOV倍率, 1.00f, 2.50f);
		snapshot.readSleepMs = Menu::read线程休眠毫秒;
		snapshot.espSleepMs = Menu::esp线程休眠毫秒;
		snapshot.aimRetargetDelayMs = Menu::瞄准切换延时毫秒;
		snapshot.inputMethodSelected = Menu::输入方式选择;
		snapshot.inputMethodApplied = Menu::输入方式当前生效;
		strncpy_s(snapshot.inputEndpoint, Menu::输入地址, _TRUNCATE);
		strncpy_s(snapshot.inputUuid, Menu::输入UUID, _TRUNCATE);
		snapshot.inputConnectRequest = Menu::输入请求连接测试;
		snapshot.inputAutoConnect = Menu::输入自动连接;
		Menu::输入请求连接测试 = false;
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
		snapshot.helperTopHintOffsetX = Menu::helper顶部提示偏移X;
		snapshot.helperTopHintOffsetY = Menu::helper顶部提示偏移Y;
		strncpy_s(snapshot.helperMapName, Menu::helper地图名, _TRUNCATE);
		strncpy_s(snapshot.helperNote, Menu::helper备注, _TRUNCATE);
		snapshot.helperRecordPending = Menu::helper请求记录;
		snapshot.helperSyncGrenadeTypePending = Menu::helper请求同步手雷类型;
		snapshot.helperListRefreshPending = Menu::helper列表请求刷新;
		snapshot.helperListSavePending = Menu::helper列表请求保存;
		Menu::helper请求记录 = false;
		Menu::helper请求同步手雷类型 = false;
		Menu::helper列表请求刷新 = false;
		Menu::helper列表请求保存 = false;
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
				mapName = cachedMapName;
			if (mapName.empty())
				mapName = NormalizeMapName(ReadCurrentMapName());
			if (!mapName.empty())
			{
				cachedMapName = mapName;
				cachedMapNameAtMs = GetNowMs();
			}
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
			if (!grenadeStatus.empty())
				Menu::helper列表状态 = grenadeStatus;
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
			(settings.visBox2D || settings.visBox3D || settings.visBones || settings.visHealth || settings.visDistance || settings.visBombEsp);
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

	void Game::HandleMouseWarnings()
	{
		std::string warning = mouseController.ConsumeWarning();
		if (warning.empty())
			return;

		Menu::输入方式当前生效 = static_cast<int>(mouseController.ActiveBackend());
		Menu::输入连接成功 = false;
		Menu::输入状态 = warning;
		Menu::输入调试状态 = std::string(u8"运行时输入警告: ") + warning;
		Menu::输入提示 = warning;
		Menu::输入提示警告 = true;
		Menu::输入提示截止时间Ms = GetNowMs() + 4000;
	}

	void Game::HandleMouseInputRequests(const SettingsSnapshot& settings)
	{
		const bool manualConnectRequested = settings.inputConnectRequest;
		const bool shouldAutoConnect = settings.inputAutoConnect &&
			settings.inputMethodSelected != static_cast<int>(InputBackend::WinAPI) &&
			settings.inputMethodSelected != settings.inputMethodApplied;

		if (!manualConnectRequested && !shouldAutoConnect)
			return;

		MouseConnectParams params{};
		if (settings.inputMethodSelected == static_cast<int>(InputBackend::KmboxNet))
			params.backend = InputBackend::KmboxNet;
		else if (settings.inputMethodSelected == static_cast<int>(InputBackend::KmboxBPro))
			params.backend = InputBackend::KmboxBPro;
		else
			params.backend = InputBackend::WinAPI;

		if (params.backend != InputBackend::WinAPI && !manualConnectRequested && !shouldAutoConnect)
			return;

		params.endpoint = settings.inputEndpoint;
		params.uuid = settings.inputUuid;

		std::string message{};
		if (mouseController.Connect(params, message))
		{
			Menu::输入方式当前生效 = static_cast<int>(params.backend);
			Menu::输入方式选择 = static_cast<int>(params.backend);
			Menu::输入连接成功 = true;
			Menu::输入状态 = message;
			std::string triggerDebug{};
			if (mouseController.TriggerClickDebug(triggerDebug))
				Menu::输入调试状态 = std::string(u8"连接后扳机测试成功: ") + triggerDebug;
			else
				Menu::输入调试状态 = std::string(u8"连接后扳机测试失败: ") + triggerDebug;
			Menu::输入提示 = message;
			Menu::输入提示警告 = false;
			Menu::输入提示截止时间Ms = GetNowMs() + 2500;
			return;
		}

		Menu::输入方式当前生效 = static_cast<int>(mouseController.ActiveBackend());
		Menu::输入连接成功 = false;
		Menu::输入状态 = std::string(u8"输入连接失败: ") + message;
		Menu::输入调试状态 = std::string(u8"连接阶段失败，未执行扳机测试: ") + message;
		Menu::输入提示 = Menu::输入状态;
		Menu::输入提示警告 = true;
		Menu::输入提示截止时间Ms = GetNowMs() + 5000;

		if (shouldAutoConnect)
		{
			Menu::输入提示 = std::string(u8"自动连接失败，请检查参数后点击 Connect / Test: ") + message;
		}
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
		constexpr std::size_t kBoneStride = 32;
		constexpr std::size_t kBoneBatchCount = 32;
		constexpr std::size_t kBoneBatchBytes = kBoneStride * kBoneBatchCount;

		auto readPtrCached = [&](std::uintptr_t address, std::uintptr_t& out, ReadPageCache& cache) -> bool
		{
			return ReadCached(address, out, cache);
		};

		auto readPtrDirect = [&](std::uintptr_t address, std::uintptr_t& out) -> bool
		{
			return ReadInto(address, out);
		};

		while (running.load())
		{
			WaitForEnable(readEnabled, readCv, readMutex, running);
			if (!running.load())
				break;

			// Educational note: reading another process memory should only be used for learning.
			SettingsSnapshot settings = SnapshotSettings();
			const int readSleepMs = std::clamp(settings.readSleepMs, 1, 20);

			ReadPageCache localCache{};
			ReadPageCache entityListPageCache{};
 
			auto safeReadCached = [&](std::uintptr_t address, auto& out, ReadPageCache& cache) {
				if (!ReadCached(address, out, cache))
					out = {};
			};

			auto safeReadInto = [&](std::uintptr_t address, auto& out) {
				if (!ReadInto(address, out))
					out = {};
			};

			auto safeReadBuffer = [&](std::uintptr_t address, void* out, std::size_t size) -> bool {
				if (!ReadBuffer(address, out, size))
				{
					if (out && size > 0)
						std::memset(out, 0, size);
					return false;
				}
				return true;
			};

			RawState newRaw{};
			if (!ReadInto(client + offsets.dwEntityList, newRaw.entityList))
				newRaw.entityList = 0;
			if (!ReadInto(client + offsets.dwPlantedC4, newRaw.plantedC4))
				newRaw.plantedC4 = 0;
			if (newRaw.plantedC4)
			{
				if (offsets.m_bBombTicking)
					safeReadCached(newRaw.plantedC4 + offsets.m_bBombTicking, newRaw.bombTicking, localCache);
				if (offsets.m_bBombDefused)
					safeReadCached(newRaw.plantedC4 + offsets.m_bBombDefused, newRaw.bombDefused, localCache);
				if (offsets.m_bBeingDefused)
					safeReadCached(newRaw.plantedC4 + offsets.m_bBeingDefused, newRaw.bombBeingDefused, localCache);

				if (offsets.m_nBombSite)
				{
					if (!ReadCached(newRaw.plantedC4 + offsets.m_nBombSite, newRaw.bombSite, localCache))
						newRaw.bombSite = -1;
				}
				else
				{
					newRaw.bombSite = -1;
				}

				safeReadCached(newRaw.plantedC4 + offsets.m_vOldOrigin, newRaw.plantedC4Pos, localCache);
				if (offsets.m_vecAbsOrigin)
				{
					std::uintptr_t c4SceneNode = 0;
					safeReadCached(newRaw.plantedC4 + offsets.m_pGameSceneNode, c4SceneNode, localCache);
					if (c4SceneNode)
					{
						Vector absOrigin{};
						safeReadInto(c4SceneNode + offsets.m_vecAbsOrigin, absOrigin);
						if (!absOrigin.IsZero())
							newRaw.plantedC4Pos = absOrigin;
					}
				}
			}
			else
			{
				newRaw.bombTicking = false;
				newRaw.bombDefused = false;
				newRaw.bombBeingDefused = false;
				newRaw.bombSite = -1;
				newRaw.plantedC4Pos = {};
			}
			if (!newRaw.entityList)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(readSleepMs));
				continue;
			}

			safeReadInto(client + offsets.dwViewMatrix, newRaw.matrix);
			newRaw.hasMatrix = newRaw.matrix[0][0] != 0.0f;

		std::uintptr_t localAddr = 0;
		safeReadInto(client + offsets.dwLocalPlayerPawn, localAddr);
		if (!localAddr)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(readSleepMs));
			continue;
		}

		localCache = {};
		ReadPageCache localHandleCache{};

				newRaw.local.valid = true;
				newRaw.local.pAddr = localAddr;
				safeReadCached(localAddr + offsets.m_vOldOrigin, newRaw.local.origin, localCache);
				if (offsets.m_vecAbsOrigin)
				{
					std::uintptr_t localSceneNode = 0;
					safeReadCached(localAddr + offsets.m_pGameSceneNode, localSceneNode, localCache);
					if (localSceneNode)
					{
						Vector localAbsOrigin{};
						safeReadInto(localSceneNode + offsets.m_vecAbsOrigin, localAbsOrigin);
						if (!localAbsOrigin.IsZero())
							newRaw.local.origin = localAbsOrigin;
					}
				}
				safeReadCached(localAddr + offsets.m_vecViewOffset, newRaw.local.viewOffset, localCache);
			{
				Vector localEyeAngles{};
				safeReadCached(localAddr + offsets.m_angEyeAngles, localEyeAngles, localCache);
				newRaw.local.pitch = localEyeAngles.x;
				newRaw.local.yaw = localEyeAngles.y;
			}
			safeReadCached(localAddr + offsets.m_iTeamNum, newRaw.local.team, localCache);
			if (offsets.m_bIsScoped)
				safeReadCached(localAddr + offsets.m_bIsScoped, newRaw.local.isScoped, localCache);
			else
				newRaw.local.isScoped = false;

		std::uintptr_t clippingWeapon = 0;
		safeReadCached(localAddr + offsets.m_pClippingWeapon, clippingWeapon, localHandleCache);
			if (clippingWeapon)
			{
				safeReadInto(clippingWeapon + offsets.m_nSubclassID, newRaw.local.activeWeaponSubclass);
				const std::uintptr_t attributeManager = clippingWeapon + offsets.m_AttributeManager;
				const std::uintptr_t itemView = attributeManager + offsets.m_Item;
				std::uint16_t defIndex = 0;
				safeReadInto(itemView + offsets.m_iItemDefinitionIndex, defIndex);
				newRaw.local.activeWeaponDefIndex = defIndex;
			}

			safeReadCached(localAddr + offsets.m_iIDEntIndex, newRaw.crosshairEnt, localCache);
			safeReadCached(localAddr + offsets.m_iShotsFired, newRaw.shotsFired, localCache);
			safeReadCached(localAddr + offsets.m_aimPunchAngle, newRaw.aimPunch, localCache);

			const bool needAllBones =
				(settings.utilDraw && (settings.visBones || settings.visVisibleBones)) ||
				(settings.aimEnabled && settings.utilVpkVisibilityParse);
			const bool needAimBoneOnly = settings.aimEnabled && !needAllBones;
			const int aimBoneIndex = std::clamp(settings.aimLocation, 0, static_cast<int>(kBoneCount) - 1);
			const bool needHeadBone = settings.utilDraw &&
				(settings.visBox2D || settings.visBox3D || settings.visHealth || settings.visDistance || settings.visBones || settings.visVisibleBones);
			const bool needBones = needAllBones || needAimBoneOnly || needHeadBone;
			const bool needPlayerSceneNode = (offsets.m_vecAbsOrigin != 0) || needBones;
			const bool needYaw = settings.utilDraw && settings.visBox3D;

		ReadPageCache controllerHandleCache{};
		for (int index = 0; index < static_cast<int>(kMaxPlayers); ++index)
		{
				RawPlayer player{};

				const std::uintptr_t listEntry1Address = newRaw.entityList + (8ull * (index & 0x7FFF) >> 9) + 16;
				std::uintptr_t listEntry1 = 0;
				if (!readPtrCached(listEntry1Address, listEntry1, entityListPageCache))
					continue;
				if (!listEntry1)
					continue;

				std::uintptr_t playerController = 0;
				if (!readPtrDirect(listEntry1 + 112ull * (index & 0x1FF), playerController))
					continue;
				if (!playerController)
					continue;

			std::uint32_t playerPawn = 0;
			if (!ReadCached(playerController + offsets.m_hPlayerPawn, playerPawn, controllerHandleCache))
				continue;
				if (!playerPawn)
					continue;

				const std::uintptr_t listEntry2Address = newRaw.entityList + 0x8ull * ((playerPawn & 0x7FFF) >> 9) + 16;
				std::uintptr_t listEntry2 = 0;
				if (!readPtrCached(listEntry2Address, listEntry2, entityListPageCache))
					continue;
				if (!listEntry2)
					continue;

				std::uintptr_t pawnPtr = 0;
				if (!readPtrDirect(listEntry2 + 112ull * (playerPawn & 0x1FF), pawnPtr))
					continue;
				if (!pawnPtr)
					continue;

				if (pawnPtr == localAddr)
					continue;

			player.pAddr = pawnPtr;
			ReadPageCache playerCache{};
			if (needPlayerSceneNode)
				safeReadCached(player.pAddr + offsets.m_pGameSceneNode, player.sceneNode, playerCache);
			safeReadCached(player.pAddr + offsets.m_iTeamNum, player.team, playerCache);
			if (settings.utilTeamCheck && player.team == newRaw.local.team)
				continue;
			safeReadCached(player.pAddr + offsets.m_lifeState, player.lifeState, playerCache);
				if (player.lifeState != 256)
					continue;

				if (settings.utilVisibleCheck && !settings.utilVpkVisibilityParse)
					safeReadCached(player.pAddr + offsets.m_entitySpottedState + 0x08, player.spotted, playerCache);
				else
					player.spotted = true;

				safeReadCached(player.pAddr + offsets.m_iHealth, player.health, playerCache);
				safeReadCached(player.pAddr + offsets.m_vOldOrigin, player.origin, playerCache);
				if (offsets.m_vecAbsOrigin && player.sceneNode)
				{
					Vector absOrigin{};
					safeReadInto(player.sceneNode + offsets.m_vecAbsOrigin, absOrigin);
					if (!absOrigin.IsZero())
						player.origin = absOrigin;
				}

				// 关键修复说明：
				// 之前将 yaw/bone 放在“每 N 帧重读”的节流逻辑里，会导致骨骼每 N 帧才出现一次，
				// 直接表现为骨骼闪烁；而自瞄依赖 screenBones，目标点也会同频率“卡顿跳变”。
				// 这里改为按需实时读取，消除同频闪烁与一卡一卡的问题。
				if (needYaw)
				{
					Vector ang{};
					safeReadCached(player.pAddr + offsets.m_angEyeAngles, ang, playerCache);
					player.pitch = ang.x;
					player.yaw = ang.y;
				}

				player.dis2LPSqr = newRaw.local.origin.CalcDis2Point3DSqr(player.origin);
				player.dis2LP = std::sqrt(player.dis2LPSqr);

				if (needBones)
				{
					if (player.sceneNode)
					{
						safeReadInto(player.sceneNode + offsets.m_modelState + 0x80, player.boneArr);
						if (player.boneArr)
						{
							if (needAllBones)
							{
								std::array<std::byte, kBoneBatchBytes> boneBatch{};
								if (safeReadBuffer(player.boneArr, boneBatch.data(), boneBatch.size()))
								{
									for (size_t i = 0; i < kBoneCount; ++i)
									{
										const int boneId = boneIds[i];
										if (boneId < 0 || static_cast<std::size_t>(boneId) >= kBoneBatchCount)
											continue;
										Vector bone{};
										std::memcpy(&bone, boneBatch.data() + static_cast<std::size_t>(boneId) * kBoneStride, sizeof(Vector));
										player.worldBones[i] = bone;
									}
									if (boneindex.head >= 0 && static_cast<std::size_t>(boneindex.head) < kBoneBatchCount)
									{
										std::memcpy(&player.head,
											boneBatch.data() + static_cast<std::size_t>(boneindex.head) * kBoneStride,
											sizeof(Vector));
									}
								}
							}
							else
							{
								if (needHeadBone)
									safeReadInto(player.boneArr + static_cast<std::uintptr_t>(boneindex.head) * kBoneStride, player.head);

								if (needAimBoneOnly)
								{
									const int aimBoneId = boneIds[aimBoneIndex];
									safeReadInto(player.boneArr + static_cast<std::uintptr_t>(aimBoneId) * kBoneStride, player.worldBones[aimBoneIndex]);
								}
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
			newEsp.localTeam = raw.local.team;
			if (settings.screen.x <= 0.0f || settings.screen.y <= 0.0f)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(espSleepMs));
				continue;
			}

			newEsp.cross = GetCross(settings.screen);
			const bool scopedFovBoost = raw.local.isScoped && IsScopeCapableWeapon(raw.local.activeWeaponDefIndex);
			const float scopedScale = std::clamp(settings.scopedFovScale, 1.0f, 2.5f);
			newEsp.fovRadius = settings.aimbotFOV * (scopedFovBoost ? scopedScale : 1.0f);
			const float fovRadiusSqr = newEsp.fovRadius * newEsp.fovRadius;
			newEsp.showFov = settings.utilDraw && settings.aimDrawFov;
			newEsp.showCross = settings.visCross;
			newEsp.bombVisible = false;

			const bool needBoxes = settings.utilDraw && (settings.visBox2D || settings.visBox3D || settings.visHealth || settings.visDistance);
			const bool needVpkAimBones =
				settings.aimEnabled &&
				settings.utilVisibleCheck &&
				settings.utilVpkVisibilityParse;
			const bool needBones = needVpkAimBones || settings.aimEnabled || (settings.utilDraw && (settings.visBones || settings.visVisibleBones));
			const bool need3d = settings.utilDraw && settings.visBox3D;

			if (raw.hasMatrix)
			{
				if (settings.utilDraw && settings.visBombEsp && raw.plantedC4 && raw.bombTicking && !raw.bombDefused)
				{
					Vector bombScreen{};
					if (WorldToScreen(raw.plantedC4Pos, raw.matrix, settings.screen, bombScreen))
					{
						newEsp.bombVisible = true;
						newEsp.bombScreen = bombScreen;
						newEsp.bombSite = raw.bombSite;
						newEsp.bombBeingDefused = raw.bombBeingDefused;
					}
				}

				int projectedCount = 0;
				for (int index = 0; index < static_cast<int>(kMaxPlayers); ++index)
				{
					const RawPlayer& rp = raw.players[index];
					if (!rp.valid)
						continue;

					if (settings.utilTeamCheck && rp.team == raw.local.team)
						continue;

					EspPlayer ep{};
					ep.valid = true;
					ep.origin = rp.origin;
					ep.health = rp.health;
					ep.team = rp.team;
					ep.spotted = rp.spotted;
					ep.dis2LPSqr = rp.dis2LPSqr;
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

							if (settings.utilDraw &&
								settings.visVisibleBones &&
								settings.utilVisibleCheck &&
								settings.utilVpkVisibilityParse &&
								visRuntime &&
								visRuntime->IsMapLoaded())
							{
								float nearestBoneDist = std::numeric_limits<float>::max();
								for (size_t i = 0; i < kBoneCount; ++i)
								{
									const Vector& screenBone = ep.screenBones[i];
									if (screenBone.z <= 0.0f || !InScreen(settings.screen, screenBone.x, screenBone.y))
										continue;

									const float distToCrossSqr = screenBone.CalculateDistanceToPoint2DSqr(newEsp.cross);
									nearestBoneDist = (std::min)(nearestBoneDist, distToCrossSqr);
								}

								if (nearestBoneDist < fovRadiusSqr)
								{
									const Vector localEye = raw.local.origin + raw.local.viewOffset;
									const uint32_t visFrameTag = static_cast<uint32_t>(GetTickCount64() / kVisRayIntervalMs);
									for (size_t i = 0; i < kBoneCount; ++i)
									{
										const Vector& worldBone = rp.worldBones[i];
										if (worldBone.IsZero())
											continue;

										const Vector& screenBone = ep.screenBones[i];
										if (screenBone.z <= 0.0f || !InScreen(settings.screen, screenBone.x, screenBone.y))
											continue;

										const float distToCrossSqr = screenBone.CalculateDistanceToPoint2DSqr(newEsp.cross);
										if (distToCrossSqr >= fovRadiusSqr)
											continue;

										ep.visibleBones[i] = IsBoneVisibleCached(index, static_cast<int>(i), localEye, worldBone, visFrameTag);
										if (ep.visibleBones[i])
											ep.anyVisibleBone = true;
									}
								}
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
					++projectedCount;
				}

				if (projectedCount >= 16)
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}

			newEsp.hasData = true;
			{
				std::unique_lock lock(shared.espMutex);
				shared.esp = newEsp;
			}

			if (settings.helperEnabled)
			{
				GrenadeRenderState grenadeState{};
				BuildGrenadeRenderState(raw, settings, grenadeState);
				PublishGrenadeRenderState(std::move(grenadeState));
			}
			else
			{
				PublishGrenadeRenderState(GrenadeRenderState{});
			}

			const int adaptiveEspSleepMs = (settings.visBones || settings.visBox3D)
				? std::max(1, espSleepMs - 1)
				: espSleepMs;
			std::this_thread::sleep_for(std::chrono::milliseconds(adaptiveEspSleepMs));
		}
	}

	// 自瞄线程：执行目标筛选、平滑移动与扳机逻辑。
	void Game::AimWorker()
	{
		bool triggerPulseDown = false;
		int lastBestIndex = -1;
		AimCurveTracker curveTracker{};
		auto triggerPulseReleaseAt = std::chrono::steady_clock::now();
		auto nextTriggerAllowedAt = std::chrono::steady_clock::now();
		auto lastTriggerDebugTick = std::chrono::steady_clock::time_point{};
		float carryX = 0.0f;
		float carryY = 0.0f;

		while (running.load())
		{
			auto loopStart = std::chrono::high_resolution_clock::now();
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
				bool movedThisFrame = false;
				const bool scopedFovBoost = raw.local.isScoped && IsScopeCapableWeapon(raw.local.activeWeaponDefIndex);
				const float scopedScale = std::clamp(settings.scopedFovScale, 1.0f, 2.5f);
				const float effectiveAimFov = settings.aimbotFOV * (scopedFovBoost ? scopedScale : 1.0f);
				const float effectiveAimFovSqr = effectiveAimFov * effectiveAimFov;
				const float maxAimDistance = settings.aimbotDis * kOneMeterUnits;
				const float maxAimDistanceSqr = maxAimDistance * maxAimDistance;
				const auto resolveCrosshairTargetIndex = [&](int crosshairEnt) -> int
				{
					if (crosshairEnt <= 0)
						return -1;

					auto directPlayerIndexIfValid = [&](int idx) -> int
					{
						if (idx < 0 || idx >= static_cast<int>(kMaxPlayers))
							return -1;
						if (!raw.players[idx].valid || raw.players[idx].pAddr == 0)
							return -1;
						return idx;
					};

					if (crosshairEnt <= static_cast<int>(kMaxPlayers))
					{
						const int byLegacyIndex = directPlayerIndexIfValid(crosshairEnt - 1);
						if (byLegacyIndex >= 0)
							return byLegacyIndex;
					}

					const int slotIndex = (crosshairEnt & 0x1FF) - 1;
					const int bySlot = directPlayerIndexIfValid(slotIndex);
					if (bySlot >= 0)
						return bySlot;

					if (raw.entityList == 0)
						return -1;

					const int entIndex = crosshairEnt & 0x7FFF;
					if (entIndex <= 0)
						return -1;

					const std::uintptr_t listEntry = Read<std::uintptr_t>(raw.entityList + 0x8ull * ((entIndex & 0x7FFF) >> 9) + 16ull);
					if (!listEntry)
						return -1;

					const std::uintptr_t crosshairPawn = Read<std::uintptr_t>(listEntry + 112ull * (entIndex & 0x1FF));
					if (!crosshairPawn)
						return -1;

					for (int i = 0; i < static_cast<int>(kMaxPlayers); ++i)
					{
						if (!raw.players[i].valid)
							continue;
						if (raw.players[i].pAddr == crosshairPawn)
							return i;
					}

					return -1;
				};

				int bestIndex = -1;
				float bestDist = std::numeric_limits<float>::max();

				std::array<int, kMaxPlayers> selectedAimBones{};
				selectedAimBones.fill(std::clamp(settings.aimLocation, 0, static_cast<int>(kBoneCount) - 1));

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
						if (settings.utilVisibleCheck)
						{
							if (settings.utilVpkVisibilityParse)
							{
								if (!visRuntime || !visRuntime->IsMapLoaded())
									continue;
							}
							else if (!rp.spotted)
							{
								continue;
							}
						}
						if (rp.dis2LPSqr > maxAimDistanceSqr)
							continue;

						const int baseAimBone = std::clamp(settings.aimLocation, 0, static_cast<int>(kBoneCount) - 1);
						if (baseAimBone < 0 || baseAimBone >= static_cast<int>(kBoneCount))
							continue;

						const Vector& baseTarget = ep.screenBones[baseAimBone];
						if (baseTarget.z <= 0.0f || !InScreen(settings.screen, baseTarget.x, baseTarget.y))
							continue;

						const float baseDistSqr = baseTarget.CalculateDistanceToPoint2DSqr(cross);
						if (baseDistSqr >= effectiveAimFovSqr)
							continue;

						int selectedAimBone = selectedAimBones[i];
						if (settings.aimSmartBoneSelection && settings.utilVpkVisibilityParse)
						{
							if (!visRuntime || !visRuntime->IsMapLoaded())
								continue;

							selectedAimBone = GetBestVisibleAimBoneIndex(raw, rp, ep, settings, cross, i);
							if (selectedAimBone < 0)
								continue;
						}
						else if (settings.utilVpkVisibilityParse)
						{
							selectedAimBone = GetFirstVisibleBoneIndex(raw, rp, ep, cross, effectiveAimFov, i);
							if (selectedAimBone < 0)
								continue;
						}
						else if (!settings.utilVisibleCheck || !settings.utilVpkVisibilityParse)
						{
							selectedAimBone = std::clamp(settings.aimLocation, 0, static_cast<int>(kBoneCount) - 1);
						}

						selectedAimBones[i] = selectedAimBone;

						const Vector target = ep.screenBones[selectedAimBone];
						if (target.z <= 0.0f || !InScreen(settings.screen, target.x, target.y))
							continue;

						const float distSqr = target.CalculateDistanceToPoint2DSqr(cross);
						if (distSqr < effectiveAimFovSqr && distSqr < bestDist)
						{
							bestDist = distSqr;
							bestIndex = i;
						}
					}
				}

				if (settings.aimEnabled && (GetAsyncKeyState(settings.aimKey) & 0x8000) && bestIndex != -1)
				{
					const EspPlayer& target = esp.players[bestIndex];
					const int selectedAimBone = selectedAimBones[bestIndex];
					const Vector targetPos = target.screenBones[selectedAimBone];
					Vector desiredMove{ 0.0f, 0.0f, 1.0f };

					if (settings.aimRecoil && newAim.hasRecoil)
					{
						desiredMove.x = targetPos.x - newAim.recoilPos.x + static_cast<float>(settings.recoilX);
						desiredMove.y = targetPos.y - newAim.recoilPos.y + static_cast<float>(settings.recoilY);
					}
					else
					{
						desiredMove.x = targetPos.x - cross.x;
						desiredMove.y = targetPos.y - cross.y;
					}

					const int stableAimHz = std::clamp(settings.aimFrequencyHz, 90, 180);
					const double deltaMs = 1000.0 / static_cast<double>(stableAimHz);

					const bool triggerActive = true;
					AimCurveConfig curveConfig{};
					curveConfig.mode = static_cast<AimCurveMode>(std::clamp(settings.aimCurveMode, 0, 3));
					curveConfig.speed = settings.aimCurveSpeed;
					curveConfig.smoothing = settings.aimCurveSmoothing;

					Vector curveDelta = curveTracker.Step(desiredMove, bestIndex, deltaMs, triggerActive, curveConfig);
					curveDelta.x *= std::clamp(settings.aimCurveXSpeedScale, 0.20f, 2.00f);
					curveDelta.y *= std::clamp(settings.aimCurveYSpeedScale, 0.20f, 2.00f);
					curveDelta.x = std::clamp(curveDelta.x, -48.0f, 48.0f);
					curveDelta.y = std::clamp(curveDelta.y, -24.0f, 24.0f);

					float currentMouseX = curveDelta.x;
					float currentMouseY = curveDelta.y;
					carryX += currentMouseX;
					carryY += currentMouseY;

					const int moveXi = static_cast<int>(std::trunc(carryX));
					const int moveYi = static_cast<int>(std::trunc(carryY));
					carryX -= static_cast<float>(moveXi);
					carryY -= static_cast<float>(moveYi);
					int outputX = moveXi;
					int outputY = moveYi;

					if (outputX != 0 || outputY != 0)
					{
						mouseController.MoveRelative(outputX, outputY);
						movedThisFrame = true;
					}
				}
				else
				{
					curveTracker.Reset();
					carryX = 0.0f;
					carryY = 0.0f;
				}

				if (settings.aimTrigger && (GetAsyncKeyState(settings.triggerKey) & 0x8000))
				{
					const int targetIndex = resolveCrosshairTargetIndex(raw.crosshairEnt);
					if (targetIndex >= 0)
					{
						const RawPlayer& target = raw.players[targetIndex];
						if (target.valid && target.lifeState == 256 && target.health > 0 && (!settings.utilTeamCheck || target.team != raw.local.team))
						{
							if (!settings.utilVisibleCheck || settings.utilVpkVisibilityParse || target.spotted)
								triggerShouldHold = true;
						}
					}
					else
					{
						const auto now = std::chrono::steady_clock::now();
						if (now - lastTriggerDebugTick >= std::chrono::milliseconds(300))
						{
							char info[128]{};
							sprintf_s(info, "trigger unresolved target, crosshair id:%d", raw.crosshairEnt);
							Menu::输入调试状态 = info;
							lastTriggerDebugTick = now;
						}
					}
				}

				if (retargetDelayMs > 0)
				{
					if (bestIndex == -1)
					{
						if (lastBestIndex != -1)
							std::this_thread::sleep_for(std::chrono::milliseconds(retargetDelayMs));
					}
					else if (!movedThisFrame && lastBestIndex != -1 && bestIndex != lastBestIndex)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(retargetDelayMs));
					}
				}

				lastBestIndex = bestIndex;

				{
					const auto now = std::chrono::steady_clock::now();
					if (triggerShouldHold && !triggerPulseDown && now >= nextTriggerAllowedAt)
					{
						if (mouseController.LeftDownImmediate())
						{
							triggerPulseDown = true;
							triggerPulseReleaseAt = now + std::chrono::milliseconds(10);
						}
					}

					if (triggerPulseDown)
					{
						const bool shouldReleaseNow = (!triggerShouldHold) || (now >= triggerPulseReleaseAt);
						if (shouldReleaseNow)
						{
							mouseController.LeftUpImmediate();
							triggerPulseDown = false;
							nextTriggerAllowedAt = now + std::chrono::milliseconds(std::clamp(settings.triggerIntervalMs, 30, 400));
						}
					}
				}
			}
			else if (triggerPulseDown)
			{
				curveTracker.Reset();
				carryX = 0.0f;
				carryY = 0.0f;
				mouseController.LeftUpImmediate();
				triggerPulseDown = false;
			}
			if (settings.displayToggle)
			{
				curveTracker.Reset();
				carryX = 0.0f;
				carryY = 0.0f;
				lastBestIndex = -1;
				if (triggerPulseDown)
				{
					mouseController.LeftUpImmediate();
					triggerPulseDown = false;
				}
				nextTriggerAllowedAt = std::chrono::steady_clock::now();
			}

			{
				std::unique_lock lock(shared.aimMutex);
				shared.aim = newAim;
			}

			const auto loopEnd = std::chrono::high_resolution_clock::now();
			const std::chrono::duration<double, std::milli> elapsed = loopEnd - loopStart;
			const int clampedAimHz = std::clamp(settings.aimFrequencyHz, 90, 180);
			const double targetFrameMs = 1000.0 / static_cast<double>(clampedAimHz);
			if (elapsed.count() < targetFrameMs)
				PreciseSleepMs(targetFrameMs - elapsed.count());
		}

		if (triggerPulseDown)
			mouseController.LeftUpImmediate();
	}

	const std::array<Game::SmartBoneCandidate, Game::kSmartBoneCandidateCount>& Game::GetSmartBonePriority() const
	{
		static const std::array<SmartBoneCandidate, kSmartBoneCandidateCount> kPriority = {
			SmartBoneCandidate{ 0 },
			SmartBoneCandidate{ 1 },
			SmartBoneCandidate{ 2 },
			SmartBoneCandidate{ 3 },
			SmartBoneCandidate{ 4 },
			SmartBoneCandidate{ 6 },
			SmartBoneCandidate{ 5 },
			SmartBoneCandidate{ 7 },
			SmartBoneCandidate{ 8 },
			SmartBoneCandidate{ 9 },
			SmartBoneCandidate{ 10 },
			SmartBoneCandidate{ 11 },
			SmartBoneCandidate{ 12 },
			SmartBoneCandidate{ 13 },
			SmartBoneCandidate{ 14 }
		};

		return kPriority;
	}

	bool Game::IsBoneVisibleWithTolerance(const Vector& localEye, const Vector& worldBone) const
	{
		if (!visRuntime)
			return false;

		if (visRuntime->IsPointVisible(localEye, worldBone))
			return true;

		constexpr float kLateralProbe = 6.0f;
		constexpr float kVerticalProbe = 3.0f;
		const Vector probes[] = {
			{ worldBone.x + kLateralProbe, worldBone.y, worldBone.z },
			{ worldBone.x - kLateralProbe, worldBone.y, worldBone.z },
			{ worldBone.x, worldBone.y + kLateralProbe, worldBone.z },
			{ worldBone.x, worldBone.y - kLateralProbe, worldBone.z },
			{ worldBone.x + kLateralProbe * 0.6f, worldBone.y + kLateralProbe * 0.6f, worldBone.z },
			{ worldBone.x - kLateralProbe * 0.6f, worldBone.y - kLateralProbe * 0.6f, worldBone.z },
			{ worldBone.x + kLateralProbe * 0.6f, worldBone.y - kLateralProbe * 0.6f, worldBone.z },
			{ worldBone.x - kLateralProbe * 0.6f, worldBone.y + kLateralProbe * 0.6f, worldBone.z },
			{ worldBone.x, worldBone.y, worldBone.z + kVerticalProbe },
			{ worldBone.x, worldBone.y, worldBone.z - kVerticalProbe }
		};

		for (const Vector& probe : probes)
		{
			if (visRuntime->IsPointVisible(localEye, probe))
				return true;
		}

		return false;
	}

	bool Game::IsBoneVisibleCached(int playerIndex,
		int boneIndex,
		const Vector& localEye,
		const Vector& worldBone,
		uint32_t frameTag) const
	{
		if (playerIndex < 0 || playerIndex >= static_cast<int>(kMaxPlayers))
			return false;
		if (boneIndex < 0 || boneIndex >= static_cast<int>(kBoneCount))
			return false;

		const auto nowMs = static_cast<uint64_t>(GetTickCount64());
		constexpr float kEyeEpsilon = 1.0f;
		constexpr float kBoneEpsilon = 1.0f;

		{
			std::lock_guard<std::mutex> lock(visCacheMutex);
			BoneVisCacheEntry& cache = visBoneCache_[playerIndex][boneIndex];
			if (frameTag != 0 && cache.valid && cache.frameTag == frameTag)
				return cache.visible;

			if (cache.valid)
			{
				const bool notExpired = (nowMs - cache.timestampMs) < kVisRayIntervalMs;
				const bool eyeStable = cache.localEye.CalcDis2Point3DSqr(localEye) <= (kEyeEpsilon * kEyeEpsilon);
				const bool boneStable = cache.worldBone.CalcDis2Point3DSqr(worldBone) <= (kBoneEpsilon * kBoneEpsilon);
				if (notExpired && eyeStable && boneStable)
					return cache.visible;
			}
		}

		const bool visible = IsBoneVisibleWithTolerance(localEye, worldBone);

		{
			std::lock_guard<std::mutex> lock(visCacheMutex);
			BoneVisCacheEntry& cache = visBoneCache_[playerIndex][boneIndex];
			cache.valid = true;
			cache.visible = visible;
			cache.timestampMs = nowMs;
			cache.frameTag = frameTag;
			cache.localEye = localEye;
			cache.worldBone = worldBone;
		}

		return visible;
	}

	int Game::GetFirstVisibleBoneIndex(const RawState& raw,
		const RawPlayer& rp,
		const EspPlayer& ep,
		const Vector& cross,
		float fovRadius,
		int playerIndex) const
	{
		if (!visRuntime || !visRuntime->IsMapLoaded())
			return -1;

		const float clampedFov = (std::max)(0.0f, fovRadius);
		const float clampedFovSqr = clampedFov * clampedFov;
		if (clampedFov <= 0.0f)
			return -1;

		const Vector localEye = raw.local.origin + raw.local.viewOffset;
		const uint32_t visFrameTag = static_cast<uint32_t>(GetTickCount64() / kVisRayIntervalMs);
		for (const SmartBoneCandidate& candidate : GetSmartBonePriority())
		{
			if (candidate.rawBoneIndex >= rp.worldBones.size() ||
				candidate.rawBoneIndex >= ep.screenBones.size())
				continue;

			const Vector& worldBone = rp.worldBones[candidate.rawBoneIndex];
			const Vector& screenBone = ep.screenBones[candidate.rawBoneIndex];
			if (worldBone.IsZero())
				continue;
			if (screenBone.z <= 0.0f)
				continue;

			const float distToCrossSqr = screenBone.CalculateDistanceToPoint2DSqr(cross);
			if (distToCrossSqr >= clampedFovSqr)
				continue;

			if (IsBoneVisibleCached(playerIndex, static_cast<int>(candidate.rawBoneIndex), localEye, worldBone, visFrameTag))
				return static_cast<int>(candidate.rawBoneIndex);
		}

		return -1;
	}

	int Game::GetFirstVisibleBoneIndexFromEsp(const RawPlayer& rp, const EspPlayer& ep) const
	{
		for (const SmartBoneCandidate& candidate : GetSmartBonePriority())
		{
			if (candidate.rawBoneIndex >= rp.worldBones.size() ||
				candidate.rawBoneIndex >= ep.visibleBones.size() ||
				candidate.rawBoneIndex >= ep.screenBones.size())
				continue;

			if (!ep.visibleBones[candidate.rawBoneIndex])
				continue;

			const Vector& worldBone = rp.worldBones[candidate.rawBoneIndex];
			const Vector& screenBone = ep.screenBones[candidate.rawBoneIndex];
			if (worldBone.IsZero() || screenBone.z <= 0.0f)
				continue;

			return static_cast<int>(candidate.rawBoneIndex);
		}

		return -1;
	}

	int Game::GetBestVisibleAimBoneIndex(const RawState& raw,
		const RawPlayer& rp,
		const EspPlayer& ep,
		const SettingsSnapshot& settings,
		const Vector& cross,
		int playerIndex) const
	{
		if (!settings.utilVpkVisibilityParse || !visRuntime)
			return -1;

		int firstVisibleBone = GetFirstVisibleBoneIndexFromEsp(rp, ep);
		if (firstVisibleBone < 0)
			firstVisibleBone = GetFirstVisibleBoneIndex(raw, rp, ep, cross, settings.aimbotFOV, playerIndex);
		if (firstVisibleBone < 0 || firstVisibleBone >= static_cast<int>(ep.screenBones.size()))
			return -1;

		const Vector& screenBone = ep.screenBones[firstVisibleBone];
		if (screenBone.z <= 0.0f || !InScreen(settings.screen, screenBone.x, screenBone.y))
			return -1;

		return firstVisibleBone;
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

		if (settings.utilDraw && settings.visBombEsp)
			DrawBombEsp(esp);

		if (!settings.utilDraw)
			return;

		const ImColor bonesColor(
			settings.colorBones[0],
			settings.colorBones[1],
			settings.colorBones[2],
			settings.colorBones[3]);
		const ImColor visibleBonesColor(
			settings.colorVisibleBones[0],
			settings.colorVisibleBones[1],
			settings.colorVisibleBones[2],
			settings.colorVisibleBones[3]);
		const ImColor esp2dColor(
			settings.colorEsp2D[0],
			settings.colorEsp2D[1],
			settings.colorEsp2D[2],
			settings.colorEsp2D[3]);

		for (const auto& player : esp.players)
		{
			if (!player.valid)
				continue;

			if (settings.utilTeamCheck && esp.localTeam > 0 && player.team == esp.localTeam)
				continue;

				const bool canUseBox = player.hasBox2d;
				const bool canDrawSkeleton = player.headScreen.z > 0.0f && player.originScreen.z > 0.0f;
				bool hasAnyScreenBone = false;
				if (settings.visVisibleBones)
				{
					for (const auto& screenBone : player.screenBones)
					{
						if (screenBone.z > 0.0f)
						{
							hasAnyScreenBone = true;
							break;
						}
					}
				}
				const bool canUse3d = player.has3dBox;

				if ((settings.visBones && canDrawSkeleton) || (settings.visVisibleBones && hasAnyScreenBone))
				{
					DrawBones(
						player,
						esp.screen,
						settings.visBones && canDrawSkeleton,
						bonesColor,
						visibleBonesColor,
						settings.visVisibleBones);
				}
			if (settings.visBox2D && canUseBox)
				DrawEsp2D(player, esp.screen, esp2dColor);
			if (settings.visBox3D && canUse3d)
				Draw3DBox(player);
			if (settings.visHealth && canUseBox)
				DrawHealth(player);
			if (settings.visDistance && canUseBox)
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
	void Game::RenderCrosshairInfo(int crosshairEnt, const SettingsSnapshot& settings) const
	{
		if (crosshairEnt > 0)
		{
			char buff[128];
			sprintf_s(buff, "crosshair id:%d", crosshairEnt);
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

		std::string mapName = cachedMapName;
		if (mapName.empty())
			mapName = NormalizeMapName(ReadCurrentMapName());
		if (!mapName.empty())
		{
			cachedMapName = mapName;
			cachedMapNameAtMs = GetNowMs();
		}
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
			Menu::helper列表请求刷新 = true;
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
		Menu::helper列表请求刷新 = true;
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

	// 教学注释：处理“列表管理页”的请求：刷新与保存。
	// 刷新：把当前 grenadeData 映射到 UI 行数据；
	// 保存：把 UI 行数据写回内存、序列化到 JSON，并立即重载当前地图点位。
	void Game::HandleGrenadeListRequests(const RawState& raw, const SettingsSnapshot& settings)
	{
		if (!settings.helperListRefreshPending && !settings.helperListSavePending)
			return;

		std::string currentMap = cachedMapName;
		if (currentMap.empty() || (GetNowMs() - cachedMapNameAtMs) >= 1000)
		{
			currentMap = NormalizeMapName(ReadCurrentMapName());
			if (!currentMap.empty())
			{
				cachedMapName = currentMap;
				cachedMapNameAtMs = GetNowMs();
			}
		}
		if (currentMap.empty())
			currentMap = NormalizeMapName(Menu::helper地图名);
		if (currentMap.empty() && raw.hasLocal)
			currentMap = cachedMapName;
		if (currentMap.empty())
			currentMap = "de_dust2";

		EnsureGrenadeMapLoaded(currentMap);

		if (settings.helperListRefreshPending)
		{
			GrenadeMapData snapshot{};
			{
				std::scoped_lock grenadeLock(grenadeMutex);
				snapshot = grenadeData;
			}

			Menu::helper列表数据.clear();
			Menu::helper列表数据.reserve(snapshot.spots.size());
			for (const auto& spot : snapshot.spots)
			{
				Menu::GrenadeListRow row{};
				row.id = spot.id;
				row.typeIndex = std::max(0, GrenadeTypeIndexByLabel(spot.type));
				if (spot.throwType == "JumpThrow") row.throwIndex = 1;
				else if (spot.throwType == "RunThrow") row.throwIndex = 2;
				else if (spot.throwType == "RunJumpThrow") row.throwIndex = 3;
				else row.throwIndex = 0;
				strncpy_s(row.name, spot.name.c_str(), _TRUNCATE);
				Menu::helper列表数据.push_back(row);
			}

			Menu::helper列表状态 = u8"列表已刷新";
		}

		if (settings.helperListSavePending)
		{
			bool writeOk = false;
			{
				std::scoped_lock grenadeLock(grenadeMutex);
				std::unordered_map<int, size_t> idToIndex{};
				for (size_t i = 0; i < grenadeData.spots.size(); ++i)
					idToIndex[grenadeData.spots[i].id] = i;

				std::vector<GrenadeSpot> rebuilt{};
				rebuilt.reserve(Menu::helper列表数据.size());
				for (const auto& src : Menu::helper列表数据)
				{
					GrenadeSpot dst{};
					auto it = idToIndex.find(src.id);
					if (it != idToIndex.end())
						dst = grenadeData.spots[it->second];
					dst.id = src.id;
					dst.type = GrenadeTypeLabelByIndex(src.typeIndex);
					switch (src.throwIndex)
					{
					case 1: dst.throwType = "JumpThrow"; break;
					case 2: dst.throwType = "RunThrow"; break;
					case 3: dst.throwType = "RunJumpThrow"; break;
					default: dst.throwType = "StandThrow"; break;
					}
					dst.name = src.name;
					if (dst.name.empty())
						dst.name = "Unnamed";
					rebuilt.push_back(std::move(dst));
				}
				grenadeData.spots = std::move(rebuilt);

			{
				rapidjson::Document doc;
				doc.SetObject();
				auto& allocator = doc.GetAllocator();

				doc.AddMember("map_name", rapidjson::Value(currentMap.c_str(), allocator), allocator);
				rapidjson::Value grenades(rapidjson::kArrayType);
				for (const auto& spot : grenadeData.spots)
				{
					rapidjson::Value node(rapidjson::kObjectType);
					node.AddMember("id", spot.id, allocator);
					node.AddMember("type", rapidjson::Value(spot.type.c_str(), allocator), allocator);
					node.AddMember("name", rapidjson::Value(spot.name.c_str(), allocator), allocator);
					WriteVectorField(node, "position", spot.standPos, allocator);
					WriteVectorField(node, "aim_target", spot.aimPos, allocator);
					node.AddMember("throw_type", rapidjson::Value(spot.throwType.c_str(), allocator), allocator);
					grenades.PushBack(node, allocator);
				}
				doc.AddMember("grenades", grenades, allocator);

				const std::string filePath = std::string("GrenadeData\\") + NormalizeMapName(currentMap) + ".json";
				std::ofstream out(filePath, std::ios::trunc);
				if (out.is_open())
				{
					rapidjson::OStreamWrapper outWrapper(out);
					rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(outWrapper);
					writer.SetIndent(' ', 2);
					doc.Accept(writer);
					writeOk = true;
				}
				else
				{
					Menu::helper列表状态 = u8"列表保存失败：无法写入文件";
				}
			}
			}

			if (writeOk)
			{
				std::string reloadError;
				if (ReloadGrenadeMap(currentMap, reloadError))
					Menu::helper列表状态 = u8"列表已保存并重载当前地图点位";
				else
					Menu::helper列表状态 = u8"保存成功，但重载失败: " + reloadError;
			}
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
			std::string cfgName = TrimAsciiWhitespace(settings.configName);
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
			std::string cfgName = TrimAsciiWhitespace(settings.configName);
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
				WriteLastLoadedConfigName(cfgName);
				Menu::config状态 = std::string(u8"加载成功: ") + cfgName;
			}
			else
			{
				Menu::config状态 = std::string(u8"加载失败: ") + error;
			}
		}
	}

	void Game::BuildGrenadeRenderState(const RawState& raw, const SettingsSnapshot& settings, GrenadeRenderState& out) const
	{
		out = {};

		if (!settings.helperEnabled || !raw.hasLocal || !raw.hasMatrix || settings.screen.x <= 0.0f || settings.screen.y <= 0.0f)
			return;

		GrenadeMapData snapshot{};
		{
			std::scoped_lock grenadeLock(grenadeMutex);
			snapshot = grenadeData;
		}
		if (snapshot.spots.empty())
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
		};

		std::vector<StandDrawItem> standItems{};
		std::vector<AimDrawItem> aimItems{};
		standItems.reserve(snapshot.spots.size());
		aimItems.reserve(snapshot.spots.size());

		const Vector localPos = raw.local.origin;
		const Vector localEye = raw.local.origin + raw.local.viewOffset;
		const float yawRad = raw.local.yaw * (kPi / 180.0f);
		const float pitchRad = raw.local.pitch * (kPi / 180.0f);
		const Vector camForward{
			std::cos(pitchRad) * std::cos(yawRad),
			std::cos(pitchRad) * std::sin(yawRad),
			-std::sin(pitchRad)
		};
		const Vector cross = GetCross(settings.screen);
		const float maxStandDrawDistanceSqr = settings.helperMaxStandDrawDistance * settings.helperMaxStandDrawDistance;
		const float standToleranceSqr = settings.helperStandTolerance * settings.helperStandTolerance;
		const float stickyStandTolerance = settings.helperStandTolerance * 1.35f;
		const float stickyStandToleranceSqr = stickyStandTolerance * stickyStandTolerance;
		const float focusRadiusSqr = settings.helperFocusRadius * settings.helperFocusRadius;

		int previousSelectedSpotId = 0;
		{
			std::shared_lock lock(shared.grenadeMutex);
			previousSelectedSpotId = shared.grenadeFront.selectedSpotId;
		}

		const GrenadeSpot* closest = nullptr;
		float minCrossDist = std::numeric_limits<float>::max();

		for (const auto& spot : snapshot.spots)
		{
			if (settings.helperFilterByWeapon && spot.type != currentWeapon)
				continue;

			const float standDistSqr = localPos.CalcDis2Point3DSqr(spot.standPos);
			if (standDistSqr > maxStandDrawDistanceSqr)
				continue;
			const bool stickyCandidate = previousSelectedSpotId != 0 && previousSelectedSpotId == spot.id;
			const float activeStandToleranceSqr = stickyCandidate ? stickyStandToleranceSqr : standToleranceSqr;
			const bool withinStandTolerance = standDistSqr <= activeStandToleranceSqr;

			const Vector toStand = spot.standPos - localEye;
			const float forwardDot = toStand.x * camForward.x + toStand.y * camForward.y + toStand.z * camForward.z;
			if (!withinStandTolerance && forwardDot < 0.0f)
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

			if (standDistSqr > activeStandToleranceSqr)
				continue;

			Vector aimScreen{};
			if (!WorldToScreen(spot.aimPos, raw.matrix, settings.screen, aimScreen))
				continue;
			if (!InScreen(settings.screen, aimScreen.x, aimScreen.y))
				aimScreen = ClampToScreenEdge(settings.screen, aimScreen);

			const float distToCross = aimScreen.CalculateDistanceToPoint2DSqr(cross);
			aimItems.push_back({ &spot, aimScreen, distToCross });

			if (distToCross < minCrossDist)
			{
				minCrossDist = distToCross;
				closest = &spot;
			}
		}

		if (standItems.empty() && aimItems.empty())
			return;

		const bool focusingByCrosshair = !aimItems.empty() && minCrossDist <= focusRadiusSqr;

		const GrenadeSpot* selectedSpot = closest;

		if (!selectedSpot && !standItems.empty())
			selectedSpot = standItems.front().spot;

		out.valid = true;
		out.cross = cross;
		out.selectedSpotId = selectedSpot ? selectedSpot->id : 0;
		out.standItems.reserve(standItems.size());
		out.aimItems.reserve(aimItems.size());

		if (settings.helperDrawStand)
		{
			for (const auto& item : standItems)
			{
				GrenadeStandRenderItem renderItem{};
				renderItem.screen = item.standScreen;
				renderItem.label = item.spot->name;
				if (!item.spot->throwType.empty())
					renderItem.label += " [" + LocalizeThrowType(item.spot->throwType) + "]";
				out.standItems.push_back(std::move(renderItem));
			}
		}

		const float looseGuideDistanceSqr = settings.helperLooseGuideDistance * settings.helperLooseGuideDistance;
		for (const auto& item : aimItems)
		{
			if (focusingByCrosshair && item.spot != closest)
				continue;

			GrenadeAimRenderItem renderItem{};
			renderItem.screen = item.aimScreen;
			renderItem.isTarget = (selectedSpot && item.spot == selectedSpot);
			renderItem.drawGuide = (focusingByCrosshair && renderItem.isTarget)
				|| (!focusingByCrosshair && renderItem.isTarget && item.distToCross <= looseGuideDistanceSqr);
			renderItem.label = item.spot->name;
			if (!item.spot->throwType.empty())
				renderItem.label += " [" + LocalizeThrowType(item.spot->throwType) + "]";
			out.aimItems.push_back(std::move(renderItem));
		}

		if (selectedSpot)
		{
			std::string throwType = LocalizeThrowType(selectedSpot->throwType.empty() ? "StandThrow" : selectedSpot->throwType);
			out.topText = std::string(u8"投掷方式: ") + throwType;
			if (!selectedSpot->name.empty())
				out.topText += "  |  " + selectedSpot->name;
		}
	}

	void Game::PublishGrenadeRenderState(GrenadeRenderState&& nextState)
	{
		{
			std::unique_lock lock(shared.grenadeMutex);
			shared.grenadeBack = std::move(nextState);
			std::swap(shared.grenadeFront, shared.grenadeBack);
		}
		shared.grenadeRevision.fetch_add(1, std::memory_order_release);
	}

	// 教学注释：返回 Configs 目录下全部 .json 配置名称（不含扩展名）。
	std::vector<std::string> Game::ListConfigs() const
	{
		std::vector<std::string> names{};
		const std::filesystem::path cfgDir = kConfigDir;

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
		const std::string cfgName = NormalizeConfigName(name);
		if (cfgName.empty())
		{
			error = u8"配置名不能为空或包含非法字符";
			return false;
		}

		std::filesystem::create_directories(kConfigDir);
		const std::filesystem::path path = kConfigDir / (cfgName + ".json");

		rapidjson::Document doc;
		doc.SetObject();
		auto& allocator = doc.GetAllocator();

		doc.AddMember("name", rapidjson::Value(cfgName.c_str(), allocator), allocator);

		rapidjson::Value visual(rapidjson::kObjectType);
		visual.AddMember("team_check", Menu::util判断阵营, allocator);
		visual.AddMember("visible_check", Menu::util可视检查, allocator);
		visual.AddMember("vpk_visibility_parse", Menu::utilVPK可视解析, allocator);
		visual.AddMember("draw_master", Menu::util绘制总开关, allocator);
		visual.AddMember("box_2d", Menu::vis方框透视, allocator);
		visual.AddMember("box_3d", Menu::vis3DBox透视, allocator);
		visual.AddMember("bones", Menu::vis绘制骨骼, allocator);
		visual.AddMember("visible_bones", Menu::vis绘制可视骨骼点, allocator);
		visual.AddMember("health", Menu::vis绘制血条, allocator);
		visual.AddMember("distance", Menu::vis绘制距离, allocator);
		visual.AddMember("bomb_esp", Menu::vis绘制C4, allocator);
		visual.AddMember("cross", Menu::vis绘制准心, allocator);

		rapidjson::Value bonesColor(rapidjson::kArrayType);
		rapidjson::Value visibleBonesColor(rapidjson::kArrayType);
		rapidjson::Value esp2dColor(rapidjson::kArrayType);
		for (int i = 0; i < 4; ++i)
		{
			bonesColor.PushBack(Menu::color骨骼[i], allocator);
			visibleBonesColor.PushBack(Menu::color可视骨骼[i], allocator);
			esp2dColor.PushBack(Menu::color2DESP[i], allocator);
		}
		visual.AddMember("bones_color", bonesColor, allocator);
		visual.AddMember("visible_bones_color", visibleBonesColor, allocator);
		visual.AddMember("esp2d_color", esp2dColor, allocator);
		doc.AddMember("visual", visual, allocator);

		rapidjson::Value aim(rapidjson::kObjectType);
		aim.AddMember("draw_fov", Menu::aim绘制FOV, allocator);
		aim.AddMember("enabled", Menu::aim自瞄, allocator);
		aim.AddMember("smart_bone_selection", Menu::aim智能部位选择, allocator);
		aim.AddMember("recoil", Menu::aim后座补偿, allocator);
		aim.AddMember("trigger", Menu::aim扳机, allocator);
		aim.AddMember("fov", Menu::aimbotFOV, allocator);
		aim.AddMember("distance", Menu::aimbotDis, allocator);
		aim.AddMember("location", Menu::AimLocation, allocator);
		aim.AddMember("recoil_x", Menu::recoil_X, allocator);
		aim.AddMember("recoil_y", Menu::recoil_Y, allocator);
		aim.AddMember("aim_key", Menu::aimKey, allocator);
		aim.AddMember("trigger_key", Menu::triggerKey, allocator);
		aim.AddMember("trigger_interval_ms", std::clamp(Menu::扳机间隔毫秒, 30, 400), allocator);
		aim.AddMember("frequency_hz", std::clamp(Menu::瞄准频率Hz, 90, 180), allocator);
		aim.AddMember("curve_mode", std::clamp(Menu::瞄准曲线模式, 0, 3), allocator);
		aim.AddMember("curve_speed", Menu::曲线速度, allocator);
		aim.AddMember("curve_x_speed_scale", std::clamp(Menu::曲线X速度比例, 0.20f, 2.00f), allocator);
		aim.AddMember("curve_y_speed_scale", std::clamp(Menu::曲线Y速度比例, 0.20f, 2.00f), allocator);
		aim.AddMember("curve_smoothing", Menu::曲线平滑, allocator);
		aim.AddMember("scoped_fov_scale", std::clamp(Menu::开镜FOV倍率, 1.00f, 2.50f), allocator);
		aim.AddMember("read_sleep_ms", Menu::read线程休眠毫秒, allocator);
		aim.AddMember("esp_sleep_ms", Menu::esp线程休眠毫秒, allocator);
		aim.AddMember("retarget_delay_ms", Menu::瞄准切换延时毫秒, allocator);
		rapidjson::Value input(rapidjson::kObjectType);
		input.AddMember("method_selected", Menu::输入方式选择, allocator);
		input.AddMember("method_applied", Menu::输入方式当前生效, allocator);
		input.AddMember("endpoint", rapidjson::Value(Menu::输入地址, allocator), allocator);
		input.AddMember("uuid", rapidjson::Value(Menu::输入UUID, allocator), allocator);
		input.AddMember("auto_connect", Menu::输入自动连接, allocator);
		aim.AddMember("input", input, allocator);
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
		helper.AddMember("top_hint_offset_x", Menu::helper顶部提示偏移X, allocator);
		helper.AddMember("top_hint_offset_y", Menu::helper顶部提示偏移Y, allocator);
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
		const std::string cfgName = NormalizeConfigName(name);
		if (cfgName.empty())
		{
			error = u8"配置名不能为空或包含非法字符";
			return false;
		}

		const std::filesystem::path path = kConfigDir / (cfgName + ".json");
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
			Menu::utilVPK可视解析 = true;
			if (visual.HasMember("draw_master") && visual["draw_master"].IsBool()) Menu::util绘制总开关 = visual["draw_master"].GetBool();
			if (visual.HasMember("box_2d") && visual["box_2d"].IsBool()) Menu::vis方框透视 = visual["box_2d"].GetBool();
			if (visual.HasMember("box_3d") && visual["box_3d"].IsBool()) Menu::vis3DBox透视 = visual["box_3d"].GetBool();
			if (visual.HasMember("bones") && visual["bones"].IsBool()) Menu::vis绘制骨骼 = visual["bones"].GetBool();
			if (visual.HasMember("visible_bones") && visual["visible_bones"].IsBool()) Menu::vis绘制可视骨骼点 = visual["visible_bones"].GetBool();
			if (visual.HasMember("health") && visual["health"].IsBool()) Menu::vis绘制血条 = visual["health"].GetBool();
			if (visual.HasMember("distance") && visual["distance"].IsBool()) Menu::vis绘制距离 = visual["distance"].GetBool();
			if (visual.HasMember("bomb_esp") && visual["bomb_esp"].IsBool()) Menu::vis绘制C4 = visual["bomb_esp"].GetBool();
			if (visual.HasMember("cross") && visual["cross"].IsBool()) Menu::vis绘制准心 = visual["cross"].GetBool();

			auto LoadColorArray = [](const rapidjson::Value& src, float dst[4]) {
				if (!src.IsArray() || src.Size() != 4)
					return;
				for (rapidjson::SizeType i = 0; i < 4; ++i)
				{
					if (src[i].IsNumber())
						dst[i] = static_cast<float>(src[i].GetDouble());
				}
			};

			if (visual.HasMember("bones_color")) LoadColorArray(visual["bones_color"], Menu::color骨骼);
			if (visual.HasMember("visible_bones_color")) LoadColorArray(visual["visible_bones_color"], Menu::color可视骨骼);
			if (visual.HasMember("esp2d_color")) LoadColorArray(visual["esp2d_color"], Menu::color2DESP);
		}

		if (doc.HasMember("aim") && doc["aim"].IsObject())
		{
			const auto& aim = doc["aim"];
			if (aim.HasMember("draw_fov") && aim["draw_fov"].IsBool()) Menu::aim绘制FOV = aim["draw_fov"].GetBool();
			if (aim.HasMember("enabled") && aim["enabled"].IsBool()) Menu::aim自瞄 = aim["enabled"].GetBool();
			if (aim.HasMember("smart_bone_selection") && aim["smart_bone_selection"].IsBool()) Menu::aim智能部位选择 = aim["smart_bone_selection"].GetBool();
			if (aim.HasMember("recoil") && aim["recoil"].IsBool()) Menu::aim后座补偿 = aim["recoil"].GetBool();
			if (aim.HasMember("trigger") && aim["trigger"].IsBool()) Menu::aim扳机 = aim["trigger"].GetBool();
			if (aim.HasMember("fov") && aim["fov"].IsNumber()) Menu::aimbotFOV = static_cast<float>(aim["fov"].GetDouble());
			if (aim.HasMember("distance") && aim["distance"].IsInt()) Menu::aimbotDis = aim["distance"].GetInt();
			if (aim.HasMember("location") && aim["location"].IsInt()) Menu::AimLocation = aim["location"].GetInt();
			if (aim.HasMember("recoil_x") && aim["recoil_x"].IsInt()) Menu::recoil_X = aim["recoil_x"].GetInt();
			if (aim.HasMember("recoil_y") && aim["recoil_y"].IsInt()) Menu::recoil_Y = aim["recoil_y"].GetInt();
			if (aim.HasMember("aim_key") && aim["aim_key"].IsInt()) Menu::aimKey = aim["aim_key"].GetInt();
			if (aim.HasMember("trigger_key") && aim["trigger_key"].IsInt()) Menu::triggerKey = aim["trigger_key"].GetInt();
			if (aim.HasMember("trigger_interval_ms") && aim["trigger_interval_ms"].IsInt()) Menu::扳机间隔毫秒 = std::clamp(aim["trigger_interval_ms"].GetInt(), 30, 400);
			if (aim.HasMember("frequency_hz") && aim["frequency_hz"].IsInt()) Menu::瞄准频率Hz = std::clamp(aim["frequency_hz"].GetInt(), 90, 180);
			if (aim.HasMember("curve_mode") && aim["curve_mode"].IsInt()) Menu::瞄准曲线模式 = std::clamp(aim["curve_mode"].GetInt(), 0, 3);
			if (aim.HasMember("curve_speed") && aim["curve_speed"].IsNumber()) Menu::曲线速度 = std::clamp(static_cast<float>(aim["curve_speed"].GetDouble()), 0.4f, 2.5f);
			if (aim.HasMember("curve_x_speed_scale") && aim["curve_x_speed_scale"].IsNumber()) Menu::曲线X速度比例 = std::clamp(static_cast<float>(aim["curve_x_speed_scale"].GetDouble()), 0.20f, 2.00f);
			if (aim.HasMember("curve_y_speed_scale") && aim["curve_y_speed_scale"].IsNumber()) Menu::曲线Y速度比例 = std::clamp(static_cast<float>(aim["curve_y_speed_scale"].GetDouble()), 0.20f, 2.00f);
			if (aim.HasMember("scoped_fov_scale") && aim["scoped_fov_scale"].IsNumber()) Menu::开镜FOV倍率 = std::clamp(static_cast<float>(aim["scoped_fov_scale"].GetDouble()), 1.00f, 2.50f);
			if (aim.HasMember("curve_frequency") && aim["curve_frequency"].IsNumber())
			{
				const float legacyFrequency = std::clamp(static_cast<float>(aim["curve_frequency"].GetDouble()), 0.3f, 3.0f);
				Menu::曲线速度 = std::clamp((Menu::曲线速度 + legacyFrequency * 0.35f), 0.4f, 2.5f);
			}
			if (aim.HasMember("curve_smoothing") && aim["curve_smoothing"].IsNumber()) Menu::曲线平滑 = std::clamp(static_cast<float>(aim["curve_smoothing"].GetDouble()), 0.30f, 0.95f);
			if (aim.HasMember("read_sleep_ms") && aim["read_sleep_ms"].IsInt()) Menu::read线程休眠毫秒 = aim["read_sleep_ms"].GetInt();
			if (aim.HasMember("esp_sleep_ms") && aim["esp_sleep_ms"].IsInt()) Menu::esp线程休眠毫秒 = aim["esp_sleep_ms"].GetInt();
			if (aim.HasMember("retarget_delay_ms") && aim["retarget_delay_ms"].IsInt()) Menu::瞄准切换延时毫秒 = aim["retarget_delay_ms"].GetInt();
			if (aim.HasMember("input") && aim["input"].IsObject())
			{
				const auto& input = aim["input"];
				if (input.HasMember("method_selected") && input["method_selected"].IsInt()) Menu::输入方式选择 = input["method_selected"].GetInt();
				if (input.HasMember("method_applied") && input["method_applied"].IsInt()) Menu::输入方式当前生效 = input["method_applied"].GetInt();
				if (input.HasMember("endpoint") && input["endpoint"].IsString()) strncpy_s(Menu::输入地址, input["endpoint"].GetString(), _TRUNCATE);
				if (input.HasMember("uuid") && input["uuid"].IsString()) strncpy_s(Menu::输入UUID, input["uuid"].GetString(), _TRUNCATE);
				if (input.HasMember("auto_connect") && input["auto_connect"].IsBool()) Menu::输入自动连接 = input["auto_connect"].GetBool();
			}
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
			if (helper.HasMember("top_hint_offset_x") && helper["top_hint_offset_x"].IsNumber()) Menu::helper顶部提示偏移X = static_cast<float>(helper["top_hint_offset_x"].GetDouble());
			if (helper.HasMember("top_hint_offset_y") && helper["top_hint_offset_y"].IsNumber()) Menu::helper顶部提示偏移Y = static_cast<float>(helper["top_hint_offset_y"].GetDouble());
			if (helper.HasMember("map_name") && helper["map_name"].IsString()) strncpy_s(Menu::helper地图名, helper["map_name"].GetString(), _TRUNCATE);
			if (helper.HasMember("note") && helper["note"].IsString()) strncpy_s(Menu::helper备注, helper["note"].GetString(), _TRUNCATE);
		}

		return true;
	}

	void Game::RenderGrenadeHelper(const RawState& raw, const SettingsSnapshot& settings) const
	{
		(void)raw;
		Menu::helper当前瞄准点位ID = 0;

		GrenadeRenderState state{};
		{
			std::shared_lock lock(shared.grenadeMutex);
			state = shared.grenadeFront;
		}
		if (!state.valid)
			return;

		auto* draw = ImGui::GetBackgroundDrawList();
		constexpr float kAimCircleRadius = 10.0f;

		for (const auto& stand : state.standItems)
		{
			draw->AddCircleFilled({ stand.screen.x, stand.screen.y }, 4.0f, ImColor(255, 255, 0));
			if (!stand.label.empty())
				AddTextShadow(draw, { stand.screen.x + 6.0f, stand.screen.y + 8.0f }, ImColor(255, 255, 255), stand.label.c_str());
		}

		for (const auto& aim : state.aimItems)
		{
			const ImColor pointColor = aim.isTarget ? ImColor(255, 80, 80) : ImColor(0, 255, 0);
			draw->AddCircle({ aim.screen.x, aim.screen.y }, kAimCircleRadius, pointColor, 0, 2.0f);

			if (aim.drawGuide)
			{
				draw->AddLine(
					{ state.cross.x, state.cross.y },
					{ aim.screen.x, aim.screen.y },
					aim.isTarget ? ImColor(255, 60, 60) : ImColor(255, 255, 255, 120),
					aim.isTarget ? 2.0f : 1.0f);
			}

			if (!aim.label.empty())
			{
				AddTextShadow(
					draw,
					{ aim.screen.x + 12.0f, aim.screen.y - 16.0f },
					aim.isTarget ? ImColor(255, 245, 190) : ImColor(215, 215, 215),
					aim.label.c_str());
			}
		}

		Menu::helper当前瞄准点位ID = state.selectedSpotId;
		if (!state.topText.empty())
		{
			const float fontSize = 30.0f;
			const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, 2000.0f, 0.0f, state.topText.c_str());
			const ImVec2 textPos{
				(settings.screen.x - textSize.x) * 0.5f + settings.helperTopHintOffsetX,
				28.0f + settings.helperTopHintOffsetY
			};

			AddTextShadow(ImGui::GetBackgroundDrawList(), ImGui::GetFont(), fontSize, textPos, ImColor(255, 220, 120), state.topText.c_str());
		}
	}
}
