#ifndef FILEMON_H
#define FILEMON_H

#include <filesystem>
#include <stack>
#include <map>
#include <string>

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Tree.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/Fl_Pixmap.H>
#include <FL/Fl_Box.H>

#include <FLE/Fle_Dock_Host.hpp>
#include <FLE/Fle_Dock_Group.hpp>
#include <FLE/Fle_Listview.hpp>
#include <FLE/Fle_Toolbar.hpp>
#include <FLE/Fle_Flat_Button.hpp>

struct inotify_event;

class Filemon : public Fl_Double_Window
{
    Fle_Dock_Host* m_dockHost;
    Fle_Listview*  m_listview;
    Fl_Tree*       m_tree;
    Fl_Output*     m_address;
    Fl_Box*        m_status;

    Fle_Dock_Group* m_treeDockGroup;
    Fle_Dock_Group* m_menuGroup;
    Fle_Dock_Group* m_toolbarGroup;
    Fle_Dock_Group* m_addressGroup;
    Fle_Dock_Group* m_statusGroup;

    std::filesystem::path m_currentPath;
    
    int m_filesInDir;

    std::stack<std::filesystem::path> m_history;

    Fl_Pixmap* m_folder16;

    int m_inotifyFd;
    std::map<int, std::filesystem::path> m_watchMap;

    static void listview_cb(Fl_Widget* w, void* data);
    static void tree_cb(Fl_Widget* w, void* data);
    static void menu_cb(Fl_Widget* w, void* data);
    static void tool_cb(Fl_Widget* w, void* data);

    void construct_menu(Fl_Menu_Bar* menu);
    void construct_toolbar(Fle_Toolbar* toolbar);
    void tree_populate_dir(Fl_Tree_Item* directory, std::filesystem::path path);
    void tree_update_branch(inotify_event* event);

    bool navigate_to_dir(const std::filesystem::path& path);
public:
    Filemon(int W, int H, const char* l);

    int run();
};

#endif