#pragma once

#include <wx/wx.h>
#include <wx/filepicker.h>
#include <wx/spinctrl.h>
#include <thread>
#include <atomic>

class MainFrame : public wxFrame {
public:
    explicit MainFrame(const wxString& title);

private:
    void OnGenerate(wxCommandEvent& event);
    void Log(const wxString& msg);

    wxDirPickerCtrl*    input_picker_    = nullptr;
    wxFilePickerCtrl*   output_picker_   = nullptr;
    wxFilePickerCtrl*   model_picker_    = nullptr;
    wxSpinCtrlDouble*   fps_spin_        = nullptr;
    wxSpinCtrl*         duration_spin_   = nullptr;
    wxSpinCtrl*         morph_spin_      = nullptr;
    wxSpinCtrl*         width_spin_      = nullptr;
    wxSpinCtrl*         height_spin_     = nullptr;
    wxCheckBox*         save_frames_cb_  = nullptr;
    wxButton*           generate_btn_    = nullptr;
    wxGauge*            progress_bar_    = nullptr;
    wxTextCtrl*         log_text_        = nullptr;

    std::atomic<bool>   running_{false};
};