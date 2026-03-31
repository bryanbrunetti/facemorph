#include "MainFrame.h"
#include "core/pipeline.h"

#include <wx/statbox.h>
#include <wx/font.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>

// Locate the bundled landmark model: bundle Resources on macOS, exe dir elsewhere.
static wxString find_default_model() {
#ifdef __APPLE__
    {
        wxString resources = wxStandardPaths::Get().GetResourcesDir();
        wxString candidate = resources + wxFILE_SEP_PATH
                           + "models" + wxFILE_SEP_PATH
                           + "shape_predictor_68_face_landmarks.dat";
        if (wxFileExists(candidate)) return candidate;
    }
#endif
    wxFileName exe(wxStandardPaths::Get().GetExecutablePath());
    return exe.GetPath() + wxFILE_SEP_PATH
         + "models" + wxFILE_SEP_PATH
         + "shape_predictor_68_face_landmarks.dat";
}

MainFrame::MainFrame(const wxString& title)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(700, 650))
{
    Centre();

    auto* panel = new wxPanel(this);
    auto* main_sizer = new wxBoxSizer(wxVERTICAL);

    // ── Path pickers (3 rows, 2 columns) ────────────────────────────
    auto* path_grid = new wxFlexGridSizer(3, 2, 8, 8);
    path_grid->AddGrowableCol(1, 1);

    path_grid->Add(new wxStaticText(panel, wxID_ANY, "Input Folder:"),
                   0, wxALIGN_CENTER_VERTICAL);
    input_picker_ = new wxDirPickerCtrl(panel, wxID_ANY, wxEmptyString,
                                        "Select input image directory");
    path_grid->Add(input_picker_, 1, wxEXPAND);

    path_grid->Add(new wxStaticText(panel, wxID_ANY, "Output File:"),
                   0, wxALIGN_CENTER_VERTICAL);
    output_picker_ = new wxFilePickerCtrl(panel, wxID_ANY, wxEmptyString,
                                          "Select output video path",
                                          "MP4 files (*.mp4)|*.mp4",
                                          wxDefaultPosition, wxDefaultSize,
                                          wxFLP_SAVE | wxFLP_USE_TEXTCTRL | wxFLP_OVERWRITE_PROMPT);
    path_grid->Add(output_picker_, 1, wxEXPAND);

    path_grid->Add(new wxStaticText(panel, wxID_ANY, "Model File:"),
                   0, wxALIGN_CENTER_VERTICAL);
    wxString default_model = find_default_model();
    model_picker_ = new wxFilePickerCtrl(panel, wxID_ANY, default_model,
                                         "Select landmark model file",
                                         "DAT files (*.dat)|*.dat",
                                         wxDefaultPosition, wxDefaultSize,
                                         wxFLP_OPEN | wxFLP_USE_TEXTCTRL | wxFLP_FILE_MUST_EXIST);
    path_grid->Add(model_picker_, 1, wxEXPAND);

    main_sizer->Add(path_grid, 0, wxEXPAND | wxALL, 10);

    // ── Video Settings group ─────────────────────────────────────────
    auto* settings_box = new wxStaticBox(panel, wxID_ANY, "Video Settings");
    auto* settings_sizer = new wxStaticBoxSizer(settings_box, wxVERTICAL);

    auto* settings_grid = new wxFlexGridSizer(5, 2, 8, 8);
    settings_grid->AddGrowableCol(1, 1);

    // FPS
    settings_grid->Add(new wxStaticText(panel, wxID_ANY, "FPS:"),
                       0, wxALIGN_CENTER_VERTICAL);
    fps_spin_ = new wxSpinCtrlDouble(panel, wxID_ANY, "24",
                                     wxDefaultPosition, wxDefaultSize,
                                     wxSP_ARROW_KEYS, 1.0, 120.0, 24.0, 1.0);
    settings_grid->Add(fps_spin_, 1, wxEXPAND);

    // Hold Duration
    settings_grid->Add(new wxStaticText(panel, wxID_ANY, "Hold Duration (ms):"),
                       0, wxALIGN_CENTER_VERTICAL);
    duration_spin_ = new wxSpinCtrl(panel, wxID_ANY, "0",
                                    wxDefaultPosition, wxDefaultSize,
                                    wxSP_ARROW_KEYS, 0, 10000, 0);
    settings_grid->Add(duration_spin_, 1, wxEXPAND);

    // Morph Steps
    settings_grid->Add(new wxStaticText(panel, wxID_ANY, "Morph Steps:"),
                       0, wxALIGN_CENTER_VERTICAL);
    morph_spin_ = new wxSpinCtrl(panel, wxID_ANY, "30",
                                 wxDefaultPosition, wxDefaultSize,
                                 wxSP_ARROW_KEYS, 0, 240, 30);
    settings_grid->Add(morph_spin_, 1, wxEXPAND);

    // Frame Size (width x height)
    settings_grid->Add(new wxStaticText(panel, wxID_ANY, "Frame Size:"),
                       0, wxALIGN_CENTER_VERTICAL);

    auto* size_sizer = new wxBoxSizer(wxHORIZONTAL);
    width_spin_ = new wxSpinCtrl(panel, wxID_ANY, "0",
                                 wxDefaultPosition, wxSize(80, -1),
                                 wxSP_ARROW_KEYS, 0, 7680, 0);
    size_sizer->Add(width_spin_, 0, wxALIGN_CENTER_VERTICAL);
    size_sizer->Add(new wxStaticText(panel, wxID_ANY, " x "),
                    0, wxALIGN_CENTER_VERTICAL);
    height_spin_ = new wxSpinCtrl(panel, wxID_ANY, "0",
                                  wxDefaultPosition, wxSize(80, -1),
                                  wxSP_ARROW_KEYS, 0, 7680, 0);
    size_sizer->Add(height_spin_, 0, wxALIGN_CENTER_VERTICAL);
    size_sizer->Add(new wxStaticText(panel, wxID_ANY, "  (0 = auto)"),
                    0, wxALIGN_CENTER_VERTICAL | wxLEFT, 6);
    settings_grid->Add(size_sizer, 1, wxEXPAND);

    // Save Frames checkbox
    settings_grid->Add(0, 0); // empty label cell
    save_frames_cb_ = new wxCheckBox(panel, wxID_ANY, "Save individual frames");
    settings_grid->Add(save_frames_cb_, 0, wxALIGN_CENTER_VERTICAL);

    settings_sizer->Add(settings_grid, 0, wxEXPAND | wxALL, 6);
    main_sizer->Add(settings_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    // ── Generate button ──────────────────────────────────────────────
    generate_btn_ = new wxButton(panel, wxID_ANY, "Generate Video");
    main_sizer->Add(generate_btn_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    // ── Progress bar ─────────────────────────────────────────────────
    progress_bar_ = new wxGauge(panel, wxID_ANY, 100);
    main_sizer->Add(progress_bar_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    // ── Log area ─────────────────────────────────────────────────────
    log_text_ = new wxTextCtrl(panel, wxID_ANY, wxEmptyString,
                               wxDefaultPosition, wxDefaultSize,
                               wxTE_MULTILINE | wxTE_READONLY | wxHSCROLL);
    wxFont mono_font(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL,
                     wxFONTWEIGHT_NORMAL);
    log_text_->SetFont(mono_font);
    main_sizer->Add(log_text_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    panel->SetSizer(main_sizer);

    // ── Event binding ────────────────────────────────────────────────
    generate_btn_->Bind(wxEVT_BUTTON, &MainFrame::OnGenerate, this);
}

void MainFrame::OnGenerate(wxCommandEvent& /*event*/)
{
    // Prevent double-clicks while already running
    if (running_) return;

    // ── Validate required inputs ─────────────────────────────────────
    wxString input_dir = input_picker_->GetPath();
    if (input_dir.IsEmpty()) {
        wxMessageBox("Please select an input folder.", "Error",
                     wxOK | wxICON_ERROR, this);
        return;
    }
    wxString model_path = model_picker_->GetPath();
    if (model_path.IsEmpty()) {
        wxMessageBox("Please select the landmark model file.", "Error",
                     wxOK | wxICON_ERROR, this);
        return;
    }

    // ── Build pipeline settings ──────────────────────────────────────
    core::PipelineSettings settings;
    settings.input_dir   = std::filesystem::path(input_dir.ToStdString());
    settings.output_path = std::filesystem::path(
                               output_picker_->GetPath().ToStdString());
    settings.model_path  = std::filesystem::path(model_path.ToStdString());
    settings.fps         = fps_spin_->GetValue();
    settings.duration_ms = duration_spin_->GetValue();
    settings.morph_steps = morph_spin_->GetValue();
    settings.width       = width_spin_->GetValue();
    settings.height      = height_spin_->GetValue();
    settings.save_frames = save_frames_cb_->GetValue();

    // Default output path when none specified
    if (settings.output_path.empty()) {
        settings.output_path = settings.input_dir.parent_path() / "output.mp4";
    }

    // ── Prepare UI ───────────────────────────────────────────────────
    running_ = true;
    generate_btn_->Disable();
    progress_bar_->SetValue(0);
    log_text_->Clear();
    Log("Starting pipeline...");

    // ── Background thread ────────────────────────────────────────────
    std::thread([this, settings]() {
        try {
            core::Pipeline pipeline(settings.model_path.string());

            auto progress_cb = [this](int current, int total,
                                      const std::string& msg) {
                this->CallAfter([this, current, total, msg]() {
                    if (total > 0) {
                        progress_bar_->SetValue(current * 100 / total);
                    }
                    if (!msg.empty()) {
                        Log(msg);
                    }
                });
            };

            bool ok = pipeline.run(settings, progress_cb);
            std::string error_msg = ok ? std::string{}
                                       : pipeline.lastError();

            this->CallAfter([this, ok, error_msg]() {
                running_ = false;
                generate_btn_->Enable();
                progress_bar_->SetValue(ok ? 100 : 0);
                if (ok) {
                    Log("Done!");
                } else {
                    Log(wxString::Format("Pipeline failed: %s",
                                         error_msg));
                }
            });
        } catch (const std::exception& e) {
            std::string err = e.what();
            this->CallAfter([this, err]() {
                running_ = false;
                generate_btn_->Enable();
                progress_bar_->SetValue(0);
                Log(wxString::Format("Error: %s", err));
            });
        }
    }).detach();
}

void MainFrame::Log(const wxString& msg)
{
    log_text_->AppendText(msg + "\n");
}