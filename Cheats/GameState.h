#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <shared_mutex>
#include <string>
#include <vector>

#include "../Math/Vector.h"

namespace Cheats
{
	constexpr size_t kMaxPlayers = 64;
	constexpr size_t kBoneCount = 15;

	struct ScreenSize
	{
		float x = 0.0f;
		float y = 0.0f;
	};

	struct RawPlayer
	{
		bool valid = false;
		std::uintptr_t pAddr = 0;
		std::uintptr_t sceneNode = 0;
		std::uintptr_t boneArr = 0;
		Vector origin{};
		Vector head{};
		int lifeState = 0;
		int team = 0;
		bool spotted = false;
		int health = 0;
		float pitch = 0.0f;
		float yaw = 0.0f;
		Vector viewOffset{};
		bool isScoped = false;
		int activeWeaponDefIndex = 0;
		int activeWeaponSubclass = 0;
		float dis2LPSqr = 0.0f;
		float dis2LP = 0.0f;
		std::array<Vector, kBoneCount> worldBones{};
	};

	struct RawState
	{
		bool hasLocal = false;
		bool hasMatrix = false;
		std::uintptr_t entityList = 0;
		std::uintptr_t plantedC4 = 0;
		Vector plantedC4Pos{};
		bool bombPlantedFlag = false;
		bool bombTicking = false;
		bool bombDefused = false;
		bool bombBeingDefused = false;
		int bombSite = -1;
		float bombTimerLength = 0.0f;
		float bombDefuseLength = 0.0f;
		float bombTimeLeft = 0.0f;
		float bombDefuseTimeLeft = 0.0f;
		float bombDefuseProgress = 0.0f;
		view_matrix_t matrix{};
		RawPlayer local{};
		std::array<RawPlayer, kMaxPlayers> players{};
		int crosshairEnt = -1;
		int shotsFired = 0;
		Vector aimPunch{};
	};

	struct EspPlayer
	{
		bool valid = false;
		Vector origin{};
		Vector originScreen{};
		Vector headScreen{};
		float espWidth = 0.0f;
		Vector esp1{};
		Vector esp2{};
		bool hasBox2d = false;
		std::array<Vector, 4> box3dTop{};
		std::array<Vector, 4> box3dBottom{};
		bool has3dBox = false;
		int health = 0;
		int team = 0;
		bool spotted = false;
		bool anyVisibleBone = false;
		float dis2LPSqr = 0.0f;
		float dis2LP = 0.0f;
		float yaw = 0.0f;
		std::array<Vector, kBoneCount> screenBones{};
		std::array<bool, kBoneCount> visibleBones{};
	};

	struct EspState
	{
		bool hasData = false;
		ScreenSize screen{};
		int localTeam = 0;
		Vector cross{};
		float fovRadius = 0.0f;
		bool showFov = false;
		bool showCross = false;
		bool bombVisible = false;
		bool bombPlanted = false;
		Vector bombScreen{};
		int bombSite = -1;
		bool bombBeingDefused = false;
		float bombTimerLength = 0.0f;
		float bombTimeLeft = 0.0f;
		float bombDefuseTimeLeft = 0.0f;
		float bombDefuseProgress = 0.0f;
		std::array<EspPlayer, kMaxPlayers> players{};
	};

	struct AimState
	{
		Vector recoilPos{};
		bool hasRecoil = false;
	};

	struct GrenadeStandRenderItem
	{
		Vector screen{};
		std::string label{};
	};

	struct GrenadeAimRenderItem
	{
		Vector screen{};
		std::string label{};
		bool isTarget = false;
		bool drawGuide = false;
	};

	struct GrenadeRenderState
	{
		bool valid = false;
		int selectedSpotId = 0;
		Vector cross{};
		std::string topText{};
		std::vector<GrenadeStandRenderItem> standItems{};
		std::vector<GrenadeAimRenderItem> aimItems{};
	};

	struct SettingsSnapshot
	{
		bool displayToggle = false;
		bool utilTeamCheck = true;
		bool utilVisibleCheck = true;
		bool utilVpkVisibilityParse = false;
		bool utilDraw = true;
		bool visBox2D = false;
		bool visBox3D = false;
		bool visBones = false;
		bool visVisibleBones = true;
		bool visHealth = true;
		bool visDistance = false;
		bool visBombEsp = true;
		bool visCross = true;
		std::array<float, 4> colorBones{ 1.0f, 1.0f, 1.0f, 1.0f };
		std::array<float, 4> colorVisibleBones{ 0.2f, 1.0f, 0.2f, 1.0f };
		std::array<float, 4> colorEsp2D{ 1.0f, 0.0f, 0.0f, 1.0f };
		bool aimDrawFov = true;
		bool aimEnabled = true;
		bool aimSmartBoneSelection = false;
		bool aimRecoil = false;
		bool aimTrigger = true;
		float aimbotFOV = 130.0f;
		int aimbotDis = 250;
		int aimLocation = 0;
		int recoilX = 4;
		int recoilY = 20;
		int aimKey = 16;
		int triggerKey = 6;
		int triggerIntervalMs = 95;
		int aimFrequencyHz = 144;
		int aimCurveMode = 0;
		float aimCurveSpeed = 1.0f;
		float aimCurveXSpeedScale = 1.0f;
		float aimCurveYSpeedScale = 0.75f;
		float aimCurveSmoothing = 0.55f;
		float scopedFovScale = 1.25f;
		int readSleepMs = 2;
		int espSleepMs = 4;
		int aimRetargetDelayMs = 120;
		int inputMethodSelected = 0;
		int inputMethodApplied = 0;
		char inputEndpoint[64]{};
		char inputUuid[64]{};
		bool inputConnectRequest = false;
		bool inputAutoConnect = true;
		bool helperEnabled = false;
		bool helperFilterByWeapon = true;
		bool helperDrawStand = true;
		bool helperDrawAim = true;
		bool helperManualTypeOverride = false;
		int helperManualType = 0;
		int helperThrowType = 0;
		float helperStandTolerance = 35.0f;
		float helperFocusRadius = 30.0f;
		float helperMaxStandDrawDistance = 2000.0f;
		float helperLooseGuideDistance = 5000.0f;
		float helperRecordDistance = 10000.0f;
		float helperTopHintOffsetX = 0.0f;
		float helperTopHintOffsetY = 0.0f;
		char helperMapName[64]{};
		char helperNote[128]{};
		bool helperRecordPending = false;
		bool helperSyncGrenadeTypePending = false;
		bool helperListRefreshPending = false;
		bool helperListSavePending = false;
		bool configRefreshPending = false;
		bool configSavePending = false;
		bool configLoadPending = false;
		char configName[64]{};
		int configSelectedIndex = 0;
		ScreenSize screen{};
	};

	struct SharedState
	{
		RawState raw{};
		EspState esp{};
		AimState aim{};
		GrenadeRenderState grenadeFront{};
		GrenadeRenderState grenadeBack{};
		SettingsSnapshot settings{};
		mutable std::shared_mutex rawMutex{};
		std::shared_mutex espMutex{};
		std::shared_mutex aimMutex{};
		mutable std::shared_mutex grenadeMutex{};
		mutable std::shared_mutex settingsMutex{};
		std::atomic<std::uint64_t> grenadeRevision{ 0 };
	};
}
