#include <wx/app.h>
#include <wx/cmdline.h>
#include <wx/image.h>

#include "main_frame.h"

namespace {

// Usage: pasgen-wx [vault.pif]
class PasgenApp : public wxApp {
public:
    bool OnInit() override {
        if (!wxApp::OnInit()) return false;  // parses the command line
        SetAppName("pasgen");
        SetAppDisplayName("Pasgen");
        SetVendorName("L0C8");
        wxInitAllImageHandlers();
        auto* frame = new pasgen::wxui::MainFrame(vault_);
        frame->Show();
        return true;
    }

    // Any key or mouse input counts as activity for auto-lock.
    int FilterEvent(wxEvent& event) override {
        const wxEventType t = event.GetEventType();
        if (t == wxEVT_KEY_DOWN || t == wxEVT_LEFT_DOWN || t == wxEVT_RIGHT_DOWN || t == wxEVT_MIDDLE_DOWN ||
            t == wxEVT_MOUSEWHEEL || t == wxEVT_MOTION)
            pasgen::wxui::MarkActivity();
        return Event_Skip;
    }

    void OnInitCmdLine(wxCmdLineParser& parser) override {
        wxApp::OnInitCmdLine(parser);
        parser.AddParam("vault file (.pif) to unlock", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL);
    }

    bool OnCmdLineParsed(wxCmdLineParser& parser) override {
        if (parser.GetParamCount() > 0) vault_ = parser.GetParam(0);
        return wxApp::OnCmdLineParsed(parser);
    }

private:
    wxString vault_;
};

}  // namespace

wxIMPLEMENT_APP(PasgenApp);
