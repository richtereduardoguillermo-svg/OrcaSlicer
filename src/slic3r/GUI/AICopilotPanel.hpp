#pragma once

#include <wx/panel.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <memory>
#include <map>
#include <string>
#include <vector>

class wxDataViewListCtrl;

namespace Slic3r {
class Http;
class PresetBundle;

namespace GUI {

class AICopilotPanel : public wxPanel
{
public:
    AICopilotPanel(wxWindow* parent,
                   wxWindowID id = wxID_ANY,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   long style = wxTAB_TRAVERSAL);
    virtual ~AICopilotPanel();

    void append_message(const wxString& sender, const wxString& text);
    void clear_messages();

    void show_proposed_diff(const std::map<std::string, std::string>& proposed_changes);
    void clear_proposed_diff();
    bool apply_proposed_changes();

private:
    void create_widgets();
    void on_ask_button(wxCommandEvent& evt);
    void on_enter_pressed(wxCommandEvent& evt);
    void on_apply_button(wxCommandEvent& evt);
    void on_discard_button(wxCommandEvent& evt);
    void send_query_to_brain(const wxString& text);

    wxTextCtrl*         m_chat_log{nullptr};
    wxTextCtrl*         m_input_text{nullptr};
    wxButton*           m_btn_ask{nullptr};

    // Diff UI
    wxPanel*            m_diff_panel{nullptr};
    wxStaticText*       m_diff_label{nullptr};
    wxDataViewListCtrl* m_diff_list{nullptr};
    wxButton*           m_btn_apply{nullptr};
    wxButton*           m_btn_discard{nullptr};

    std::map<std::string, std::string> m_pending_changes;

    std::shared_ptr<bool> m_alive;
    std::shared_ptr<Http> m_current_request;
};

} // namespace GUI
} // namespace Slic3r
