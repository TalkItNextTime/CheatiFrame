#include "Menu.h"
#include "../Visuals/External.h"

#include <cstring>

bool isListeningForAimKey = false;
bool isListeningForTriggerKey = false;

int GetPressedKey() {
	for (int key = 0x01; key < 0xFE; key++) {
		if (GetAsyncKeyState(key) & 0x8000) {
			return key;
		}
	}
	return 0;
}
void Menu::ShowMenu()
{


	static float temp5 = 50.f;
	//自定义菜单区
	ImGuiIO& io = ImGui::GetIO();
	ImGui::Begin("Cosmic");
	ImGui::Text(u8"使用Insert键控制菜单显隐");
	ImGui::Text(u8"帧数:%.2f     ", io.Framerate);
	ImGui::SameLine();
	//ImGui::GetFrameCount();
	//按钮
	if (ImGui::Button(u8"退出"))
	{
		exit(0);
	}
	ImGui::Checkbox(u8"判断阵营", &Menu::util判断阵营);
	ImGui::Checkbox(u8"可视检查", &Menu::util可视检查);

	//新建选项卡
	if (ImGui::BeginTabBar(u8"选项卡"))
	{

		//选项标签
		if (ImGui::BeginTabItem(u8"视觉选项"))
		{
			//ImGui::Text(u8"There is 选项标签 1");
			ImGui::Checkbox(u8"绘制总开关", &Menu::util绘制总开关);
			ImGui::Checkbox(u8"绘制准心", &Menu::vis绘制准心);
			ImGui::Checkbox(u8"绘制2D方框", &Menu::vis方框透视);
			//ImGui::Checkbox(u8"绘制动态2D方框", &Menu::DrawDynamicESP);
			ImGui::Checkbox(u8"绘制3D方框", &Menu::vis3DBox透视);
			ImGui::Checkbox(u8"绘制血量", &Menu::vis绘制血条);
			//ImGui::Checkbox(u8"绘制连线", &Menu::DrawLine);
			ImGui::Checkbox(u8"绘制距离", &Menu::vis绘制距离);
			ImGui::Checkbox(u8"绘制骨骼", &Menu::vis绘制骨骼);

			//ImGui::Checkbox(u8"绘制3D方框", &Menu::DrawDynamicESP);



			//滑块
			//ImGui::SliderFloat(u8"滑块1", &temp5, 0.f, 100.f, "%.1f");

			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(u8"瞄准"))
		{
			//ImGui::Text(u8"There is 选项标签 2");
			ImGui::Checkbox(u8"绘制FOV", &Menu::aim绘制FOV);
			//ImGui::Checkbox(u8"可视判断", &Menu::aim可视判断);
			ImGui::Text(u8"扳机和后座补偿不要同时开启");
			ImGui::Checkbox(u8"自瞄", &Menu::aim自瞄);
			ImGui::SameLine();
			ImGui::Checkbox(u8"扳机", &Menu::aim扳机);
			ImGui::SameLine();
			ImGui::Checkbox(u8"后座补偿", &Menu::aim后座补偿);

			///ImGui::Combo(u8"自瞄热键", &Menu::DefaultAimHotKey, Menu::aimHotKey, IM_ARRAYSIZE(Menu::aimHotKey));
			//ImGui::Combo(u8"扳机热键", &Menu::DefaultTriggleHotKey, Menu::TriggleHotKey, IM_ARRAYSIZE(Menu::TriggleHotKey));

			ImGui::Text(isListeningForAimKey ? u8"等待设置热键..." : u8"自瞄热键: %d", Menu::aimKey);
			if (ImGui::Button(u8"切换自瞄热键"))
				isListeningForAimKey = true;
			if (isListeningForAimKey) {
				int pressedKey = GetPressedKey();
				if (pressedKey != 0) {
					Menu::aimKey = pressedKey;
					isListeningForAimKey = false;
				}
			}
			ImGui::Text(isListeningForTriggerKey ? u8"等待设置热键..." : u8"扳机热键: %d", Menu::triggerKey);
			if (ImGui::Button(u8"切换扳机热键"))
				isListeningForTriggerKey = true;
			if (isListeningForTriggerKey) {
				int pressedKey = GetPressedKey();
				if (pressedKey != 0) {
					Menu::triggerKey = pressedKey;
					isListeningForTriggerKey = false;
				}
			}


			ImGui::SliderInt(u8"后座补偿X", &Menu::recoil_X, 0, 100, "%d");
			ImGui::SliderInt(u8"后座补偿Y", &Menu::recoil_Y, 0, 100, "%d");

			ImGui::Text(u8"自瞄部位");
			ImGui::RadioButton(u8"头部", &Menu::AimLocation, Menu::AimLoc::Head);
			ImGui::SameLine();
			ImGui::RadioButton(u8"胸部", &Menu::AimLocation, Menu::AimLoc::Chest);
			//ImGui::Checkbox(u8"绘制目标连线", &Menu::DrawTarget);
			ImGui::SliderFloat(u8"自瞄FOV", &Menu::aimbotFOV, 30.f, 800.f, "%.1f");
			ImGui::SliderInt(u8"瞄准距离", &Menu::aimbotDis, 0, 500, "%d");

			ImGui::SliderFloat(u8"鼠标质量（越大越慢）", &Menu::MASS, 5.f, 100.f, "%.1f");
			ImGui::SliderFloat(u8"弹簧刚度（越大越快）", &Menu::SPRING_CONSTANT, 0.f, 3000.f, "%.1f");
			ImGui::SliderFloat(u8"阻尼（越大失速越快）", &Menu::DAMPING_CONSTANT, 0.f, 1000.f, "%.1f");
			//ImGui::SliderFloat(u8"引力", &Menu::GRAVITY_CONSTANT, 0.f, 20.f, "%.1f");




			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem(u8"投掷物辅助"))
		{
			if (!Menu::helper本次菜单已自动同步)
			{
				Menu::helper本次菜单已自动同步 = true;
				Menu::helper请求同步手雷类型 = true;
			}

			static const char* grenadeTypes[] = { "Smoke", "Flash", "HE", "Decoy" };
			static const char* throwTypes[] = { u8"站投", u8"跳投", u8"跑投" };

			ImGui::Checkbox(u8"启用投掷物辅助", &Menu::helper启用);
			ImGui::Checkbox(u8"按当前手雷类型筛选", &Menu::helper按武器筛选);
			ImGui::Checkbox(u8"绘制站位", &Menu::helper绘制站位);
			ImGui::Checkbox(u8"绘制瞄点", &Menu::helper绘制瞄点);
			ImGui::TextWrapped(u8"说明: 手雷类型会在菜单每次重新显示后，首次打开本页时自动读取一次当前手持投掷物。"
				u8"\n手动覆盖仅用于调试/录制：强制按你选择的手雷类型筛选或记录。关闭后恢复自动读取。");

			ImGui::Separator();
			ImGui::Checkbox(u8"手动覆盖手雷类型", &Menu::helper手动类型覆盖);
			ImGui::Combo(u8"手雷类型", &Menu::helper手动类型, grenadeTypes, IM_ARRAYSIZE(grenadeTypes));
			ImGui::Combo(u8"投掷方式", &Menu::helper投掷方式, throwTypes, IM_ARRAYSIZE(throwTypes));

			ImGui::SliderFloat(u8"站位判定容差", &Menu::helper站位容差, 10.0f, 120.0f, "%.1f");
			ImGui::SliderFloat(u8"聚焦半径(像素)", &Menu::helper聚焦半径, 20.0f, 100.0f, "%.1f");
			ImGui::SliderFloat(u8"站位最大绘制距离", &Menu::helper站位最远绘制, 500.0f, 5000.0f, "%.0f");
			ImGui::SliderFloat(u8"非聚焦引导线阈值", &Menu::helper非聚焦引导线距离, 50.0f, 8000.0f, "%.0f");
			ImGui::SliderFloat(u8"记录瞄点距离", &Menu::helper记录瞄点距离, 2000.0f, 20000.0f, "%.0f");

			ImGui::Separator();
			ImGui::InputText(u8"地图名", Menu::helper地图名, IM_ARRAYSIZE(Menu::helper地图名));
			ImGui::InputText(u8"备注", Menu::helper备注, IM_ARRAYSIZE(Menu::helper备注));

			if (ImGui::Button(u8"记录当前点位"))
				Menu::helper请求记录 = true;
			ImGui::SameLine();
			if (ImGui::Button(u8"刷新地图点位"))
				Menu::helper请求刷新 = true;

			ImGui::TextWrapped(u8"状态: %s", Menu::helper状态.c_str());

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem(u8"配置"))
		{
			if (Menu::config列表.empty())
			{
				Menu::config请求刷新列表 = true;
			}

			ImGui::Text(u8"配置管理");
			ImGui::InputText(u8"配置名", Menu::config名称, IM_ARRAYSIZE(Menu::config名称));

			if (ImGui::Button(u8"刷新配置列表"))
				Menu::config请求刷新列表 = true;
			ImGui::SameLine();
			if (ImGui::Button(u8"保存配置"))
				Menu::config请求保存 = true;
			ImGui::SameLine();
			if (ImGui::Button(u8"加载配置"))
				Menu::config请求加载 = true;

			const char* previewName = Menu::config列表.empty()
				? u8"(无配置)"
				: Menu::config列表[(Menu::config选择索引 < 0 || Menu::config选择索引 >= static_cast<int>(Menu::config列表.size()))
					? 0
					: Menu::config选择索引].c_str();

			if (ImGui::BeginCombo(u8"已保存配置", previewName))
			{
				for (int i = 0; i < static_cast<int>(Menu::config列表.size()); ++i)
				{
					const bool selected = (Menu::config选择索引 == i);
					if (ImGui::Selectable(Menu::config列表[i].c_str(), selected))
					{
						Menu::config选择索引 = i;
						strncpy_s(Menu::config名称, Menu::config列表[i].c_str(), _TRUNCATE);
					}
					if (selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::TextWrapped(u8"配置状态: %s", Menu::config状态.c_str());
			ImGui::EndTabItem();
		}



		ImGui::EndTabBar();

	}




	ImGui::End();


}
