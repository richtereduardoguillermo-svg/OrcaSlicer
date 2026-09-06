#include "AICopilotPanel.hpp"
#include "I18N.hpp"
#include "GUI_App.hpp"

namespace Slic3r {
namespace GUI {

AICopilotPanel::AICopilotPanel(wxWindow* parent,
                               wxWindowID id,
                               const wxPoint& pos,
                               const wxSize& size,
                               long style)
    : wxPanel(parent, id, pos, size, style)
{
    create_widgets();
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

    // Initial greeting in skeleton mode
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

    // Skeleton placeholder response
    append_message("Copiloto", _L("(Modo esqueleto) Recibí tu consulta: ") + text);
}

void AICopilotPanel::on_enter_pressed(wxCommandEvent& evt)
{
    on_ask_button(evt);
}

} // namespace GUI
} // namespace Slic3r
