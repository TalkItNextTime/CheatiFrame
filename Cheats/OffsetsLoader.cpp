#ifndef _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
#define _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
#endif
#ifndef _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION
#define _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION
#endif

#include "OffsetsLoader.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>

namespace Cheats
{
	namespace
	{
		// 调试输出：将已加载的偏移值同时以十六进制/十进制打印到控制台。
		void PrintLoadedOffset(const char* source, const std::string& name, std::uintptr_t value)
		{
		#ifdef _DEBUG
			std::cout << "[OffsetsLoader][" << source << "] " << name
				<< " = 0x" << std::hex << value
				<< " (" << std::dec << value << ')'
				<< '\n';
		#else
			(void)source;
			(void)name;
			(void)value;
		#endif
		}

		// 读取并解析JSON文件，要求根节点为对象类型。
		bool ParseJsonFile(const std::string& path, rapidjson::Document& doc, std::string& error)
		{
			std::ifstream stream(path);
			if (!stream.is_open())
			{
				error = "Failed to open file: " + path;
				return false;
			}

			rapidjson::IStreamWrapper wrapper(stream);
			doc.ParseStream(wrapper);
			if (doc.HasParseError() || !doc.IsObject())
			{
				error = "JSON parse failed: " + path;
				return false;
			}
			return true;
		}

		// 从对象中读取无符号整数字段（兼容int/uint的JSON数值）。
		bool ReadUintField(const rapidjson::Value& obj, const char* name, std::uintptr_t& out, std::string& error)
		{
			if (!obj.HasMember(name))
			{
				error = std::string("Missing field: ") + name;
				return false;
			}

			const auto& value = obj[name];
			if (!value.IsNumber())
			{
				error = std::string("Field is not numeric: ") + name;
				return false;
			}

			if (value.IsUint64())
				out = static_cast<std::uintptr_t>(value.GetUint64());
			else if (value.IsUint())
				out = static_cast<std::uintptr_t>(value.GetUint());
			else if (value.IsInt64())
				out = static_cast<std::uintptr_t>(value.GetInt64());
			else if (value.IsInt())
				out = static_cast<std::uintptr_t>(value.GetInt());
			else
			{
				error = std::string("Unsupported field type: ") + name;
				return false;
			}

			return true;
		}

		// 读取 client_dll.json 的 classes[className].fields[fieldName] 字段值。
		bool ReadClientField(const rapidjson::Value& classes,
			const char* className,
			const char* fieldName,
			std::uintptr_t& out,
			std::string& error)
		{
			if (!classes.HasMember(className))
			{
				error = std::string("Missing class: ") + className;
				return false;
			}

			const auto& cls = classes[className];
			if (!cls.IsObject() || !cls.HasMember("fields") || !cls["fields"].IsObject())
			{
				error = std::string("Invalid class fields: ") + className;
				return false;
			}

			return ReadUintField(cls["fields"], fieldName, out, error);
		}
	}

	// 统一加载偏移目录中的核心JSON文件并填充 OffsetsData 结构。
	bool LoadOffsetsFromDir(const std::string& offsetsDir, OffsetsData& out, std::string& error)
	{
		const std::string offsetsPath = offsetsDir + "\\offsets.json";
		const std::string buttonsPath = offsetsDir + "\\buttons.json";
		const std::string clientPath = offsetsDir + "\\client_dll.json";

		rapidjson::Document offsetsDoc;
		if (!ParseJsonFile(offsetsPath, offsetsDoc, error))
			return false;

		const auto& offsetsRoot = offsetsDoc["client.dll"];
		if (!offsetsRoot.IsObject())
		{
			error = "offsets.json missing client.dll";
			return false;
		}

		if (!ReadUintField(offsetsRoot, "dwEntityList", out.dwEntityList, error) ||
			!ReadUintField(offsetsRoot, "dwViewMatrix", out.dwViewMatrix, error) ||
			!ReadUintField(offsetsRoot, "dwLocalPlayerPawn", out.dwLocalPlayerPawn, error) ||
			!ReadUintField(offsetsRoot, "dwLocalPlayerController", out.dwLocalPlayerController, error) ||
			!ReadUintField(offsetsRoot, "dwGlobalVars", out.dwGlobalVars, error) ||
			!ReadUintField(offsetsRoot, "dwPlantedC4", out.dwPlantedC4, error))
		{
			error = "offsets.json read failed: " + error;
			return false;
		}

		PrintLoadedOffset("offsets.json", "dwEntityList", out.dwEntityList);
		PrintLoadedOffset("offsets.json", "dwViewMatrix", out.dwViewMatrix);
		PrintLoadedOffset("offsets.json", "dwLocalPlayerPawn", out.dwLocalPlayerPawn);
		PrintLoadedOffset("offsets.json", "dwLocalPlayerController", out.dwLocalPlayerController);
		PrintLoadedOffset("offsets.json", "dwGlobalVars", out.dwGlobalVars);
		PrintLoadedOffset("offsets.json", "dwPlantedC4", out.dwPlantedC4);

		rapidjson::Document buttonsDoc;
		if (!ParseJsonFile(buttonsPath, buttonsDoc, error))
			return false;

		const auto& buttonsRoot = buttonsDoc["client.dll"];
		if (!buttonsRoot.IsObject())
		{
			error = "buttons.json missing client.dll";
			return false;
		}

		if (!ReadUintField(buttonsRoot, "attack", out.attack, error) ||
			!ReadUintField(buttonsRoot, "attack2", out.attack2, error))
		{
			error = "buttons.json read failed: " + error;
			return false;
		}

		PrintLoadedOffset("buttons.json", "attack", out.attack);
		PrintLoadedOffset("buttons.json", "attack2", out.attack2);

		rapidjson::Document clientDoc;
		if (!ParseJsonFile(clientPath, clientDoc, error))
			return false;

		const auto& clientRoot = clientDoc["client.dll"];
		if (!clientRoot.IsObject() || !clientRoot.HasMember("classes") || !clientRoot["classes"].IsObject())
		{
			error = "client_dll.json missing classes";
			return false;
		}

		const auto& classes = clientRoot["classes"];

		struct FieldSpec
		{
			const char* className;
			const char* fieldName;
			std::uintptr_t* target;
		};

		FieldSpec specs[] =
		{
			{ "CCSPlayerController", "m_hPlayerPawn", &out.m_hPlayerPawn },
			{ "C_BaseEntity", "m_iHealth", &out.m_iHealth },
			{ "C_BaseEntity", "m_lifeState", &out.m_lifeState },
			{ "C_BaseEntity", "m_iTeamNum", &out.m_iTeamNum },
			{ "C_BasePlayerPawn", "m_vOldOrigin", &out.m_vOldOrigin },
			{ "C_CSPlayerPawn", "m_entitySpottedState", &out.m_entitySpottedState },
			{ "C_BaseEntity", "m_pGameSceneNode", &out.m_pGameSceneNode },
			{ "CGameSceneNode", "m_vecAbsOrigin", &out.m_vecAbsOrigin },
			{ "CSkeletonInstance", "m_modelState", &out.m_modelState },
			{ "C_CSPlayerPawn", "m_iShotsFired", &out.m_iShotsFired },
			{ "C_CSPlayerPawn", "m_aimPunchAngle", &out.m_aimPunchAngle },
			{ "C_CSPlayerPawn", "m_iIDEntIndex", &out.m_iIDEntIndex },
			{ "C_CSPlayerPawn", "m_bIsScoped", &out.m_bIsScoped },
			{ "C_PlantedC4", "m_bBombTicking", &out.m_bBombTicking },
			{ "C_PlantedC4", "m_bBombDefused", &out.m_bBombDefused },
			{ "C_PlantedC4", "m_bBeingDefused", &out.m_bBeingDefused },
			{ "C_PlantedC4", "m_nBombSite", &out.m_nBombSite },
			{ "C_CSPlayerPawn", "m_angEyeAngles", &out.m_angEyeAngles },
			{ "C_BaseModelEntity", "m_vecViewOffset", &out.m_vecViewOffset },
			{ "C_CSPlayerPawn", "m_pClippingWeapon", &out.m_pClippingWeapon },
			{ "C_BaseEntity", "m_nSubclassID", &out.m_nSubclassID },
			{ "C_EconEntity", "m_AttributeManager", &out.m_AttributeManager },
			{ "C_AttributeContainer", "m_Item", &out.m_Item },
			{ "C_EconItemView", "m_iItemDefinitionIndex", &out.m_iItemDefinitionIndex },
		};

		for (const auto& spec : specs)
		{
			if (!ReadClientField(classes, spec.className, spec.fieldName, *spec.target, error))
			{
				std::ostringstream oss;
				oss << "client_dll.json read failed: " << spec.className << "::" << spec.fieldName << " - " << error;
				error = oss.str();
				return false;
			}

			std::ostringstream key;
			key << spec.className << "::" << spec.fieldName;
			PrintLoadedOffset("client_dll.json", key.str(), *spec.target);
		}

		return true;
	}
}
