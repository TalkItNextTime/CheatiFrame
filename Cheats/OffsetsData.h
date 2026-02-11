#pragma once

#include <cstdint>

namespace Cheats
{
	struct OffsetsData
	{
		std::uintptr_t dwEntityList{};
		std::uintptr_t dwViewMatrix{};
		std::uintptr_t dwLocalPlayerPawn{};
		std::uintptr_t dwLocalPlayerController{};
		std::uintptr_t dwGlobalVars{};
		std::uintptr_t dwPlantedC4{};

		std::uintptr_t attack{};
		std::uintptr_t attack2{};

		std::uintptr_t m_hPlayerPawn{};
		std::uintptr_t m_iHealth{};
		std::uintptr_t m_lifeState{};
		std::uintptr_t m_iTeamNum{};
		std::uintptr_t m_vOldOrigin{};
		std::uintptr_t m_entitySpottedState{};
		std::uintptr_t m_pGameSceneNode{};
		std::uintptr_t m_vecAbsOrigin{};
		std::uintptr_t m_modelState{};
		std::uintptr_t m_iShotsFired{};
		std::uintptr_t m_aimPunchAngle{};
		std::uintptr_t m_iIDEntIndex{};
		std::uintptr_t m_bIsScoped{};
		std::uintptr_t m_bBombTicking{};
		std::uintptr_t m_bBombDefused{};
		std::uintptr_t m_bBeingDefused{};
		std::uintptr_t m_nBombSite{};
		std::uintptr_t m_angEyeAngles{};
		std::uintptr_t m_vecViewOffset{};
		std::uintptr_t m_pClippingWeapon{};
		std::uintptr_t m_nSubclassID{};
		std::uintptr_t m_AttributeManager{};
		std::uintptr_t m_Item{};
		std::uintptr_t m_iItemDefinitionIndex{};
	};
}
