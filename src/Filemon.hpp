#ifndef FILEMON_H
#define FILEMON_H

#include <filesystem>
#include <stack>
#include <map>
#include <string>

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Tree.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Pixmap.H>
#include <FL/Fl_Check_Button.H>

#include <FLE/Fle_Dock_Host.hpp>
#include <FLE/Fle_Dock_Group.hpp>
#include <FLE/Fle_Listview.hpp>
#include <FLE/Fle_Toolbar.hpp>
#include <FLE/Fle_Flat_Button.hpp>

#include "Preferences.hpp"

#include <glib-2.0/gio/gio.h>

class Flat_Menu_Bar;
class Icon_Listview;
class Filemon : public Fl_Double_Window
{
    friend class Flat_Menu_Bar;
    friend class Icon_Listview;
    
    Fle_Dock_Host*   m_dockHost;
    Icon_Listview*   m_listview;
    Fl_Tree*         m_tree;
    Fl_Tree*         m_volumesTree;
    Fl_Output*       m_address;
    Fl_Box*          m_status;
    Fl_Input*        m_searchFor;
    Fl_Input*        m_excludeFromSearch;
    Fl_Check_Button* m_caseSensitive;
    Fl_Check_Button* m_regex;

    Fle_Dock_Group* m_treeDockGroup;
    Fle_Dock_Group* m_menuGroup;
    Fle_Dock_Group* m_toolbarGroup;
    Fle_Dock_Group* m_addressGroup;
    Fle_Dock_Group* m_statusGroup;
    Fle_Dock_Group* m_searchGroup;
    Fle_Dock_Group* m_volumesGroup;

    Fl_Double_Window* m_aboutWindow;
    Fl_Pixmap*        m_filemonLogo;

    std::filesystem::path m_currentPath;
    
    int m_filesInDir;
    FilemonPreferences m_preferences;

    std::stack<std::filesystem::path> m_history;

    Fl_Pixmap* m_folder16;
    Fl_Pixmap* m_device16;
    Fl_Pixmap* m_toolbarBack24;
    Fl_Pixmap* m_toolbarForward24;
    Fl_Pixmap* m_toolbarUp24;
    Fl_Pixmap* m_toolbarSearch24;
    Fl_Pixmap* m_toolbarFolders24;
    Fl_Pixmap* m_toolbarDevices24;
    Fl_Pixmap* m_toolbarHome24;

    std::map<std::filesystem::path, GFileMonitor*> m_monitors;

    static void listview_cb(Fl_Widget* w, void* data);
    static void tree_cb(Fl_Widget* w, void* data);
    static void menu_cb(Fl_Widget* w, void* data);
    static void tool_cb(Fl_Widget* w, void* data);
    static void popup_cb(Fl_Widget* w, void* data);
    static void search_cb(Fl_Widget* w, void* data);
    static void volumes_tree_cb(Fl_Widget* w, void* data);

    static void dir_watch_cb(GFileMonitor* monitor, GFile* file, GFile* other_file, GFileMonitorEvent event_type, gpointer user_data);
    static void volume_added_cb(GVolumeMonitor* monitor, GVolume* volume, gpointer user_data);
    static void volume_removed_cb(GVolumeMonitor* monitor, GVolume* volume, gpointer user_data);
    static void volume_changed_cb(GVolumeMonitor* monitor, GVolume* volume, gpointer user_data);
    static void mount_added_cb(GVolumeMonitor* monitor, GMount* mount, gpointer user_data);
    static void mount_removed_cb(GVolumeMonitor* monitor, GMount* mount, gpointer user_data);
    static void mount_finished_cb(GObject* source_object, GAsyncResult* res, gpointer data);
    
    void construct_menu(Fl_Menu_Bar* menu);
    void construct_toolbar(Fle_Toolbar* toolbar);
    void construct_search_pane(Fl_Group* pane);
    void tree_populate_dir(Fl_Tree_Item* directory, std::filesystem::path path);
    void monitor_directory(const std::filesystem::path& path);

    bool navigate_to_dir(const std::filesystem::path& path);
    bool open_file(const std::filesystem::path& path);
    bool open_terminal();
    bool filter_file(const std::filesystem::path& path);
    void popup_menu();

    void new_file();
    void new_folder();
    void rename_selected();
    void select_all(bool select);
    void copy_selected_files(bool cut);
    void paste_files();
    void trash_file(const std::filesystem::path& path);
    void show_hidden_files(bool show);
public:
    Filemon(int W, int H, const char* l);

    int run();
};

#endif