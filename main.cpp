#include "Visuals/External.h"
#include "Cheats/Cheats.h"



int main()
{
	Visual::external.AttachWindow("SDL_app", "Counter-Strike 2", Cheats::CheatMain);
	//myimgui::imgui_external
	return 0;
}