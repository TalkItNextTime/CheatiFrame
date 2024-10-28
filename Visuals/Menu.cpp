#include "Menu.h"
#include "../Visuals/External.h"


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

	//新建选项卡
	if (ImGui::BeginTabBar(u8"选项卡"))
	{

		//选项标签
		if (ImGui::BeginTabItem(u8"视觉选项"))
		{
			//ImGui::Text(u8"There is 选项标签 1");
			ImGui::Checkbox(u8"绘制总开关", &Menu::util绘制总开关);
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
			ImGui::Checkbox(u8"自瞄", &Menu::aim自瞄);
			ImGui::Checkbox(u8"扳机", &Menu::aim扳机);

			ImGui::Text(u8"自瞄部位");
			ImGui::RadioButton(u8"头部", &Menu::AimLocation, Menu::AimLoc::Head);
			ImGui::SameLine();
			ImGui::RadioButton(u8"胸部", &Menu::AimLocation, Menu::AimLoc::Chest);
			//ImGui::Checkbox(u8"绘制目标连线", &Menu::DrawTarget);
			ImGui::SliderFloat(u8"自瞄FOV", &Menu::aimbotFOV, 30.f, 800.f, "%.1f");
			ImGui::SliderInt(u8"瞄准距离", &Menu::aimbotDis, 0, 500, "%d");

			ImGui::SliderFloat(u8"鼠标质量（越大越慢）", &Menu::MASS, 5.f, 100.f, "%.1f");
			ImGui::SliderFloat(u8"弹簧刚度（越大越快）", &Menu::SPRING_CONSTANT, 0.f, 3000.f, "%.1f");
			//ImGui::SliderFloat(u8"阻尼（越大失速越快）", &Menu::DAMPING_CONSTANT, 0.f, 1000.f, "%.1f");
			//ImGui::SliderFloat(u8"引力", &Menu::GRAVITY_CONSTANT, 0.f, 20.f, "%.1f");




			ImGui::EndTabItem();
		}



		ImGui::EndTabBar();

	}




	ImGui::End();


}