#pragma once
#include <string>
namespace offsets
{

	//offsets
	inline constexpr std::uintptr_t dwEntityList = 0x19CFC48;
	inline constexpr std::uintptr_t dwViewMatrix = 0x1A31D30;
	inline constexpr std::uintptr_t dwLocalPlayerPawn = 0x1834B18;


	//client
	inline constexpr std::uintptr_t m_hPlayerPawn = 0x80C;
	inline constexpr std::uintptr_t m_iHealth = 0x344;
	inline constexpr std::uintptr_t m_lifeState = 0x348;
	inline constexpr std::uintptr_t m_iszPlayerName = 0x660;
	inline constexpr std::uintptr_t m_iTeamNum = 0x3E3;
	inline constexpr std::uintptr_t m_vOldOrigin = 0x1324; //坐标
	inline constexpr std::uintptr_t m_entitySpottedState = 0x23B8;
	inline constexpr std::uintptr_t m_pGameSceneNode = 0x328;
	inline constexpr std::uintptr_t m_modelState = 0x170;
	inline constexpr std::uintptr_t m_iShotsFired = 0x23E4; // 是否射击
	inline constexpr std::uintptr_t m_aimPunchAngle = 0x1584; // 后座角度
	inline constexpr std::uintptr_t m_aimPunchCache = 0x15A8; // 后座cache
	inline constexpr std::uintptr_t m_iIDEntIndex = 0x1458; // 准心瞄准信息
	inline constexpr std::uintptr_t attack = 0x182D620;//强制射击
	inline constexpr std::uintptr_t attack2 = 0x182D6B0;//强制射击2
	inline constexpr int m_pitch = 0x1438;
	inline constexpr int m_yaw = 0x143C;

	inline constexpr int aimbot_pitch = 0x1A3BBB0;
	inline constexpr int aimbot_yaw = 0x1A3BBB4;
	
}
