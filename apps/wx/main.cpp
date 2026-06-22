#include "session_config.h"
#include "main_frame.h"

#include <wx/cmdline.h>
#include <wx/wx.h>

#include <iostream>

class RocketBoxApp : public wxApp {
public:
    bool OnInit() override {
        wxCmdLineParser parser;
        parser.AddOption("c", "config", "Identity config file",
                         wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
        parser.AddOption("p", "port", "Fabric USB port index (0 or 1 when two cables on one PC)",
                         wxCMD_LINE_VAL_NUMBER, wxCMD_LINE_PARAM_OPTIONAL);
        parser.SetCmdLine(wxApp::argc, wxApp::argv);
        if (parser.Parse(false) != 0) {
            return false;
        }

        wxString config_path;
        parser.Found("config", &config_path);

        int cli_port_index = -1;
        long port_value = 0;
        if (parser.Found("port", &port_value)) {
            cli_port_index = static_cast<int>(port_value);
        }

        auto* frame = new MainFrame(config_path.ToStdString(), cli_port_index);
        frame->Show(true);
        frame->Raise();
        if (!frame->IsShownOnScreen()) {
            frame->Centre();
        }
        return true;
    }
};

wxIMPLEMENT_APP(RocketBoxApp);
