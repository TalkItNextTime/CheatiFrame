#include <iostream>
#include <ctime>
#include <cmath>
#include "../Visuals/Menu.h"
#include <windows.h>

// 定义常量
const float PIXELS_PER_METER = 100.0f;


// 弹簧阻尼算法：根据目标点计算鼠标平滑位移，
// 用于模拟更自然的准星跟随效果（学习用途）。
inline void SpringAlgo(float mousePositionX, float mousePositionY,
	float targetX, float targetY,
	float& currentMousePositionX, float& currentMousePositionY,
	float spring_constant,float damping_constant,
	float gravity_constant,float mass
	) {
	float deltaX = targetX - currentMousePositionX;
	float deltaY = targetY - currentMousePositionY;

	float springForceX = spring_constant * deltaX;
	float springForceY = spring_constant * deltaY;

	float damperForceX = -damping_constant * currentMousePositionX;
	float damperForceY = -damping_constant * currentMousePositionY;

	float gravityForceX = gravity_constant * deltaY;
	float gravityForceY = -gravity_constant * deltaX;

	currentMousePositionX += (springForceX + damperForceX + gravityForceX) / mass * 0.01f;
	currentMousePositionY += (springForceY + damperForceY + gravityForceY) / mass * 0.01f;
}

