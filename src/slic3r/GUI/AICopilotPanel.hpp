#pragma once

#include <wx/panel.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/sizer.h>

namespace Slic3r {
namespace GUI {

class AICopilotPanel : public wxPanel
{
public:
    AICopilotPanel(wxWindow* parent,
                   wxWindowID id = wxID_ANY,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   long style = wxTAB_TRAVERSAL);
    virtual ~AICopilotPanel() = default;

    void append_message(const wxString& sender, const wxString& text);
    void clear_messages();

private:
    void create_widgets();
    void on_ask_button(wxCommandEvent& evt);
    void on_enter_pressed(wxCommandEvent& evt);

    wxTextCtrl* m_chat_log{nullptr};
    wxTextCtrl* m_input_text{nullptr};
    wxButton*   m_btn_ask{nullptr};
};

} // namespace GUI
} // namespace Slic3r
