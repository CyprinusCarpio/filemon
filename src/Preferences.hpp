#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <string>
#include <FL/Fl_Double_Window.H>

struct FilemonPreferences
{
    bool m_showHiddenFiles = false;
    std::string m_terminalEmulator = "xterm";
    std::string m_scheme = "base";
    std::string m_colors = "default";
};

void show_preferences_wnd(FilemonPreferences* prefs);
#endif