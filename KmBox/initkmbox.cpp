#include <kmboxNet.h>
#include <KmboxB.h>
#include "menu/menu.h"
#include "config/config.h"
#include "initkmbox.h"
#include <iostream>

_com comPort;

namespace kmbox {
    void c_initialize::initkmbox(const cfg& cfg)
    {
        if (cfg.kmboxtype == "Net")
        {
            std::cout << "Initializing KMbox NET with IP: " << cfg.kmboxip
                << ", Port: " << cfg.kmboxport
                << ", UUID: " << cfg.kmboxuuid << std::endl;

            int initResult = kmNet_init(const_cast<char*>(cfg.kmboxip.c_str()),
                const_cast<char*>(std::to_string(cfg.kmboxport).c_str()),
                const_cast<char*>(cfg.kmboxuuid.c_str()));

            if (initResult != success)
            {
                std::cerr << "Failed to initialize KMbox NET, error code: " << initResult << std::endl;
                exit(1);
            }
            std::cout << "KMbox NET initialized successfully\n";
        }
        else if (cfg.kmboxtype == "BPro")
        {
            std::cout << "Initializing KMbox BPro on COM port " << cfg.kmboxcomport << std::endl;

            if (comPort.open(cfg.kmboxcomport, 115200)) // Adjust baud rate as necessary
            {
                std::cout << "KMbox BPro initialized successfully." << std::endl;
            }
            else
            {
                std::cerr << "Failed to initialize KMbox BPro on COM port " << cfg.kmboxcomport << std::endl;
                exit(1);
            }
        }
        else
        {
            std::cerr << "Wrong KMbox type specified, please type exactly Net or BPro in your cfg." << cfg.kmboxtype << std::endl;
            exit(1);
        }


        int deltaX = 600; // Move right
        int deltaY = 600;   // 
        if (cfg.kmboxtype == "NET")
        {
            std::cout << "Moving mouse 600 pixels to the right (NET)\n";
            kmNet_mouse_move_auto(deltaX, deltaY, 2000); // Adjust timing as necessary
        }
        else if (cfg.kmboxtype == "BPro")
        {
            // Docx file included in the project files /Kmbox
            std::cout << "Moving mouse 600 pixels to the right (BPro)\n";
            char cmd[1024] = { 0 };
            sprintf_s(cmd, "km.move(%d, %d, 10)\r\n", deltaX, deltaY);
            comPort.write(cmd);
        }
    }
}
