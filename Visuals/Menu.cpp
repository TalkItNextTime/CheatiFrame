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
	Menu::utilVPK可视解析 = true;
	ImGui::TextDisabled(u8"VPK地图可视解析: 已强制启用");
	ImGui::TextWrapped(u8"%s", Menu::vpk可视状态.c_str());

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
			ImGui::Checkbox(u8"绘制可视骨骼点", &Menu::vis绘制可视骨骼点);

			ImGui::SeparatorText(u8"颜色设置");
			ImGui::ColorEdit4(u8"骨骼颜色", Menu::color骨骼);
			ImGui::ColorEdit4(u8"可视骨骼颜色", Menu::color可视骨骼);
			ImGui::ColorEdit4(u8"2D ESP颜色", Menu::color2DESP);

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
			ImGui::Checkbox(u8"智能部位选择", &Menu::aim智能部位选择);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(u8"自动选择当前暴露在掩体外的骨骼部位进行瞄准。");

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
			ImGui::SliderInt(u8"读线程休眠(ms)", &Menu::read线程休眠毫秒, 1, 20, "%d");
			ImGui::SliderInt(u8"ESP线程休眠(ms)", &Menu::esp线程休眠毫秒, 1, 30, "%d");
			ImGui::SliderInt(u8"瞄准切换延时(ms)", &Menu::瞄准切换延时毫秒, 0, 1000, "%d");
			//ImGui::SliderFloat(u8"引力", &Menu::GRAVITY_CONSTANT, 0.f, 20.f, "%.1f");




			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem(u8"投掷物辅助"))
		{
			if (Menu::helper列表等待本次菜单自动高亮)
			{
				Menu::helper列表等待本次菜单自动高亮 = false;
			}

			if (!Menu::helper本次菜单已自动同步)
			{
				Menu::helper本次菜单已自动同步 = true;
				Menu::helper请求同步手雷类型 = true;
			}

			static const char* grenadeTypes[] = { u8"烟雾弹", u8"闪光弹", u8"高爆雷", u8"诱饵弹", u8"燃烧弹" };
			static const char* throwTypes[] = { u8"站投", u8"跳投", u8"跑投" };
			static const char* listGrenadeTypes[] = { u8"烟雾", u8"燃烧", u8"闪光", u8"手雷", u8"诱饵" };
			static const char* rowGrenadeTypes[] = { u8"烟雾弹", u8"闪光弹", u8"高爆雷", u8"诱饵弹", u8"燃烧弹" };
			static const char* rowThrowTypes[] = { u8"站投", u8"跳投", u8"跑投", u8"跑跳" };

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
			float drawDistMeter = Menu::helper站位最远绘制 / 75.0f;
			if (ImGui::SliderFloat(u8"站位最大绘制距离(米)", &drawDistMeter, 6.0f, 80.0f, "%.1f m"))
				Menu::helper站位最远绘制 = drawDistMeter * 75.0f;
			ImGui::SliderFloat(u8"非聚焦引导线阈值", &Menu::helper非聚焦引导线距离, 50.0f, 8000.0f, "%.0f");
			ImGui::SliderFloat(u8"顶部投掷提示偏移X", &Menu::helper顶部提示偏移X, -600.0f, 1280.0f, "%.0f");
			ImGui::SliderFloat(u8"顶部投掷提示偏移Y", &Menu::helper顶部提示偏移Y, -300.0f, 720.0f, "%.0f");

			ImGui::Separator();
			ImGui::InputText(u8"地图名", Menu::helper地图名, IM_ARRAYSIZE(Menu::helper地图名));
			ImGui::InputText(u8"备注", Menu::helper备注, IM_ARRAYSIZE(Menu::helper备注));

			if (ImGui::Button(u8"记录当前点位"))
				Menu::helper请求记录 = true;
			ImGui::SameLine();
			if (ImGui::Button(u8"刷新地图点位"))
				Menu::helper请求刷新 = true;

			ImGui::TextWrapped(u8"状态: %s", Menu::helper状态.c_str());

			ImGui::Separator();
			ImGui::Text(u8"点位列表管理");

			if (Menu::helper列表数据.empty())
				Menu::helper列表请求刷新 = true;

			for (int i = 0; i < 5; ++i)
			{
				ImGui::Checkbox(listGrenadeTypes[i], &Menu::helper列表筛选类型[i]);
				if (i != 4)
					ImGui::SameLine();
			}

			if (ImGui::Button(u8"刷新列表"))
				Menu::helper列表请求刷新 = true;
			ImGui::SameLine();
			if (ImGui::Button(u8"保存到文件"))
				Menu::helper列表请求保存 = true;

			ImGui::BeginChild("GrenadeManageList", ImVec2(0, 320), true);
			if (ImGui::BeginTable("GrenadeManageTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
			{
				ImGui::TableSetupColumn(u8"序号", ImGuiTableColumnFlags_WidthFixed, 55.0f);
				ImGui::TableSetupColumn(u8"类型", ImGuiTableColumnFlags_WidthFixed, 120.0f);
				ImGui::TableSetupColumn(u8"名称", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn(u8"投掷", ImGuiTableColumnFlags_WidthFixed, 110.0f);
				ImGui::TableSetupColumn(u8"操作", ImGuiTableColumnFlags_WidthFixed, 70.0f);
				ImGui::TableHeadersRow();

				const bool anyFilterOn = Menu::helper列表筛选类型[0] || Menu::helper列表筛选类型[1] || Menu::helper列表筛选类型[2] || Menu::helper列表筛选类型[3] || Menu::helper列表筛选类型[4];
				std::vector<size_t> toDelete{};

				for (size_t i = 0; i < Menu::helper列表数据.size(); ++i)
				{
					auto& row = Menu::helper列表数据[i];
					const bool isAutoHighlighted = (Menu::helper列表高亮点位ID > 0 && row.id == Menu::helper列表高亮点位ID);
					const int filterIndex =
						(row.typeIndex == 0) ? 0 :
						(row.typeIndex == 4) ? 1 :
						(row.typeIndex == 1) ? 2 :
						(row.typeIndex == 2) ? 3 :
						4;

					if (anyFilterOn && (filterIndex < 0 || filterIndex >= 5 || !Menu::helper列表筛选类型[filterIndex]))
						continue;

					ImGui::PushID(static_cast<int>(row.id + static_cast<int>(i) * 1000));
					ImGui::TableNextRow();
					if (isAutoHighlighted)
					{
						ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImColor(242, 198, 46, 110));
						if (Menu::helper列表高亮待滚动)
						{
							ImGui::SetScrollHereY(0.35f);
							Menu::helper列表高亮待滚动 = false;
						}
					}
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%d", static_cast<int>(i + 1));

					ImGui::TableSetColumnIndex(1);
					ImGui::SetNextItemWidth(-1.0f);
					ImGui::Combo("##Type", &row.typeIndex, rowGrenadeTypes, IM_ARRAYSIZE(rowGrenadeTypes));

					ImGui::TableSetColumnIndex(2);
					ImGui::SetNextItemWidth(-1.0f);
					ImGui::InputText("##Name", row.name, IM_ARRAYSIZE(row.name));

					ImGui::TableSetColumnIndex(3);
					ImGui::SetNextItemWidth(-1.0f);
					ImGui::Combo("##Throw", &row.throwIndex, rowThrowTypes, IM_ARRAYSIZE(rowThrowTypes));

					ImGui::TableSetColumnIndex(4);
					if (ImGui::Button(u8"删除"))
						toDelete.push_back(i);

					ImGui::PopID();
				}

				for (auto it = toDelete.rbegin(); it != toDelete.rend(); ++it)
				{
					if (*it < Menu::helper列表数据.size())
						Menu::helper列表数据.erase(Menu::helper列表数据.begin() + static_cast<long long>(*it));
				}

				ImGui::EndTable();
			}
			ImGui::EndChild();

			ImGui::TextWrapped(u8"列表状态: %s", Menu::helper列表状态.c_str());

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
