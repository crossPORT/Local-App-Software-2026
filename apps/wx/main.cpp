#include "demo_config.h"
#include "main_frame.h"

#include <wx/cmdline.h>
#include <wx/wx.h>

#include <iostream>

class DemoApp : public wxApp {
public:
    bool OnInit() override {
        wxCmdLineParser parser;
        parser.AddOption("c", "config", "Identity config file",
                         wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
        parser.SetCmdLine(wxApp::argc, wxApp::argv);
        if (parser.Parse(false) != 0) {
            return false;
        }

        wxString config_path;
        parser.Found("config", &config_path);
        if (config_path.empty()) {
            config_path = "booth-port0.conf";
        }

        auto* frame = new MainFrame(config_path.ToStdString());
        frame->Show(true);
        frame->Raise();
        if (!frame->IsShownOnScreen()) {
            frame->Centre();
        }
        return true;
    }
};

wxIMPLEMENT_APP(DemoApp);
