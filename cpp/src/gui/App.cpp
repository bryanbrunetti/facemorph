#include "App.h"
#include "MainFrame.h"

wxIMPLEMENT_APP(App);

bool App::OnInit() {
    auto* frame = new MainFrame("Image Transition");
    frame->Show(true);
    return true;
}