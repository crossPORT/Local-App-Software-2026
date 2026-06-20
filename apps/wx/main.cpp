#include "demo_config.h"
#include "main_frame.h"

#include <wx/cmdline.h>
#include <wx/wx.h>

#include <iostream>

class DemoApp : public wxApp {
public:
    bool OnInit() override {
        wxCmdLineParser parser;
        parser.AddOption("p", "port", "Fabric port index (0 = first 1772:0006 device)",
                         wxCMD_LINE_VAL_NUMBER, wxCMD_LINE_PARAM_OPTIONAL);
        parser.AddOption("c", "config", "Demo config file",
                         wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
        parser.SetCmdLine(wxApp::argc, wxApp::argv);
        if (parser.Parse(false) != 0) {
            return false;
        }

        long port = 0;
        parser.Found("port", &port);
        if (port < 0) {
            std::cerr << "Port index must be >= 0\n";
            return false;
        }

        wxString config_path;
        parser.Found("config", &config_path);
        if (config_path.empty()) {
            config_path = wxString::Format("booth-port%ld.conf", port);
        }

        auto* frame = new MainFrame(static_cast<int>(port), config_path.ToStdString());
        frame->Show(true);
        frame->Raise();
        if (!frame->IsShownOnScreen()) {
            frame->Centre();
        }
        return true;
    }
};

wxIMPLEMENT_APP(DemoApp);
