#include "AICopilotPanel.hpp"
#include "I18N.hpp"
#include "GUI_App.hpp"
#include "../Utils/Http.hpp"
#include <nlohmann/json.hpp>

namespace Slic3r {
namespace GUI {

AICopilotPanel::AICopilotPanel(wxWindow* parent,
                               wxWindowID id,
                               const wxPoint& pos,
                               const wxSize& size,
                               long style)
    : wxPanel(parent, id, pos, size, style),
      m_alive(std::make_shared<bool>(true))
{
    create_widgets();
}

AICopilotPanel::~AICopilotPanel()
{
    if (m_alive) {
        *m_alive = false;
    }
    if (m_current_request) {
        m_current_request->cancel();
    }
}

void AICopilotPanel::create_widgets()
{
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

    auto* main_sizer = new wxBoxSizer(wxVERTICAL);

    // Chat history / log (read-only, multiline)
    m_chat_log = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                wxDefaultPosition, wxSize(-1, FromDIP(130)),
                                wxTE_MULTILINE | wxTE_READONLY | wxBORDER_SIMPLE);
    main_sizer->Add(m_chat_log, 1, wxEXPAND | wxALL, FromDIP(4));

    // Input row: text box + Ask button
    auto* input_sizer = new wxBoxSizer(wxHORIZONTAL);

    m_input_text = new wxTextCtrl(this, wxID_ANY, wxEmptyString,
                                  wxDefaultPosition, wxDefaultSize,
                                  wxTE_PROCESS_ENTER | wxBORDER_SIMPLE);
    m_input_text->SetHint(_L("Consultá al copiloto..."));

    m_btn_ask = new wxButton(this, wxID_ANY, _L("Preguntar"),
                             wxDefaultPosition, wxDefaultSize);

    input_sizer->Add(m_input_text, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
    input_sizer->Add(m_btn_ask, 0, wxALIGN_CENTER_VERTICAL);

    main_sizer->Add(input_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(4));

    SetSizer(main_sizer);
    Layout();

    // Event bindings
    m_btn_ask->Bind(wxEVT_BUTTON, &AICopilotPanel::on_ask_button, this);
    m_input_text->Bind(wxEVT_TEXT_ENTER, &AICopilotPanel::on_enter_pressed, this);

    wxGetApp().UpdateDarkUI(this);

    // Initial greeting
    append_message("Copiloto", _L("Hola Eduardo. Estoy listo para ayudarte a calibrar y optimizar tus impresiones."));
}

void AICopilotPanel::append_message(const wxString& sender, const wxString& text)
{
    if (!m_chat_log)
        return;

    wxString formatted = wxString::Format("[%s]: %s\n\n", sender, text);
    m_chat_log->AppendText(formatted);
}

void AICopilotPanel::clear_messages()
{
    if (m_chat_log)
        m_chat_log->Clear();
}

void AICopilotPanel::on_ask_button(wxCommandEvent& evt)
{
    wxString text = m_input_text->GetValue().Trim().Trim(false);
    if (text.IsEmpty())
        return;

    append_message("Eduardo", text);
    m_input_text->Clear();

    send_query_to_brain(text);
}

void AICopilotPanel::on_enter_pressed(wxCommandEvent& evt)
{
    on_ask_button(evt);
}

void AICopilotPanel::send_query_to_brain(const wxString& text)
{
    if (m_btn_ask)
        m_btn_ask->Disable();

    nlohmann::json payload;
    payload["user_message"] = text.ToUTF8().data();
    std::string post_body = payload.dump();

    if (m_current_request) {
        m_current_request->cancel();
        m_current_request.reset();
    }

    auto http = Slic3r::Http::post("http://127.0.0.1:8787/echo");
    m_current_request = http.header("Content-Type", "application/json")
        .set_post_body(post_body)
        .timeout_connect(3)
        .timeout_max(10)
        .on_complete([this, alive = m_alive](std::string body, unsigned http_status) {
            wxGetApp().CallAfter([this, alive, body, http_status]() {
                if (!*alive) return;
                if (m_btn_ask) m_btn_ask->Enable();
                try {
                    auto j = nlohmann::json::parse(body);
                    std::string reply = j.value("reply", "");
                    if (reply.empty())
                        reply = body;
                    append_message("Copiloto", wxString::FromUTF8(reply.c_str()));
                } catch (...) {
                    append_message("Copiloto", wxString::FromUTF8(body.c_str()));
                }
            });
        })
        .on_error([this, alive = m_alive](std::string body, std::string error, unsigned http_status) {
            wxGetApp().CallAfter([this, alive, error, http_status]() {
                if (!*alive) return;
                if (m_btn_ask) m_btn_ask->Enable();
                wxString err_msg = wxString::Format(_L("Error conectando con brain_service (%s, código %u)"),
                                                    wxString::FromUTF8(error.c_str()), http_status);
                append_message("Copiloto", err_msg);
            });
        })
        .perform();
}

} // namespace GUI
} // namespace Slic3r
