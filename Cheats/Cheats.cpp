#include "Cheats.h"

#define PI 3.14159265358979323846



	void Cheats::CheatMain()
	{
		//获取当下时间
		static std::chrono::time_point time1 = std::chrono::steady_clock::now();
		auto time2 = std::chrono::steady_clock::now();

		
		//time2 - time1 >= std::chrono::milliseconds(500)

		if (GetAsyncKeyState(VK_INSERT) && time2 - time1 >= std::chrono::milliseconds(200))//菜单呼出热键
		{
			Menu::DisplayToggle = !Menu::DisplayToggle;
			time1 = time2;

		}

		//显示菜单
		if (Menu::DisplayToggle)
		{
			Menu::ShowMenu();
		}

		if (GetAsyncKeyState(VK_END) & 0x8000)
		{
			exit(0);
		}

		//功能函数调用
		//初始化一次
		static bool b = true;
		if (b)
		{
			if (!Cheats::gameName.CheatInit())
				return;
			b = !b;
		}

		 

		Cheats::gameName.CheatLoop();

	}

	bool Cheats::Game::CheatInit()
	{
		gamehandle = OpenProcess(PROCESS_ALL_ACCESS, false, Visual::external.gamewindow.pid);
		if (gamehandle == NULL)
		{
			printf("打开游戏进程句柄失败，尝试管理员运行！\r\n");
			return false;
		}

		//获取模块地址
		client = bind_modules(Visual::external.gamewindow.pid, "client.dll");
		if (client == NULL)
		{
			CloseHandle(gamehandle);
			return false;
		}

		//初始化entityList
		entityList = Read<uintptr_t>(client + offsets::dwEntityList);
		if (entityList == NULL)
		{
			printf("没读到entityList\r\n");
			return false;
		}




		return true;
	}

	void Cheats::Game::CheatLoop() //遍历信息
	{
		static std::chrono::time_point time1 = std::chrono::steady_clock::now();
		auto time2 = std::chrono::steady_clock::now();
		//遍历所有人
		
		for (int i = 0; i < 64; i++)
		{
			if (!UpdateLocalInfo())										//更新本地玩家信息
				return;

			if (!UpdateMatrix())										//更新矩阵信息
				return;

			if (!UpdatePlayerInfo(i))									//更新其他玩家信息
				continue;												//当前玩家信息不存在就跳过

			if (!Calc2DBoxPos())										//计算2D方框大小
				continue;

			if (!UpdateBones())											//更新骨骼坐标
				continue;

			EnterAimQueue();											//进入自瞄队列
			

			if (Menu::util绘制总开关 && Menu::vis绘制骨骼)				//绘制骨骼
				DrawBones();

			if (Menu::util绘制总开关 && Menu::vis方框透视)				//绘制2D方框
				DrawESP2D();

			if (Menu::util绘制总开关 && Menu::vis3DBox透视)				//绘制3D方框
				Draw3DBox();

			if (Menu::util绘制总开关 && Menu::vis绘制血条)				//绘制血条
				DrawHealth();

			if (Menu::util绘制总开关 && Menu::vis绘制距离)				//绘制距离
				DrawDistance();


				
			

		}
		crosshair_ent = getiIDEntIndex();
		//printf("switchTarget: %d \r\n", Menu::switchTarget);
		GetTargetInfo();

		//printf("aim_punch: %f  %f\r\n", aim_punch.x, aim_punch.y);
		//printf("recoilPos: %f  %f\r\n", recoilPos.x, recoilPos.y);

		recoilCompensation();
		if (Menu::util绘制总开关 && Menu::aim绘制FOV)					//绘制FOV
			DrawFov();
		if (Menu::aim扳机 && GetAsyncKeyState(Menu::triggerKey) && !Menu::DisplayToggle)//扳机
			TriggerBot();
									
		if (Menu::aim自瞄 && GetAsyncKeyState(Menu::aimKey) & 0x8000 && target_info.dis2Cross <= Menu::aimbotFOV && !Menu::DisplayToggle)
			Aimbot();													//自瞄
		if (Menu::vis绘制准心)
			DrawCross();												//准心
		if (crosshair_ent && crosshair_ent != -1)
		{
			char buff[256];
			sprintf_s(buff, u8"准心id:%d 可以射击", crosshair_ent);
			ImGui::GetBackgroundDrawList()->AddText({ Visual::external.gamewindow.size.x / 2 - 120,150 }, ImColor(255, 0, 0), buff);
			ImGui::GetBackgroundDrawList()->AddCircleFilled({ Visual::external.gamewindow.size.x / 2,200 }, 10, ImColor(255, 0, 0));
		}
		//if (GetAsyncKeyState(VK_LBUTTON)) 
		//{
		//	Vector aim_punch = getAimPunch();
		//	printf("aim_punch: %f  %f\r\n", aim_punch.x, aim_punch.y);
		//}
		//if (Menu::aim后座补偿 && GetAsyncKeyState(VK_LBUTTON) && getShots())
		//{
		//	recoilMove(recoilPos.x, recoilPos.y);						//补偿压枪
		//}
	}











	uintptr_t Cheats::Game::bind_modules(DWORD pid, std::string_view mname)
	{
		HANDLE _handle = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);//遍历句柄
		MODULEENTRY32 mod;
		mod.dwSize = sizeof mod;
		for (Module32First(_handle, &mod); Module32Next(_handle, &mod);)//遍历模块
		{
			if (mname.compare(mod.szModule) == 0)//对比模块名是否符合目标模块名
			{
				
				printf("%s Address = %llx \r\n", mname.data(),mod.modBaseAddr);
				return (uintptr_t)mod.modBaseAddr;
			}
		}
		if (mod.modBaseAddr == 0x0)
		{
			printf("没找到%s模块,请检查模块名是否正确\r\n", mname.data());
			throw std::exception("模块基址为空！");
		}
	}

	bool Cheats::Game::UpdateLocalInfo()
	{
		//读自身基址
		pLocal.pAddr = Read<uintptr_t>(client + offsets::dwLocalPlayerPawn);
		if (pLocal.pAddr == NULL)
		{
			printf("未能读取到自身基址\r\n");
			return false;
		}
		//printf("自身基址：%llx\r\n", pLocal.pAddr);
		//坐标
		pLocal.origin = Read<Vector>(pLocal.pAddr + offsets::m_vOldOrigin);
		//printf("自身坐标x：%f\r\n", pLocal.origin.x);
		//printf("自身坐标y：%f\r\n", pLocal.origin.y);
		//printf("自身坐标z：%f\r\n", pLocal.origin.z);
		//队伍
		pLocal.team = Read<int>(pLocal.pAddr + offsets::m_iTeamNum);

		return true;
	}

	bool Cheats::Game::UpdatePlayerInfo(int index)
	{
		

		//读敌人基址
		list_entry1 = Read<uintptr_t>(entityList + (8 * (index & 0x7FFF) >> 9) + 16);
		if (!list_entry1)
			return false;
		playerController = Read<uintptr_t>(list_entry1 + 120 * (index & 0x1FF));
		if (!playerController)
			return false;
		playerPawn = Read<uint32_t>(playerController + offsets::m_hPlayerPawn);
		if (!playerPawn)
			return false;
		list_entry2 = Read<uintptr_t>(entityList + 0x8 * ((playerPawn & 0x7FFF) >> 9) + 16);
		if (!list_entry2)
			return false;
		pCSPlayerPawnPtr = Read<uintptr_t>(list_entry2 + 120 * (playerPawn & 0x1FF));
		if (!pCSPlayerPawnPtr)
			return false;
		player.pAddr = pCSPlayerPawnPtr;
		//printf("敌人基址：%llx\r\n", player.pAddr);
		if (player.pAddr == NULL || player.pAddr == pLocal.pAddr)
			return false;;												//读到自己的信息略过

		//队伍
		player.team = Read<int>(player.pAddr + offsets::m_iTeamNum);
		if (player.team == NULL)
		{
			printf("没读到该角色team\r\n");
			return false;
		}
		if (Menu::util判断阵营 && player.team == pLocal.team)
		{
			return false;
		}

		//存活状态
		player.lifeState = Read<int>(player.pAddr + offsets::m_lifeState);
		if (player.lifeState != 256)
			return false;

		//可见性
		if (Menu::util可视检查)
		{
			player.spotted = Read<bool>(player.pAddr + offsets::m_entitySpottedState + 0x08);
		}
		else
		{
			player.spotted = true;
		}

		//血量
		player.health = Read<int>(player.pAddr + offsets::m_iHealth);

		//坐标
		player.origin = Read<Vector>(player.pAddr + offsets::m_vOldOrigin);
		if (player.origin.x == NULL || player.origin.y == NULL || player.origin.z == NULL)
		{
			printf("没读到角色坐标\r\n");
			return false;
		}

		//俯仰角
		player.pitch = Read<float>(player.pAddr + offsets::m_pitch);
		//偏航角
		player.yaw = Read<float>(player.pAddr + offsets::m_yaw);

		//printf("角色俯仰角：%f\r\n", player.pitch);
		//printf("角色偏航角：%f\r\n", player.yaw);

		//计算与本地玩家的距离
		player.dis2LP = pLocal.origin.CalcDis2Point3D(player.origin);


		return true;
	}
	
	bool Cheats::Game::UpdateMatrix()
	{
		matrix = Read<view_matrix_t>(client + offsets::dwViewMatrix);
		if (matrix[0][0] == NULL)
		{
			printf("矩阵读取失败\r\n");
			return false;
		}

		return true;
	}

	bool Cheats::Game::UpdateBones()
	{
		player.sceneNode = Read<uintptr_t>(player.pAddr + offsets::m_pGameSceneNode);
		player.boneArr = Read<uintptr_t>(player.sceneNode + offsets::m_modelState + 0x80);  //骨骼数组
		player.head = Read<Vector>(player.boneArr + boneindex.head * 32); //head world origin
		//printf("x:%f  y:%f  z:%f \r\n", player.head.x, player.head.y, player.head.z);
		ZeroMemory(&player.WorldBoneArr, sizeof(player.WorldBoneArr));
		ZeroMemory(&player.ScreenBoneArr, sizeof(player.ScreenBoneArr));
		//ZeroMemory(&player.ScreenBone2Arr, sizeof(player.ScreenBone2Arr));

		for (int i = 0; i < sizeof(BoneIndex) / sizeof(int); i++)
		{
			int number = *((int*)&boneindex + i);
			//printf("%d\r\n", number);
			player.WorldBoneArr[i] = Read<Vector>(player.boneArr + number * 32);

			if (abs(player.WorldBoneArr[i].x - 0) <= 10.f || abs(player.WorldBoneArr[i].y - 0) <= 10.f || abs(player.WorldBoneArr[i].z - 0) <= 10.f)
				continue;
			//转化为屏幕坐标并保存
			player.ScreenBoneArr[i] = player.WorldBoneArr[i].world2screen(matrix);
			//printf("BoneNumber:%d  x:%f  y:%f\r\n", number,player.ScreenBoneArr[i].x, player.ScreenBoneArr[i].y);
			/*char buff[256];
			sprintf_s(buff, "% d", number);
			ImGui::GetBackgroundDrawList()->AddText({ player.ScreenBoneArr[i].x,player.ScreenBoneArr[i].y }, ImColor(255, 255, 255), buff);*/

		}

		//for (int i = 0; i < sizeof(boneConnections) / sizeof(boneConnections[0]); i++)
		//{
		//	int bone1 = boneConnections[i].bone1;
		//	int bone2 = boneConnections[i].bone2;

		//	Vector VectorBone1 = Read<Vector>(player.boneArr + bone1 * 32);
		//	Vector VectorBone2 = Read<Vector>(player.boneArr + bone2 * 32);

		//	player.ScreenBone1Arr[i] = VectorBone1.world2screen(matrix);
		//	player.ScreenBone2Arr[i] = VectorBone2.world2screen(matrix);

		//}
		return true;
	}

	bool Cheats::Game::Calc2DBoxPos()
	{
		Vector head_temp;
		Vector origin_screen = player.origin.world2screen(matrix);
		head_temp = player.origin;
		head_temp.z += 68.f;

		Vector head_screen = head_temp.world2screen(matrix);
		//head_temp.x = origin_temp.x;
		//head_temp.y = origin_temp.y + 72.f;

		player.espWidth = (origin_screen.y - head_screen.y) / 4.f;

		player.ESP1.x = head_screen.x - player.espWidth;
		player.ESP1.y = head_screen.y;

		player.ESP2.x = origin_screen.x + player.espWidth;
		player.ESP2.y = origin_screen.y;

		return true;
	}

	void Cheats::Game::DrawESP2D()
	{
		if (!InScreen((player.ESP1.x + player.ESP2.x) / 2, (player.ESP1.y + player.ESP2.y) / 2)) //当人物2D方框的中心不在屏幕内时则不画框
			return;

		ImGui::GetBackgroundDrawList()->AddRect({ player.ESP1.x ,player.ESP1.y }, { player.ESP2.x, player.ESP2.y }, ImColor(255, 0, 0));

	}

	void Cheats::Game::DrawHealth()
	{
		ImColor healthColor = ImColor(0, 255, 0);

		if (player.health < 60)
			healthColor = ImColor(255, 255, 0);
		if (player.health < 30)
			healthColor = ImColor(255, 0, 0);

		float height = (player.health / 100.f) * (player.ESP1.y - player.ESP2.y);
		ImGui::GetBackgroundDrawList()->AddRect({ player.ESP1.x - 7,player.ESP1.y }, { player.ESP1.x - 2,player.ESP2.y }, ImColor(0, 0, 0));
		ImGui::GetBackgroundDrawList()->AddRectFilled({ player.ESP1.x - 3,player.ESP2.y + height }, { player.ESP1.x - 6,player.ESP2.y }, healthColor);

	}

	void Cheats::Game::Draw3DBox()
	{

		Vector TopPos3DArray[4], BottomPos3DArray[4];  //4个顶部世界坐标和4个底部世界坐标
		Vector TopPos2DArray[4], BottomPos2DArray[4];  //4个顶部屏幕坐标和4个底部屏幕坐标
		ImColor color = ImColor(255, 0, 0);
		ImColor frontcolor = ImColor(255, 255, 0);		//朝向那面绘制连线的颜色

		float head_z = player.origin.z + 68.f;//头部z


		if (!InScreen((player.ESP1.x + player.ESP2.x)/2.f, (player.ESP1.y+ player.ESP2.y)/2.f))
			return;

		for (int i = 0; i < 4; i++)
		{
			//以人物视角偏移+45度
			int offset = 45 + i * 90;

			// 获取8个点分别是：包围人物的最下面4个点和最上面4个点分别在人物视角的45 135 225 315度一次for循环获取2个点
			//因为cos（α弧度）= x / 斜边  所以x = cos（α弧度）*斜边  同理y = sin（α弧度） * 斜边  这单斜边长度自定义为55
			//cos（）计算出的值就是单位向量1对应x的值  同理sin（）计算出的值就是单位向量1的对应y的值 *55就是模拟当前角度向量值为55的x，y坐标
			BottomPos3DArray[i].x = player.origin.x + cosf((player.yaw + offset) * PI / 180.f) * 25.f;
			BottomPos3DArray[i].y = player.origin.y + sinf((player.yaw + offset) * PI / 180.f) * 25.f;
			BottomPos3DArray[i].z = player.origin.z;

			TopPos3DArray[i] = BottomPos3DArray[i];
			TopPos3DArray[i].z = head_z;

			//float tempworldBottom[3] = { BottomPos3DArray[i].x,BottomPos3DArray[i].y,BottomPos3DArray[i].z };
			//float tempscreenBottom[2] = { BottomPos2DArray[i].x,BottomPos2DArray[i].y };
			//float tempworldTop[3] = { TopPos3DArray[i].x, TopPos3DArray[i].y, TopPos3DArray[i].z };
			//float tempscreenTop[2]{ TopPos2DArray[i].x, TopPos2DArray[i].y };



			//将8个世界坐标转换为屏幕坐标
			BottomPos2DArray[i] = BottomPos3DArray[i].world2screen(matrix);
			TopPos2DArray[i] = TopPos3DArray[i].world2screen(matrix);
			//if (!WorldToScreen(tempworldBottom, tempscreenBottom) || !WorldToScreen(tempworldTop, tempscreenTop))
			//	break;

			//BottomPos2DArray[i].x = tempscreenBottom[0];
			//BottomPos2DArray[i].y = tempscreenBottom[1];
			//TopPos2DArray[i].x = tempscreenTop[0];
			//TopPos2DArray[i].y = tempscreenTop[1];

			//点1：脚旁边的点坐标 点2：对应着脚旁边的坐标的头部旁边的坐标 绘制竖线
			if (i == 0 || i == 3)
			{
				ImGui::GetBackgroundDrawList()->AddLine({ BottomPos2DArray[i].x,BottomPos2DArray[i].y }, { TopPos2DArray[i].x,TopPos2DArray[i].y }, frontcolor, 1.2f);
			}
			else
			{
				ImGui::GetBackgroundDrawList()->AddLine({ BottomPos2DArray[i].x,BottomPos2DArray[i].y }, { TopPos2DArray[i].x,TopPos2DArray[i].y }, color);
			}
		
			if (i)
			{

				//点1：旧的x点 点2：多90度同平面的点  绘制脚旁边的连线
					ImGui::GetBackgroundDrawList()->AddLine({ BottomPos2DArray[i - 1].x,BottomPos2DArray[i - 1].y }, { BottomPos2DArray[i].x,BottomPos2DArray[i].y }, color);

				//同理绘制头部平面的连线
					ImGui::GetBackgroundDrawList()->AddLine({ TopPos2DArray[i - 1].x,TopPos2DArray[i - 1].y }, { TopPos2DArray[i].x,TopPos2DArray[i].y }, color);

				//如果是最后一次绘制 就把315度和最初的45度的点连起来形成矩形，头部位置同理
				if (i == 3)
				{
					ImGui::GetBackgroundDrawList()->AddLine({ BottomPos2DArray[0].x,BottomPos2DArray[0].y }, { BottomPos2DArray[i].x,BottomPos2DArray[i].y }, frontcolor, 1.2f);
					ImGui::GetBackgroundDrawList()->AddLine({ TopPos2DArray[0].x,TopPos2DArray[0].y }, { TopPos2DArray[i].x,TopPos2DArray[i].y }, frontcolor, 1.2f);
				}
			}
			
		}

	}

	void Cheats::Game::DrawDistance()
	{
		char buff[256];
		sprintf_s(buff, "%.f m", player.dis2LP / 75.f);
		if (InScreen((player.ESP1.x + player.ESP2.x) / 2.05f, player.ESP2.y))
			ImGui::GetBackgroundDrawList()->AddText({ (player.ESP1.x + player.ESP2.x) / 2.05f,player.ESP2.y }, ImColor(255, 255, 255), buff);
	}

	void Cheats::Game::DrawFov()
	{
		float mid_x = Visual::external.gamewindow.size.x * 0.5f;
		float mid_y = Visual::external.gamewindow.size.y * 0.5f;
		ImGui::GetBackgroundDrawList()->AddCircle({ mid_x,mid_y }, Menu::aimbotFOV, ImColor(255, 255, 255), 18);

	}

	void Cheats::Game::DrawBones()
	{
		ConnectBones(0, 2);
		ConnectBones(3, 9);
		ConnectBones(10, 14);

		Vector screenHead = player.head.world2screen(matrix);
		Vector screenPos = player.origin.world2screen(matrix);
		float headHeight = (screenPos.y - screenHead.y) / 8.f;

		if (InScreen(screenHead.x, screenHead.y))
			ImGui::GetBackgroundDrawList()->AddCircle({ screenHead.x,screenHead.y }, headHeight - 3, ImColor(255, 255, 255));

	
		//for (int i = 0; i < sizeof(boneConnections) / sizeof(boneConnections[0]); i++)
		//{
		//	if (InScreen(player.ScreenBone1Arr[i].x, player.ScreenBone1Arr[i].y) && InScreen(player.ScreenBone2Arr[i].x, player.ScreenBone2Arr[i].y))
		//		ImGui::GetBackgroundDrawList()->AddLine({ player.ScreenBone1Arr[i].x, player.ScreenBone1Arr[i].y }, { player.ScreenBone2Arr[i].x, player.ScreenBone2Arr[i].y }, ImColor(255, 255, 255));
		//}

		//ImGui::GetBackgroundDrawList()->AddCircle({ player.ScreenBoneArr[0].x,player.ScreenBoneArr[0].y }, player.espWidth/2.f, ImColor(255, 255, 255));



	}

	void Cheats::Game::ConnectBones(int begin, int end)
	{
		Vector oldPoint;
		for (int i = begin; i <= end; i++)
		{
			if (InScreen(player.ScreenBoneArr[i].x, player.ScreenBoneArr[i].y))
			{
				if (i != begin)
				{
					ImGui::GetBackgroundDrawList()->AddLine({ oldPoint.x,oldPoint.y }, { player.ScreenBoneArr[i].x,player.ScreenBoneArr[i].y }, ImColor(255, 255, 255));
				}
				oldPoint = { player.ScreenBoneArr[i].x,player.ScreenBoneArr[i].y };
			}
		}
	}

	void Cheats::Game::DrawCross()
	{
		Vector cross = GetCross();

		
		ImGui::GetBackgroundDrawList()->AddLine({ cross.x - 10,cross.y }, { cross.x + 10,cross.y }, ImColor(255, 0, 0));
		ImGui::GetBackgroundDrawList()->AddLine({ cross.x,cross.y - 10 }, { cross.x ,cross.y + 10 }, ImColor(255, 0, 0));
		
	}

	void Cheats::Game::EnterAimQueue()
	{
		//if (Menu::aim自瞄 && GetAsyncKeyState(VK_XBUTTON2) & 0x8000)
		//{
		//	Menu::switchTarget = false;
		//}
		//else
		//{
		//	Menu::switchTarget = true;
		//}

		Vector cross = GetCross();

		float x = player.ScreenBoneArr[Menu::AimLocation].x;
		float y = player.ScreenBoneArr[Menu::AimLocation].y;
		if (!InScreen(x, y))
			return;

		Vector targetPos = { x,y };//自瞄部位的屏幕坐标
		player.dis2Cross = targetPos.CalculateDistanceToPoint2D(cross);
		//printf("dis2Cross:%f \r\n", player.dis2Cross);
		//printf("Spotted:%d \r\n", player.spotted);
		//比较获取离鼠标最近的
		if (player.dis2Cross < Menu::aimbotFOV &&
			player.dis2LP <= Menu::aimbotDis * 75 &&
			player.dis2Cross < tem_distance_to_crosshair)
		{
			tem_distance_to_crosshair = player.dis2Cross;
			temp_target_info = player;
			//aimTargetAddr = player.pAddr;
		}
	}

	void Cheats::Game::GetTargetInfo()
	{
		//给真正的变量赋值
		distance_to_crosshair = tem_distance_to_crosshair;
		target_info = temp_target_info;
		tem_distance_to_crosshair = 99999.f;//还原初始值
		ZeroMemory(&temp_target_info, sizeof(temp_target_info));//清空内存块
		//if (Menu::switchTarget)
		//	ZeroMemory(&aimTargetAddr, sizeof(aimTargetAddr));//清空内存块
	
	}

	void Cheats::Game::Aimbot()
	{

		if (target_info.pAddr != NULL && target_info.spotted)
		{


			////以需要自瞄的骨骼点xyz和自己摄像机xyz来计算俯仰角和偏航角
			//float temp_x = target_info.WorldBoneArr[Menu::AimLocation].x - player.origin.x;
			//float temp_y = target_info.WorldBoneArr[Menu::AimLocation].y - player.origin.y;
			//float temp_z = target_info.WorldBoneArr[Menu::AimLocation].z - (player.origin.z + 60.f);
			Vector cross = GetCross();
			float new_x = 0.f;
			float new_y = 0.f;
			
			if (Menu::aim后座补偿 && !Menu::DisplayToggle && getShots() > 1)
			{

				
				new_x = target_info.ScreenBoneArr[Menu::AimLocation].x - recoilPos.x + Menu::recoil_X;
				new_y = target_info.ScreenBoneArr[Menu::AimLocation].y - recoilPos.y + Menu::recoil_Y;
			}
			else
			{
				new_x = target_info.ScreenBoneArr[Menu::AimLocation].x - cross.x; // 移动的新坐标等于瞄准目标所在屏幕的坐标减去屏幕中心的坐标
				new_y = target_info.ScreenBoneArr[Menu::AimLocation].y - cross.y;
			}


			// 将新的算法集成进来
			float currentMousePositionX = 0.0f; // 假设当前鼠标位置为0
			float currentMousePositionY = 0.0f; // 假设当前鼠标位置为0

			// 调用更新鼠标位置的函数
			SpringAlgo(new_x, new_y, new_x, new_y, currentMousePositionX, currentMousePositionY, Menu::SPRING_CONSTANT, Menu::DAMPING_CONSTANT, Menu::GRAVITY_CONSTANT, Menu::MASS);

			// 发送鼠标移动事件
			mouse_event(MOUSEEVENTF_MOVE, static_cast<LONG>(currentMousePositionX), static_cast<LONG>(currentMousePositionY), 0, 0);

			//auto new_x = target_info.ScreenBoneArr[Menu::AimLocation].x - cross.x;
			//auto new_y = target_info.ScreenBoneArr[Menu::AimLocation].y - cross.y;

			//mouse_event(MOUSEEVENTF_MOVE, new_x, new_y, 0, 0);


			////计算俯仰角和偏航角
			//float angle_pitch = -(atan2(temp_z, sqrt(temp_x * temp_x + temp_y * temp_y)) * 180.0f / PI);
			//float angle_yaw = atan2(temp_y, temp_x) * 180.0f / PI;
			//uintptr_t pitchAddr = client + offsets::aimbot_pitch;
			//uintptr_t yawAddr = client + offsets::aimbot_yaw;
			////printf("pitchAddr:%llx   yawAddr:%llx  \r\n", pitchAddr, yawAddr);

			//Write(pitchAddr, angle_pitch);
			//Write(yawAddr, angle_yaw);
				


		}

	}

	void Cheats::Game::TriggerBot()
	{
		if (crosshair_ent && crosshair_ent != -1)
		{

			std::uintptr_t list_entry = Read<std::uintptr_t>(entityList + 0x8 * (crosshair_ent >> 9) + 0x10);
			if (!list_entry)
			{
				std::cout << "[-] List entry invalid\n";
				return;
			}
			const auto entity_pawn = Read<std::uintptr_t>(list_entry + 120 * (crosshair_ent & 0x1FF));
			if (!entity_pawn)
			{
				std::cout << "[-] Entity pawn is invalid\n";
				return;
			}
			int entity_team = Read<int>(entity_pawn + offsets::m_iTeamNum);
			//printf("准心人物阵营：%d\r\n", entity_team);
			if (Menu::util判断阵营 && pLocal.team == entity_team)
				return;

			mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);//模拟鼠标单击
			//Write<int>(client + offsets::attack, 65537);
			//if (time2 - time1 >= std::chrono::milliseconds(1000))
			//{
			//	Write<int>(client + offsets::attack, 16777472);
			//}
		}
		if (crosshair_ent == -1)
		{
			//Write<int>(client + offsets::attack, 16777472);
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
		}
		else
			mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);	

			//printf("getiIDEntIndex:%d\r\n", crosshair_ent);
			//printf("attack code: %d\r\n", Read<int>(client + offsets::attack));
			//recoilCompensation();
			//if (recoilPos.y < 528.f && recoilPos.y != 0)
			//{
			//	mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
			//	return;
			//}

			
			

	}

	int Cheats::Game::getShots()
	{
		return Read<int>(pLocal.pAddr + offsets::m_iShotsFired);
	}

	Vector Cheats::Game::getAimPunch()
	{
		return Read<Vector>(pLocal.pAddr + offsets::m_aimPunchAngle);
		//return Read<Vector>(pLocal.pAddr + offsets::m_aimPunchCache);
	}

	void Cheats::Game::recoilCompensation()
	{
		if (getShots() > 1)
		{
			aim_punch = getAimPunch();
			//printf("aim_punch: %f  %f\r\n", aim_punch.x, aim_punch.y);
			recoilPos = GetCross();

			const float alpha = 0.8;//缓动系数

			recoilPos.x = recoilPos.x * (1 - alpha) + (GetCross().x - aim_punch.y * 10) * alpha;
			recoilPos.y = recoilPos.y * (1 - alpha) + (GetCross().y + aim_punch.x * 10) * alpha;

			

			ImGui::GetBackgroundDrawList()->AddCircleFilled({ recoilPos.x - 2,recoilPos.y - 2 }, 6, ImColor(255, 255, 0));
		}
		else
		{
			recoilPos.x = 0;
			recoilPos.y = 0;
		}

	}

	void Cheats::Game::recoilMove(float x, float y)
	{
		if (Menu::aim后座补偿 && getShots())
			recoilCompensation();

		

		Vector cross = GetCross();
		// 将新的算法集成进来
		float currentMousePositionX = 0.0f; // 假设当前鼠标位置为0
		float currentMousePositionY = 0.0f; // 假设当前鼠标位置为0

		float new_x = cross.x - recoilPos.x + Menu::recoil_X;
		float new_y = cross.y - recoilPos.y + Menu::recoil_X;

		// 调用更新鼠标位置的函数
		SpringAlgo(new_x, new_y, new_x, new_y, currentMousePositionX, currentMousePositionY,
			120.f/*弹簧刚度*/,
			150.f/*阻尼*/,
			5.f/*引力常数*/,
			50.f/*质量*/);
		//printf("currentMousePosition:%f  %f  \r\n", currentMousePositionX, currentMousePositionY);
		//printf("recoilPos:%f  %f  \r\n", recoilPos.x, recoilPos.y);
		// 发送鼠标移动事件
		mouse_event(MOUSEEVENTF_MOVE, static_cast<LONG>(currentMousePositionX), static_cast<LONG>(currentMousePositionY), 0, 0);
		//ZeroMemory(&recoilPos, sizeof(recoilPos));
	}

	int Cheats::Game::getiIDEntIndex()
	{
		return Read<int>(pLocal.pAddr + offsets::m_iIDEntIndex);
	}



	/*void Cheats::Game::ConnectBones(int begin, int end)
	{
		Vector2 oldPoint;
		for (int i = begin; i <= end; i++)
		{
			if (!InScreen(player.ScreenBoneArr[i].x, player.ScreenBoneArr[i].y))
				return;

			if (i != begin)
			{

				ImGui::GetBackgroundDrawList()->AddLine({ oldPoint.x,oldPoint.y }, { player.ScreenBoneArr[i].x,player.ScreenBoneArr[i].y }, ImColor(255, 255, 255));
			}
			oldPoint = { player.ScreenBoneArr[i].x,player.ScreenBoneArr[i].y };
		}
	}*/

	bool Cheats::Game::InScreen(float x, float y)
	{
		auto& width = Visual::external.gamewindow.size.x;
		auto& high = Visual::external.gamewindow.size.y;
		if (x > width || y > high || x < 0 || y < 0)
			return false;

		return true;
	}

	Vector Cheats::Game::GetCross()
	{
		return { Visual::external.gamewindow.size.x / 2,Visual::external.gamewindow.size.y / 2 };
	}

