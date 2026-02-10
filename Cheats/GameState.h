#pragma once

#include <array>
#include <cstdint>
#include <shared_mutex>

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
		int activeWeaponDefIndex = 0;
		int activeWeaponSubclass = 0;
		float dis2LP = 0.0f;
		std::array<Vector, kBoneCount> worldBones{};
	};

	struct RawState
	{
		bool hasLocal = false;
		bool hasMatrix = false;
		std::uintptr_t entityList = 0;
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
		float dis2LP = 0.0f;
		float yaw = 0.0f;
		std::array<Vector, kBoneCount> screenBones{};
	};

	struct EspState
	{
		bool hasData = false;
		ScreenSize screen{};
		Vector cross{};
		float fovRadius = 0.0f;
		bool showFov = false;
		bool showCross = false;
		std::array<EspPlayer, kMaxPlayers> players{};
	};

	struct AimState
	{
		Vector recoilPos{};
		bool hasRecoil = false;
	};

	struct SettingsSnapshot
	{
		bool displayToggle = false;
		bool utilTeamCheck = true;
		bool utilVisibleCheck = true;
		bool utilDraw = true;
		bool visBox2D = false;
		bool visBox3D = false;
		bool visBones = false;
		bool visHealth = true;
		bool visDistance = false;
		bool visCross = true;
		bool aimDrawFov = true;
		bool aimEnabled = true;
		bool aimRecoil = false;
		bool aimTrigger = true;
		float aimbotFOV = 130.0f;
		int aimbotDis = 250;
		int aimLocation = 0;
		int recoilX = 4;
		int recoilY = 20;
		int aimKey = 16;
		int triggerKey = 6;
		float mass = 18.0f;
		float spring = 400.0f;
		float damping = 260.0f;
		float gravity = 10.0f;
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
		char helperMapName[64]{};
		char helperNote[128]{};
		bool helperRecordPending = false;
		bool helperSyncGrenadeTypePending = false;
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
		SettingsSnapshot settings{};
		std::shared_mutex rawMutex{};
		std::shared_mutex espMutex{};
		std::shared_mutex aimMutex{};
		mutable std::shared_mutex settingsMutex{};
	};
}
