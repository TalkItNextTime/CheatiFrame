#pragma once

#include <string>
#include <vector>

namespace Menu
{
	enum AimLoc
	{
		Head = 0, Chest = 1

	};



	inline bool DisplayToggle = true; //菜单显示开关
	void ShowMenu();//显示菜单的函数

	inline bool util判断阵营 = true;
	inline bool util可视检查 = true;
	inline bool util绘制总开关 = true;
	inline bool vis方框透视 = false;
	inline bool vis3DBox透视 = true;
	inline bool vis绘制骨骼 = false;
	inline bool vis绘制血条 = true;
	inline bool vis绘制距离 = false;
	inline bool vis绘制准心 = true;
	inline bool aim绘制FOV = true;
	inline bool aim自瞄 = true;
	//inline bool aim可视判断 = true;
	inline bool aim后座补偿 = false;
	inline bool aim扳机 = true;
	inline float aimbotFOV = 130.f;
	inline int aimbotDis = 250;
	inline int AimLocation = AimLoc::Head;//自瞄位置

	//后座补偿
	inline int recoil_X = 4;
	inline int recoil_Y = 20;

	//热键
	inline int aimKey = 16;		//默认左Shift
	inline int triggerKey = 6;	//默认上侧键
	//inline char* aimHotKey[] = { u8"上侧键", u8"下侧键", u8"左Shift", u8"大小写锁"};
	//inline int DefaultAimHotKey = 0;
	//inline char* TriggleHotKey[] = { u8"上侧键", u8"下侧键", u8"左Shift", u8"大小写锁" };
	//inline int DefaultTriggleHotKey = 3;

	//算法
	inline float MASS = 18.f;
	inline float SPRING_CONSTANT = 400.0f;
	inline float DAMPING_CONSTANT = 260.0f;
	inline float GRAVITY_CONSTANT = 10.f;

	inline bool helper启用 = false;
	inline bool helper按武器筛选 = true;
	inline bool helper绘制站位 = true;
	inline bool helper绘制瞄点 = true;
	inline bool helper手动类型覆盖 = false;
	inline int helper手动类型 = 0;
	inline int helper投掷方式 = 0;
	inline float helper站位容差 = 35.0f;
	inline float helper聚焦半径 = 80.0f;
	inline float helper站位最远绘制 = 2000.0f;
	inline float helper非聚焦引导线距离 = 200.0f;
	inline float helper记录瞄点距离 = 10000.0f;

	inline char helper地图名[64] = "de_dust2";
	inline char helper备注[128] = "";

	inline bool helper请求记录 = false;
	inline bool helper请求刷新 = false;
	inline bool helper请求同步手雷类型 = false;
	inline bool helper本次菜单已自动同步 = false;
	inline std::string helper状态 = u8"未加载点位";

	inline char config名称[64] = "default";
	inline int config选择索引 = 0;
	inline std::vector<std::string> config列表{};
	inline bool config请求刷新列表 = false;
	inline bool config请求保存 = false;
	inline bool config请求加载 = false;
	inline std::string config状态 = u8"未加载配置";

}
