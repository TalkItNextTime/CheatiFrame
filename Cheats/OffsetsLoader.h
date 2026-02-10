#pragma once

#include <string>
#include "OffsetsData.h"

namespace Cheats
{
	// 从指定目录读取 offsets/buttons/client_dll 三个JSON并填充偏移结构。
	// 成功返回 true，失败返回 false 并通过 error 输出原因。
	bool LoadOffsetsFromDir(const std::string& offsetsDir, OffsetsData& out, std::string& error);
}
