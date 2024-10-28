#pragma once


namespace Menu
{
	enum AimLoc
	{
		Head = 0, Chest = 1

	};

	inline bool switchTarget = true;

	inline bool DisplayToggle = true; //菜单显示开关
	void ShowMenu();//显示菜单的函数

	inline bool util判断阵营 = true;
	inline bool util绘制总开关 = true;
	inline bool vis方框透视 = true;
	inline bool vis3DBox透视 = false;
	inline bool vis绘制骨骼 = true;
	inline bool vis绘制血条 = true;
	inline bool vis绘制距离 = true;
	inline bool aim绘制FOV = true;
	inline bool aim自瞄 = true;
	inline bool aim扳机 = true;
	inline float aimbotFOV = 100.f;
	inline int aimbotDis = 300;
	inline int AimLocation = AimLoc::Head;//自瞄位置

	//算法
	inline float MASS = 20.f;
	inline float SPRING_CONSTANT = 350.0f;
	inline float DAMPING_CONSTANT = 100.0f;
	inline float GRAVITY_CONSTANT = 5.f;

}
