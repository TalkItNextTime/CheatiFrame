#pragma once
#include <string>
namespace offsets
{

	//offsets
	inline constexpr std::uintptr_t dwEntityList = 0x19F2488;
	inline constexpr std::uintptr_t dwViewMatrix = 0x1A54550;
	inline constexpr std::uintptr_t dwLocalPlayerPawn = 0x1855CE8;


	//client
	inline constexpr std::uintptr_t m_hPlayerPawn = 0x80C;
	inline constexpr std::uintptr_t m_iHealth = 0x344;
	inline constexpr std::uintptr_t m_lifeState = 0x348;
	inline constexpr std::uintptr_t m_iszPlayerName = 0x660;
	inline constexpr std::uintptr_t m_iTeamNum = 0x3E3;
	inline constexpr std::uintptr_t m_vOldOrigin = 0x1324; //坐标
	inline constexpr std::uintptr_t m_entitySpottedState = 0x23D0;
	inline constexpr std::uintptr_t m_pGameSceneNode = 0x328;
	inline constexpr std::uintptr_t m_modelState = 0x170;
	inline constexpr std::uintptr_t m_iShotsFired = 0x23FC; // 是否射击
	inline constexpr std::uintptr_t m_aimPunchAngle = 0x1584; // 后座角度
	inline constexpr std::uintptr_t m_aimPunchCache = 0x1590; // 后座cache
	inline constexpr std::uintptr_t m_iIDEntIndex = 0x1458; // 准心瞄准信息
	inline constexpr int aimbot_pitch = 0x1A5E650;//dwViewAngles 
	inline constexpr int aimbot_yaw = 0x1A5E654;//dwViewAngles + 4
	//buttons
	inline constexpr std::uintptr_t attack = 0x184E4D0;//强制射击
	inline constexpr std::uintptr_t attack2 = 0x184E560;//强制射击2

	inline constexpr int m_pitch = 0x1438;
	inline constexpr int m_yaw = 0x143C;

	
}
