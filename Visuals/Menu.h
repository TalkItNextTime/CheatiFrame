#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace Menu
{
	struct GrenadeListRow
	{
		int id = 0;
		int typeIndex = 0;
		int throwIndex = 0;
		char name[128]{};
	};

	enum AimLoc
	{
		Head = 0, Chest = 1

	};

	enum InputMethod
	{
		WinAPI = 0,
		KmboxNet = 1,
		KmboxBPro = 2
	};

	enum AimCurveType
	{
		SineArc = 0,
		ExpoDecay = 1,
		SmootherStep = 2,
		RandomPerTrigger = 3
	};



	inline bool DisplayToggle = true; //菜单显示开关
	void ShowMenu();//显示菜单的函数

	inline bool util判断阵营 = true;
	inline bool util可视检查 = true;
	inline bool utilVPK可视解析 = false;
	inline std::string vpk可视状态 = "Map Status: (Disabled)";
	inline bool util绘制总开关 = true;
	inline bool vis方框透视 = false;
	inline bool vis3DBox透视 = true;
	inline bool vis绘制骨骼 = false;
	inline bool vis绘制可视骨骼点 = true;
	inline bool vis绘制血条 = true;
	inline bool vis绘制距离 = false;
	inline bool vis绘制C4 = true;
	inline bool vis绘制准心 = true;
	inline float color骨骼[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	inline float color可视骨骼[4] = { 0.2f, 1.0f, 0.2f, 1.0f };
	inline float color2DESP[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
	inline bool aim绘制FOV = true;
	inline bool aim自瞄 = true;
	inline bool aim智能部位选择 = false;
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
	inline int 扳机间隔毫秒 = 95;
	//inline char* aimHotKey[] = { u8"上侧键", u8"下侧键", u8"左Shift", u8"大小写锁"};
	//inline int DefaultAimHotKey = 0;
	//inline char* TriggleHotKey[] = { u8"上侧键", u8"下侧键", u8"左Shift", u8"大小写锁" };
	//inline int DefaultTriggleHotKey = 3;

	//算法
	inline int 瞄准频率Hz = 144;
	inline int 瞄准曲线模式 = AimCurveType::SineArc;
	inline float 曲线速度 = 1.0f;
	inline float 曲线X速度比例 = 1.0f;
	inline float 曲线Y速度比例 = 0.75f;
	inline float 曲线平滑 = 0.55f;
	inline float 开镜FOV倍率 = 1.25f;
	inline int read线程休眠毫秒 = 2;
	inline int esp线程休眠毫秒 = 4;
	inline int 瞄准切换延时毫秒 = 120;
	inline int 输入方式选择 = InputMethod::WinAPI;
	inline int 输入方式当前生效 = InputMethod::WinAPI;
	inline char 输入地址[64] = "127.0.0.1:6234";
	inline char 输入UUID[64] = "";
	inline bool 输入请求连接测试 = false;
	inline bool 输入自动连接 = true;
	inline bool 输入连接成功 = true;
	inline std::string 输入状态 = u8"当前输入方式: WinAPI";
	inline std::string 输入调试状态 = u8"触发调试: idle";
	inline std::string 输入提示 = "";
	inline bool 输入提示警告 = false;
	inline std::uint64_t 输入提示截止时间Ms = 0;

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
	inline float helper顶部提示偏移X = 0.0f;
	inline float helper顶部提示偏移Y = 0.0f;

	inline char helper地图名[64] = "de_dust2";
	inline char helper备注[128] = "";

	inline bool helper请求记录 = false;
	inline bool helper请求刷新 = false;
	inline bool helper请求同步手雷类型 = false;
	inline bool helper本次菜单已自动同步 = false;
	inline bool helper列表筛选类型[5] = { true, true, true, true, true };
	inline bool helper列表请求刷新 = false;
	inline bool helper列表请求保存 = false;
	inline int helper当前瞄准点位ID = 0;
	inline int helper列表高亮点位ID = 0;
	inline bool helper列表等待本次菜单自动高亮 = false;
	inline bool helper列表高亮待滚动 = false;
	inline std::vector<GrenadeListRow> helper列表数据{};
	inline std::string helper列表状态 = u8"未加载列表";
	inline std::string helper状态 = u8"未加载点位";

	inline char config名称[64] = "default";
	inline int config选择索引 = 0;
	inline std::vector<std::string> config列表{};
	inline bool config请求刷新列表 = false;
	inline bool config请求保存 = false;
	inline bool config请求加载 = false;
	inline std::string config状态 = u8"未加载配置";

}
