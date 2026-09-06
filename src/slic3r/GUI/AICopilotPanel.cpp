#include "AICopilotPanel.hpp"
#include "I18N.hpp"
#include "GUI_App.hpp"
#include "Plater.hpp"
#include "Tab.hpp"
#include "../Utils/Http.hpp"
#include "libslic3r/Print.hpp"
#include "libslic3r/PrintBase.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/Preset.hpp"
#include "libslic3r/AppConfig.hpp"
#include <wx/dataview.h>
#include <wx/textdlg.h>
#include <wx/msgdlg.h>
#include <wx/datetime.h>
#include <nlohmann/json.hpp>
#include <boost/log/trivial.hpp>
#include <sstream>

namespace Slic3r {
namespace GUI {

static Preset::Type determine_family(const std::string& key, PresetBundle* bundle)
{
    if (!bundle) return Preset::TYPE_INVALID;

    // Check filament options
    if (bundle->filaments.get_selected_preset().config.has(key) ||
        bundle->filaments.default_preset().config.has(key)) {
        return Preset::TYPE_FILAMENT;
    }
    const auto& fil_opts = Preset::filament_options();
    if (std::find(fil_opts.begin(), fil_opts.end(), key) != fil_opts.end()) {
        return Preset::TYPE_FILAMENT;
    }

    // Check print options
    if (bundle->prints.get_selected_preset().config.has(key) ||
        bundle->prints.default_preset().config.has(key)) {
        return Preset::TYPE_PRINT;
    }
    const auto& prt_opts = Preset::print_options();
    if (std::find(prt_opts.begin(), prt_opts.end(), key) != prt_opts.end()) {
        return Preset::TYPE_PRINT;
    }

    // Check printer options
    if (bundle->printers.get_selected_preset().config.has(key) ||
        bundle->printers.default_preset().config.has(key)) {
        return Preset::TYPE_PRINTER;
    }
    const auto& ptr_opts = Preset::printer_options();
    if (std::find(ptr_opts.begin(), ptr_opts.end(), key) != ptr_opts.end()) {
        return Preset::TYPE_PRINTER;
    }

    return Preset::TYPE_INVALID;
}

static std::string get_current_option_value(const std::string& key, Preset::Type family, PresetBundle* bundle)
{
    if (!bundle) return "-";
    const PresetCollection* col = nullptr;
    switch (family) {
        case Preset::TYPE_PRINT:    col = &bundle->prints; break;
        case Preset::TYPE_FILAMENT: col = &bundle->filaments; break;
        case Preset::TYPE_PRINTER:  col = &bundle->printers; break;
        default: return "-";
    }

    if (col->get_selected_preset().config.has(key)) {
        return col->get_selected_preset().config.opt_serialize(key);
    }
    if (col->get_edited_preset().config.has(key)) {
        return col->get_edited_preset().config.opt_serialize(key);
    }
    if (col->default_preset().config.has(key)) {
        return col->default_preset().config.opt_serialize(key);
    }
    return "-";
}

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

    // Diff Panel (initially hidden)
    m_diff_panel = new wxPanel(this, wxID_ANY);
    auto* diff_sizer = new wxBoxSizer(wxVERTICAL);

    m_diff_label = new wxStaticText(m_diff_panel, wxID_ANY, _L("Cambios sugeridos por el copiloto:"));
    wxFont font_bold = m_diff_label->GetFont();
    font_bold.SetWeight(wxFONTWEIGHT_BOLD);
    m_diff_label->SetFont(font_bold);
    diff_sizer->Add(m_diff_label, 0, wxEXPAND | wxBOTTOM, FromDIP(3));

    m_diff_list = new wxDataViewListCtrl(m_diff_panel, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(85)), wxDV_ROW_LINES | wxDV_HORIZ_RULES);
    m_diff_list->AppendTextColumn(_L("Clave"), wxDATAVIEW_CELL_INERT, FromDIP(120), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_diff_list->AppendTextColumn(_L("Actual"), wxDATAVIEW_CELL_INERT, FromDIP(60), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_diff_list->AppendTextColumn(_L("Propuesto"), wxDATAVIEW_CELL_INERT, FromDIP(70), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    diff_sizer->Add(m_diff_list, 1, wxEXPAND | wxBOTTOM, FromDIP(4));

    auto* btn_diff_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_btn_apply = new wxButton(m_diff_panel, wxID_ANY, _L("Aplicar cambios"));
    m_btn_discard = new wxButton(m_diff_panel, wxID_ANY, _L("Descartar"));
    btn_diff_sizer->Add(m_btn_apply, 1, wxRIGHT, FromDIP(4));
    btn_diff_sizer->Add(m_btn_discard, 0);
    diff_sizer->Add(btn_diff_sizer, 0, wxEXPAND);

    m_diff_panel->SetSizer(diff_sizer);
    m_diff_panel->Hide();

    main_sizer->Add(m_diff_panel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(4));

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
    m_btn_apply->Bind(wxEVT_BUTTON, &AICopilotPanel::on_apply_button, this);
    m_btn_discard->Bind(wxEVT_BUTTON, &AICopilotPanel::on_discard_button, this);

    wxGetApp().UpdateDarkUI(this);

    // Initial greeting
    append_message("Copiloto", _L("Hola Eduardo. Estoy listo para ayudarte a calibrar y optimizar tus impresiones."));
}

void AICopilotPanel::append_message(const wxString& sender, const wxString& text)
{
    if (!m_chat_log)
        return;

    wxString formatted = "[" + sender + "]: " + text + "\n\n";
    m_chat_log->AppendText(formatted);
}

void AICopilotPanel::clear_messages()
{
    if (m_chat_log)
        m_chat_log->Clear();
}

void AICopilotPanel::show_proposed_diff(const std::map<std::string, std::string>& proposed_changes)
{
    m_pending_changes = proposed_changes;
    if (m_diff_list) {
        m_diff_list->DeleteAllItems();
    }

    if (m_pending_changes.empty()) {
        if (m_diff_panel && m_diff_panel->IsShown()) {
            m_diff_panel->Hide();
            Layout();
            if (GetParent()) GetParent()->Layout();
        }
        return;
    }

    auto* bundle = wxGetApp().preset_bundle;

    for (const auto& kv : m_pending_changes) {
        const std::string& key = kv.first;
        const std::string& proposed = kv.second;
        Preset::Type family = determine_family(key, bundle);
        std::string current_val = get_current_option_value(key, family, bundle);

        wxVector<wxVariant> row;
        row.push_back(wxVariant(wxString::FromUTF8(key.c_str())));
        row.push_back(wxVariant(wxString::FromUTF8(current_val.c_str())));
        row.push_back(wxVariant(wxString::FromUTF8(proposed.c_str())));
        m_diff_list->AppendItem(row);
    }

    if (m_diff_panel) {
        m_diff_panel->Show(true);
        Layout();
        if (GetParent()) GetParent()->Layout();
    }
}

void AICopilotPanel::clear_proposed_diff()
{
    m_pending_changes.clear();
    if (m_diff_list) {
        m_diff_list->DeleteAllItems();
    }
    if (m_diff_panel && m_diff_panel->IsShown()) {
        m_diff_panel->Hide();
        Layout();
        if (GetParent()) GetParent()->Layout();
    }
}

void AICopilotPanel::on_apply_button(wxCommandEvent& evt)
{
    apply_proposed_changes();
}

void AICopilotPanel::on_discard_button(wxCommandEvent& evt)
{
    clear_proposed_diff();
    append_message("Copiloto", _L("Cambios propuestos descartados."));
}

bool AICopilotPanel::apply_proposed_changes()
{
    if (m_pending_changes.empty())
        return false;

    auto* bundle = wxGetApp().preset_bundle;
    if (!bundle) {
        wxMessageBox(_L("Error: Preset bundle no disponible."), _L("Error"), wxOK | wxICON_ERROR, this);
        return false;
    }

    // Group pending changes by family
    std::map<Preset::Type, std::vector<std::pair<std::string, std::string>>> grouped;
    for (const auto& kv : m_pending_changes) {
        Preset::Type fam = determine_family(kv.first, bundle);
        if (fam != Preset::TYPE_INVALID) {
            grouped[fam].push_back(kv);
        } else {
            append_message("Copiloto", _L("Aviso: el parámetro \"") + wxString::FromUTF8(kv.first.c_str()) + _L("\" no pertenece a ningún preset conocido y será omitido."));
        }
    }

    if (grouped.empty()) {
        wxMessageBox(_L("No hay parámetros válidos para aplicar."), _L("Aviso"), wxOK | wxICON_WARNING, this);
        clear_proposed_diff();
        return false;
    }

    std::vector<std::string> applied_summary;

    for (const auto& group : grouped) {
        Preset::Type family = group.first;
        const auto& items = group.second;

        PresetCollection* collection = nullptr;
        wxString family_label;
        switch (family) {
            case Preset::TYPE_PRINT:
                collection = &bundle->prints;
                family_label = _L("Proceso (Print)");
                break;
            case Preset::TYPE_FILAMENT:
                collection = &bundle->filaments;
                family_label = _L("Filamento (Filament)");
                break;
            case Preset::TYPE_PRINTER:
                collection = &bundle->printers;
                family_label = _L("Impresora (Printer)");
                break;
            default:
                continue;
        }

        const Preset& orig_preset = collection->get_selected_preset();
        std::string orig_name = orig_preset.name;

        wxString today_str = wxDateTime::Now().Format("%Y-%m-%d");
        wxString default_name = wxString::FromUTF8(orig_name.c_str()) + " - AI tuned " + today_str;

        wxString prompt_text = _L("Guardar preset derivado de ") + family_label + ":\n" +
                               _L("(Heredará de \"") + wxString::FromUTF8(orig_name.c_str()) +
                               _L("\" sin modificar el original en disco)");

        wxTextEntryDialog dlg(this, prompt_text, _L("Aplicar ajustes del Copiloto"), default_name);

        if (dlg.ShowModal() != wxID_OK) {
            append_message("Copiloto", _L("Aplicación de cambios para ") + family_label + _L(" cancelada por el usuario."));
            continue;
        }

        wxString chosen_name = dlg.GetValue().Trim().Trim(false);
        if (chosen_name.IsEmpty()) {
            wxMessageBox(_L("El nombre del preset no puede estar vacío."), _L("Error"), wxOK | wxICON_ERROR, this);
            continue;
        }

        if (orig_preset.is_system && chosen_name.ToStdString() == orig_name) {
            wxMessageBox(_L("No se puede sobreescribir un perfil del sistema. Por favor elija un nombre diferente."), _L("Aviso"), wxOK | wxICON_WARNING, this);
            continue;
        }

        // Clone current preset and apply overrides in memory
        Preset temp_preset = orig_preset;
        for (const auto& item : items) {
            try {
                temp_preset.config.set_deserialize_strict(item.first, item.second);
            } catch (const std::exception& e) {
                BOOST_LOG_TRIVIAL(error) << "AI Copilot: Error set_deserialize_strict on " << item.first << " = " << item.second << ": " << e.what();
            }
        }

        // Save preset with detach=false to maintain inheritance
        collection->save_current_preset(chosen_name.ToUTF8().data(), false, false, &temp_preset);

        Preset* saved_preset = collection->find_preset(chosen_name.ToUTF8().data(), false, true);
        if (saved_preset) {
            saved_preset->save_info();
        }

        bundle->update_compatible(PresetSelectCompatibleType::Never);

        Tab* tab = wxGetApp().get_tab(family);
        if (tab) {
            tab->update_tab_ui(true);
        }

        if (family == Preset::TYPE_FILAMENT) {
            wxGetApp().plater()->sidebar().update_presets_from_to(family, orig_name, chosen_name.ToUTF8().data());
        }
        wxGetApp().plater()->sidebar().update_presets(family);
        wxGetApp().plater()->sidebar().update_all_preset_comboboxes();

        applied_summary.push_back(std::string(family_label.ToUTF8().data()) + ": \"" + chosen_name.ToStdString() + "\" (hereda de \"" + orig_name + "\")");
    }

    if (wxGetApp().app_config) {
        bundle->export_selections(*wxGetApp().app_config);
    }

    if (!applied_summary.empty()) {
        wxString msg = _L("Cambios aplicados exitosamente. Presets derivados creados y activos:");
        for (const auto& s : applied_summary) {
            msg += "\n • " + wxString::FromUTF8(s.c_str());
        }
        append_message("Copiloto", msg);
    }

    clear_proposed_diff();
    return true;
}

void AICopilotPanel::on_ask_button(wxCommandEvent& evt)
{
    wxString text = m_input_text->GetValue().Trim().Trim(false);
    if (text.IsEmpty())
        return;

    append_message("Eduardo", text);
    m_input_text->Clear();

    if (text == "/test_apply") {
        append_message("Copiloto", _L("Modo de prueba activado: se cargaron cambios propuestos de prueba (Filamento y Proceso) sin consultar al brain_service."));
        show_proposed_diff({
            {"hot_plate_temp", "100"},
            {"brim_width", "8.0"}
        });
        return;
    }

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

    // Context from Presets
    auto* preset_bundle = wxGetApp().preset_bundle;
    if (preset_bundle) {
        payload["printer_preset"] = preset_bundle->printers.get_selected_preset().name;
        payload["filament_preset"] = preset_bundle->filaments.get_selected_preset().name;
        payload["process_preset"] = preset_bundle->prints.get_selected_preset().name;

        nlohmann::json diff_json = nlohmann::json::object();
        for (const auto& key : preset_bundle->prints.current_dirty_options()) {
            if (preset_bundle->prints.get_edited_preset().config.has(key)) {
                diff_json[key] = preset_bundle->prints.get_edited_preset().config.opt_serialize(key);
            }
        }
        for (const auto& key : preset_bundle->filaments.current_dirty_options()) {
            if (preset_bundle->filaments.get_edited_preset().config.has(key)) {
                diff_json[key] = preset_bundle->filaments.get_edited_preset().config.opt_serialize(key);
            }
        }
        payload["config_diff_from_system"] = diff_json;
    }

    // Context from Plater and Print
    auto* plater = wxGetApp().plater();
    nlohmann::json warnings_json = nlohmann::json::array();
    nlohmann::json stats_json = nlohmann::json::object();

    if (plater) {
        try {
            std::vector<StringObjectException> warnings;
            StringObjectException err = plater->fff_print().validate(&warnings);
            if (!err.string.empty()) {
                warnings_json.push_back({
                    {"type", static_cast<int>(err.type)},
                    {"message", err.string},
                    {"opt_key", err.opt_key}
                });
            }
            for (const auto& w : warnings) {
                warnings_json.push_back({
                    {"type", static_cast<int>(w.type)},
                    {"message", w.string},
                    {"opt_key", w.opt_key}
                });
            }

            const auto& stats = plater->fff_print().print_statistics();
            stats_json["estimated_normal_print_time"] = stats.estimated_normal_print_time;
            stats_json["total_used_filament"] = stats.total_used_filament;
            stats_json["total_weight"] = stats.total_weight;
            stats_json["total_cost"] = stats.total_cost;
        } catch (...) {
            // Guard against uninitialized print
        }
    }
    payload["validation_warnings"] = warnings_json;
    payload["print_statistics"] = stats_json;

    std::string post_body = payload.dump();

    if (m_current_request) {
        m_current_request->cancel();
        m_current_request.reset();
    }

    auto http = Slic3r::Http::post("http://127.0.0.1:8787/diagnose");
    m_current_request = http.header("Content-Type", "application/json")
        .set_post_body(post_body)
        .timeout_connect(5)
        .timeout_max(40)
        .on_complete([this, alive = m_alive](std::string body, unsigned http_status) {
            wxGetApp().CallAfter([this, alive, body, http_status]() {
                if (!*alive) return;
                if (m_btn_ask) m_btn_ask->Enable();
                try {
                    auto j = nlohmann::json::parse(body);
                    std::string diag = j.value("diagnosis_text", "");
                    std::string conf = j.value("confidence", "");
                    if (diag.empty())
                        diag = j.value("reply", body);

                    wxString sender = "Copiloto";
                    if (conf == "cloud") {
                        sender = "Copiloto (Gemini Cloud)";
                    } else if (conf == "local") {
                        sender = "Copiloto (Local)";
                    }

                    wxString full_msg = wxString::FromUTF8(diag.c_str());

                    std::map<std::string, std::string> proposed_map;
                    if (j.contains("proposed_changes") && j["proposed_changes"].is_object() && !j["proposed_changes"].empty()) {
                        full_msg += "\n\n" + _L("Parámetros sugeridos:");
                        for (auto& item : j["proposed_changes"].items()) {
                            std::string val_str;
                            if (item.value().is_string()) {
                                val_str = item.value().get<std::string>();
                            } else if (item.value().is_number_integer()) {
                                val_str = std::to_string(item.value().get<int64_t>());
                            } else if (item.value().is_number_float()) {
                                std::ostringstream ss;
                                ss << item.value().get<double>();
                                val_str = ss.str();
                            } else if (item.value().is_boolean()) {
                                val_str = item.value().get<bool>() ? "1" : "0";
                            } else {
                                val_str = item.value().dump();
                            }
                            proposed_map[item.key()] = val_str;
                            full_msg += "\n • " + wxString::FromUTF8(item.key().c_str()) + ": " + wxString::FromUTF8(val_str.c_str());
                        }
                    }

                    append_message(sender, full_msg);
                    show_proposed_diff(proposed_map);
                } catch (...) {
                    append_message("Copiloto", wxString::FromUTF8(body.c_str()));
                }
            });
        })
        .on_error([this, alive = m_alive](std::string body, std::string error, unsigned http_status) {
            wxGetApp().CallAfter([this, alive, error, http_status]() {
                if (!*alive) return;
                if (m_btn_ask) m_btn_ask->Enable();
                wxString err_msg = _L("Error al consultar el brain_service (") +
                                   wxString::FromUTF8(error.c_str()) +
                                   ", código " + wxString::Format("%u", http_status) + ")";
                append_message("Copiloto", err_msg);
            });
        })
        .perform();
}

} // namespace GUI
} // namespace Slic3r
