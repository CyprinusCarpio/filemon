#include "Preferences.hpp"

#include <FL/Fl_Tabs.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Check_Button.H>

#include <FLE/Fle_Schemes.hpp>
#include <FLE/Fle_Colors.hpp>

void preferences_close_cb(Fl_Widget* w, void* d)
{
    Fl_Double_Window* wnd = (Fl_Double_Window*)w->window();
    if(!wnd) wnd = (Fl_Double_Window*)w;
    
    FilemonPreferences* prefs = (FilemonPreferences*)d;
    prefs->m_scheme = fle_get_scheme();
    prefs->m_colors = fle_get_colors();

    wnd->hide();
}
void terminal_emulator_cb(Fl_Widget* w, void* d)
{
    FilemonPreferences* prefs = (FilemonPreferences*)d;
    prefs->m_terminalEmulator = ((Fl_Input*)w)->value();
}
void show_hidden_files_cb(Fl_Widget* w, void* d)
{
    FilemonPreferences* prefs = (FilemonPreferences*)d;
    prefs->m_showHiddenFiles = ((Fl_Check_Button*)w)->value();
}

void show_preferences_wnd(FilemonPreferences* prefs)
{
    Fl_Double_Window wnd(400, 400, "Filemon Preferences");
    wnd.set_modal();
    wnd.callback(preferences_close_cb, prefs);

    Fl_Button* close = new Fl_Button(325, 370, 70, 26, "Close");
    close->callback(preferences_close_cb, prefs);

    Fl_Tabs* tabs = new Fl_Tabs(0, 0, 400, 360);

    Fl_Group* generic = new Fl_Group(10, 26, 380, 348, "Generic");
        Fl_Input* terminalEmulatorInput = new Fl_Input(150, 50, 100, 26, "Terminal Emulator");
        terminalEmulatorInput->value(prefs->m_terminalEmulator.c_str());
        terminalEmulatorInput->callback(terminal_emulator_cb, prefs);

        Fl_Box* optionsLabel = new Fl_Box(80, 90, 70, 26, "Options:");
        optionsLabel->align(FL_ALIGN_RIGHT | FL_ALIGN_INSIDE);

        Fl_Check_Button* showHidden = new Fl_Check_Button(150, 90, 150, 26, "Show hidden files");
        showHidden->value(prefs->m_showHiddenFiles ? 1 : 0);
        showHidden->callback(show_hidden_files_cb, prefs);
    generic->end();

    Fl_Group* appearance = new Fl_Group(10, 26, 380, 348, "Appearance");
        Fle_Scheme_Choice* schemeChoice = new Fle_Scheme_Choice(70, 50, 100, 26, "Scheme");
        schemeChoice->value(schemeChoice->find_index(prefs->m_scheme.c_str()));

        Fle_Colors_Choice* colorsChoice = new Fle_Colors_Choice(70, 80, 100, 26, "Colors");
        colorsChoice->value(colorsChoice->find_index(prefs->m_colors.c_str()));
    appearance->end();

    tabs->end();

    wnd.end();

    wnd.hotspot(wnd);
    wnd.show();

    while (wnd.shown()) Fl::wait();
}