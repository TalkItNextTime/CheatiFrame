#pragma once


#include <Windows.h>
#include <stdio.h>
#include <chrono> //时间库
#include <TlHelp32.h>
#include <vector>
#include <thread>

#include"../Visuals/Menu.h"
#include"../Visuals/External.h"
#include "../Math/Vector.h"
#include "../Visuals/Menu.h"
#include "Offsets.h"
#include "AlgorAim.h"
//#include "Bones.h"

namespace Cheats
{
	inline uintptr_t entityList;
	inline uintptr_t list_entry1;
	inline uintptr_t playerController;
	inline uint32_t	 playerPawn;
	inline uintptr_t list_entry2;
	inline uintptr_t pCSPlayerPawnPtr;

	//inline uintptr_t aimTargetAddr;


	struct BoneIndex //骨骼索引
	{
		int head = 6, spine = 4, hip = 0;
		int hand_l = 11, lowerarm_l = 9, upperarm_l = 8, spine2 = 4;
		int upperarm_r = 13, lowerarm_r = 14, hand_r = 16;
		int calf_l = 24, thigh_l = 23, hip2 = 0;
		int thigh_r = 26, calf_r = 27;
	};



	class Game
	{
	public:
		HANDLE gamehandle;   //游戏进程句柄
		ptrdiff_t client; //client.dll地址
		view_matrix_t matrix;
		BoneIndex boneindex;
		float tem_distance_to_crosshair = 99999.f;//人物与准心之间的距离
		float distance_to_crosshair = 0.f;
		Vector recoilPos;
		Vector aim_punch;
		int crosshair_ent=-1;

		bool CheatInit(); //初始化
		void CheatLoop(); //遍历

		struct Player   //人物信息结构体
		{
		public:
			
			uintptr_t pAddr; //人物结构体保存地址
			uintptr_t sceneNode;
			uintptr_t boneArr;

			Vector origin;
			Vector head;

			int lifeState; //256存活
			int team;  //警3 匪2
			bool spotted;
			int health;
			float pitch;
			float yaw;

			float espWidth;
			Vector ESP1;
			Vector ESP2;

			float dis2LP;//与本地玩家的距离
			float dis2Cross; //人物与准心的距离

			Vector WorldBoneArr[sizeof(boneindex) / sizeof(int)];							 //人物世界骨骼点数组
			Vector ScreenBoneArr[sizeof(boneindex) / sizeof(int)];							 //人物屏幕骨骼点数组
			//Vector ScreenBone1Arr[sizeof(boneConnections) / sizeof(boneConnections[0])]; //人物屏幕骨骼点数组
			//Vector ScreenBone2Arr[sizeof(boneConnections) / sizeof(boneConnections[0])]; //人物屏幕骨骼点数组

		};
		
		Player pLocal;	//本地玩家信息
		Player player;	//当前遍历的玩家信息

		Player temp_target_info;			//自瞄目标的信息
		Player target_info;

		

	
		
	private:

	uintptr_t bind_modules(DWORD pid, std::string_view mname);
	bool UpdateLocalInfo();
	bool UpdatePlayerInfo(int index);
	bool UpdateMatrix();										//更新矩阵
	bool UpdateBones();											//更新骨骼信息

	bool Calc2DBoxPos();										//计算2D方框屏幕位置
	void DrawESP2D();											//绘制2D方框
	void DrawHealth();											//绘制血条
	void Draw3DBox();											//绘制3D方框
	void DrawDistance();										//绘制距离
	void DrawFov();												//绘制自瞄范围
	void DrawBones();											//绘制骨骼
	void ConnectBones(int begin,int end);						//绘制骨骼连线
	void DrawCross();											//绘制准心

	//自瞄
	void EnterAimQueue();										//进入自瞄队列
	void GetTargetInfo();										//获取目标信息
	void Aimbot();												//写内存自瞄
	void TriggerBot();											//扳机
	int getShots();												//获取开火状态
	Vector getAimPunch();										//获取后座
	void recoilCompensation();									//后座补偿计算
	void recoilMove(float x,float y);							//后座补偿鼠标移动
	int getiIDEntIndex();										//获取准心瞄准信息

	//void algorAim(Vector targetPos, float& currentMousePositionX, float& currentMousePositionY);											//算法自瞄

	bool InScreen(float x, float y);							//判断是否在屏幕内
	Vector GetCross();											//获取准心坐标
	template <typename T>T Read(uintptr_t address);
	template <typename T>T Write(uintptr_t address, T value);

	};

	inline Game gameName ;

	void CheatMain();

	template <typename T>
	T Game::Read(uintptr_t address)
	{
		T buffer{ };
		ReadProcessMemory(this->gamehandle, (void*)address, &buffer, sizeof(T), 0);
		return buffer;
	}

	template <typename T>
	T Game::Write(uintptr_t address, T value)
	{
		WriteProcessMemory(this->gamehandle, (void*)address, &value, sizeof(T), NULL);
		return value;
	}


}








