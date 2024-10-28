#include <iostream>
#include <ctime>
#include <cmath>
#include "../Visuals/Menu.h"


// 定义常量
const float PIXELS_PER_METER = 100.0f;


// 更新鼠标位置的函数
inline void updateMousePosition(float mousePositionX, float mousePositionY,
	float targetX, float targetY,
	float& currentMousePositionX, float& currentMousePositionY) {
	float deltaX = targetX - currentMousePositionX;
	float deltaY = targetY - currentMousePositionY;

	float springForceX = Menu::SPRING_CONSTANT * deltaX;
	float springForceY = Menu::SPRING_CONSTANT * deltaY;

	float damperForceX = -Menu::DAMPING_CONSTANT * currentMousePositionX;
	float damperForceY = -Menu::DAMPING_CONSTANT * currentMousePositionY;

	float gravityForceX = Menu::GRAVITY_CONSTANT * deltaY;
	float gravityForceY = -Menu::GRAVITY_CONSTANT * deltaX;

	currentMousePositionX += (springForceX + damperForceX + gravityForceX) / Menu::MASS * 0.01f;
	currentMousePositionY += (springForceY + damperForceY + gravityForceY) / Menu::MASS * 0.01f;
}