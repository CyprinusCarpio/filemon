#include "Filemon.hpp"
#include "File.hpp"
#include "Util.hpp"
#include "../icons/folder16.xpm"
#include "../icons/filemon.xpm"

#include <FL/Fl_Text_Display.H>

#include <iostream>
#include <sys/types.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef FLTK_USE_WAYLAND
#include <FL/platform.H>
#endif

enum MenuIDS
{
    MENU_FILE_NEW_FILE,
    MENU_FILE_NEW_FOLDER,
    MENU_FILE_FILE_PROPERTIES,
    MENU_FILE_FOLDER_PROPERTIES,
    MENU_FILE_OPEN_TERMINAL,
    MENU_FILE_QUIT,

    MENU_EDIT_CUT,
    MENU_EDIT_COPY,
    MENU_EDIT_PASTE,
    MENU_EDIT_RENAME,
    MENU_EDIT_TRASH,
    MENU_EDIT_SELECT_ALL,
    MENU_EDIT_DESELECT_ALL,
    MENU_EDIT_PREFERENCES,

    MENU_VIEW,
    MENU_VIEW_STATUS,
    MENU_VIEW_TREE,
    MENU_VIEW_SHOW_HIDDEN,
    MENU_VIEW_BIGICONS,
    MENU_VIEW_SMALLICONS,
    MENU_VIEW_LIST,
    MENU_VIEW_DETAILS,
    MENU_VIEW_DETAILS_HIGHLIGHT_NONE,
    MENU_VIEW_DETAILS_HIGHLIGHT_LINE,
    MENU_VIEW_DETAILS_HIGHLIGHT_BOX,

    MENU_HELP_SHORTCUTS,
    MENU_HELP_ABOUT,
};

enum DockIDS
{
    DOCK_TREE,
    DOCK_MENU,
    DOCK_TOOLBAR,
    DOCK_LOCATION,
    DOCK_STATUS,
};

void about_wnd_close_callback(Fl_Widget* w, void* d)
{
    w->window()->hide();
}

void about_wnd_github_callback(Fl_Widget* w, void* d)
{
    g_app_info_launch_default_for_uri("https://github.com/CyprinusCarpio/filemon", nullptr, nullptr);
}

class FlatMenuBar : public Fl_Menu_Bar
{
public:
    FlatMenuBar(int X, int Y, int W, int H, const char* l) : Fl_Menu_Bar(X, Y, W, H, l) {};

protected:
    void draw() override
    {
        fl_push_clip(x() + 3, y() + 3, w() - 6, h() - 6);
        Fl_Menu_Bar::draw();
        fl_pop_clip();
    }
};

Filemon::Filemon(int W, int H, const char* l): Fl_Double_Window(W, H, l)
{
    m_dockHost = new Fle_Dock_Host(0, 0, W, H, l, FLE_DOCK_ALLDIRS);
    resizable(m_dockHost);
    end();

    m_listview = new Fle_Listview(0, 0, 0, 0, "");
    m_listview->callback(listview_cb, (void*)this);
    m_listview->set_property_widths({80, 80, 80, 100, 120});
    m_listview->set_property_order({FILE_PROPERTY_SIZE, FILE_PROPERTY_OWNER, FILE_PROPERTY_GROUP, FILE_PROPERTY_PERMISSIONS, FILE_PROPERTY_MODIFIED});
    m_listview->add_property_name("Size");
    m_listview->add_property_name("Owner");
    m_listview->add_property_name("Group");
    m_listview->add_property_name("Permission");
    m_listview->add_property_name("Modified");
    m_dockHost->add_work_widget(m_listview);
    
    m_treeDockGroup = new Fle_Dock_Group(m_dockHost, DOCK_TREE, "Folders", FLE_DOCK_DETACHABLE | FLE_DOCK_FLEXIBLE, FLE_DOCK_LEFT, FLE_DOCK_RIGHT, 75, 180, false);
    m_tree = new Fl_Tree(0, 0, 0, 0, "");
    m_tree->callback(tree_cb, (void*)this);
    m_tree->showroot(false);
    m_tree->root_label("/");
    m_folder16 = new Fl_Pixmap(folder16_xpm);
    m_tree->usericon(m_folder16);
    m_treeDockGroup->add_band_widget(m_tree);

    m_menuGroup = new Fle_Dock_Group(m_dockHost, DOCK_MENU, "Menu", FLE_DOCK_NO_HOR_LABEL, FLE_DOCK_TOP, FLE_DOCK_BOTTOM, 200, 26, false);
    FlatMenuBar* menu = new FlatMenuBar(0, 0, 0, 0, "");
    m_menuGroup->add_band_widget(menu, 1, 1, 1, 1);
    construct_menu(menu);

    m_toolbarGroup = new Fle_Dock_Group(m_dockHost, DOCK_TOOLBAR, "Toolbar", FLE_DOCK_FLEXIBLE | FLE_DOCK_NO_HOR_LABEL, FLE_DOCK_TOP, FLE_DOCK_BOTTOM, 135, 32, true);
    Fle_Toolbar* toolbar = new Fle_Toolbar(0, 0, 0, 0, Fl_Flex::HORIZONTAL);
    m_toolbarGroup->add_band_widget(toolbar);
    construct_toolbar(toolbar);

    m_addressGroup = new Fle_Dock_Group(m_dockHost, DOCK_LOCATION, "Location", FLE_DOCK_FLEXIBLE, FLE_DOCK_TOP, FLE_DOCK_BOTTOM, 200, 32, false);
    m_address = new Fl_Output(0, 0, 0, 0, "");
    m_addressGroup->add_band_widget(m_address, 1, 1, 1, 1);

    m_statusGroup = new Fle_Dock_Group(m_dockHost, DOCK_STATUS, "Status", FLE_DOCK_FLEXIBLE | FLE_DOCK_NO_HOR_LABEL, FLE_DOCK_BOTTOM, FLE_DOCK_BOTTOM, 200, 32, false);
    m_status = new Fl_Box(0, 0, 0, 0, "");
    m_status->box(FL_THIN_DOWN_BOX);
    m_status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    m_status->copy_label("Ready");
    m_statusGroup->add_band_widget(m_status, 2, 4, 2, 4);

    tree_populate_dir(m_tree->root(), "/");

    m_terminalEmulator = "xterm";

    m_filemonLogo = new Fl_Pixmap(filemon_xpm);
    m_aboutWindow = new Fl_Double_Window(512, 256, "About Filemon");
    //m_aboutWindow->set_modal();
    Fl_Box* logo = new Fl_Box(0, 0, 256, 256);
    logo->image(m_filemonLogo);
    Fl_Text_Display* text = new Fl_Text_Display(256, 0, 256, 215);
    Fl_Text_Buffer* buffer = new Fl_Text_Buffer();
    buffer->text(
        "Filemon 0.1.0\n"
        "A lightweight X11/Wayland file browser for Linux based on the FLTK toolkit."
    );
    text->buffer(buffer);
    text->wrap_mode(2, 0);

    Fl_Button* githubButton = new Fl_Button(350, 222, 70, 26, "GitHub");
    githubButton->callback(about_wnd_github_callback);
    Fl_Button* closeButton = new Fl_Button(430, 222, 70, 26, "Close");
    closeButton->callback(about_wnd_close_callback);
}

void Filemon::listview_cb(Fl_Widget* w, void* data)
{
    Filemon* filemon = (Filemon*)data;

    Fle_Listview_Reason reason = (Fle_Listview_Reason)Fl::callback_reason();
    ListviewFile* file = (ListviewFile*)filemon->m_listview->get_callback_item();
    if(reason == FLE_LISTVIEW_REASON_RESELECTED && Fl::event_clicks())
    {
        if(file->is_folder())
        {
            filemon->navigate_to_dir(file->get_path());
        }
        else
        {
            filemon->open_file(file->get_path());
        }
    }
    if(reason == FLE_LISTVIEW_REASON_DESELECTED || reason == FLE_LISTVIEW_REASON_SELECTED || reason == FLE_LISTVIEW_REASON_RESELECTED)
    {
        int selectedCount = filemon->m_listview->get_selected_count();
        if(selectedCount > 0)
        {
            std::vector<int> selectedItems = filemon->m_listview->get_selected();
            size_t totalSizeBytes = 0;
            for(int i = 0; i < selectedCount; i++)
            {
                ListviewFile* item = (ListviewFile*)filemon->m_listview->get_item(selectedItems[i]);
                totalSizeBytes += item->size() != -1 ? item->size() : 0;
            }
            std::string totalSize = file_size_to_string(totalSizeBytes);
            std::string status = "Selected: " + std::to_string(selectedCount) + " (File size: " + totalSize + ")";
            filemon->m_status->copy_label(status.c_str());
        }
        else
        {
            std::string status = "Elements: " + std::to_string(filemon->m_filesInDir);
            filemon->m_status->copy_label(status.c_str());
        }
    }
}

void Filemon::tree_cb(Fl_Widget* w, void* data)
{
    Filemon* filemon = (Filemon*)data;

    Fl_Tree      *tree = (Fl_Tree*)w;
    Fl_Tree_Item *item = (Fl_Tree_Item*)tree->callback_item();

    char* itempath = new char[1024];
    tree->item_pathname(itempath, 1024, item);
    std::string itempathString = itempath;
    delete[] itempath;
    itempathString.insert(0, "/");

    if(tree->callback_reason() == FL_TREE_REASON_OPENED)
    {
        std::string child = item->child(0)->label();
        if(child == "")
        {
            item->clear_children();
            
            filemon->tree_populate_dir(item, itempathString);
        }
    }
    else if(tree->callback_reason() == FL_TREE_REASON_SELECTED)
    {
        filemon->navigate_to_dir(itempathString);
    }
}

void Filemon::menu_cb(Fl_Widget* w, void* data)
{
    Filemon* filemon = (Filemon*)w->window();
    Fle_Listview* listview = filemon->m_listview;
    Fl_Menu_Bar* menu = (Fl_Menu_Bar*)w;
    size_t item = (size_t)data;

    switch(item)
    {
        case MENU_FILE_OPEN_TERMINAL:
            filemon->open_terminal();
            break;
        case MENU_FILE_QUIT:
            exit(0);
            break;
        case MENU_EDIT_CUT:
            filemon->copy_selected_files(true);
            break;
        case MENU_EDIT_COPY:
            filemon->copy_selected_files(false);
            break;
        case MENU_EDIT_PASTE:
            filemon->paste_files();
            break;
        case MENU_EDIT_TRASH:
        {
            std::vector<int> selected = listview->get_selected();
            for(int i = 0; i < selected.size(); i++)
            {
                ListviewFile* file = (ListviewFile*)listview->get_item(selected[i]);
                filemon->trash_file(file->get_path());
            }
            filemon->navigate_to_dir(filemon->m_currentPath);
            break;
        }
        case MENU_VIEW:
            ((Fl_Menu_Item*)menu->find_item("View/Show treeview"))->value(!filemon->m_treeDockGroup->hidden());
            break;
        case MENU_VIEW_BIGICONS:
            listview->set_display_mode(FLE_LISTVIEW_DISPLAY_ICONS);
            break;
        case MENU_VIEW_SMALLICONS:
            listview->set_display_mode(FLE_LISTVIEW_DISPLAY_SMALL_ICONS);
            break;
        case MENU_VIEW_LIST:
            listview->set_display_mode(FLE_LISTVIEW_DISPLAY_LIST);
            break;
        case MENU_VIEW_DETAILS:
            listview->set_display_mode(FLE_LISTVIEW_DISPLAY_DETAILS);
            break;
        case MENU_VIEW_DETAILS_HIGHLIGHT_NONE:
            listview->set_details_mode(FLE_LISTVIEW_DETAILS_NONE);
            break;
        case MENU_VIEW_DETAILS_HIGHLIGHT_LINE:
            listview->set_details_mode(FLE_LISTVIEW_DETAILS_LINES);
            break;
        case MENU_VIEW_DETAILS_HIGHLIGHT_BOX:
            listview->set_details_mode(FLE_LISTVIEW_DETAILS_HIGHLIGHT);
            break;
        case MENU_HELP_ABOUT:
            filemon->m_aboutWindow->show();
            break;
    }
}

void Filemon::tool_cb(Fl_Widget* w, void* data)
{
    Filemon* filemon = (Filemon*)w->window();

    size_t tool = (size_t)data;

    switch(tool)
    {
        case 0:
            if(filemon->m_history.size() < 2) return;
            filemon->m_history.pop();
            filemon->navigate_to_dir(filemon->m_history.top());
            filemon->m_history.pop();
            break;
        case 1:
            break;
        case 2:
            filemon->navigate_to_dir(filemon->m_currentPath.parent_path());
            break;
        case 4:
            if(filemon->m_treeDockGroup->hidden())
            {
                filemon->m_treeDockGroup->show_group();
            }
            else
            {
                filemon->m_treeDockGroup->hide_group();
            }
            break;
    }
}

void Filemon::dir_watch_cb(GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data)
{
    Filemon* filemon = (Filemon*)user_data;

    GFile* dir = g_file_get_parent(file);

    // If the file is in the current directory, update the listview
    if(strcmp(g_file_get_path(dir), filemon->m_currentPath.c_str()) == 0)
    {
        if(event_type == G_FILE_MONITOR_EVENT_DELETED)
        {
            for(int i = 0; i < filemon->m_listview->get_item_count(); i++)
            {
                ListviewFile* listviewFile = (ListviewFile*)filemon->m_listview->get_item(i);
                if(listviewFile->get_path() == g_file_get_path(file))
                {
                    filemon->m_listview->remove_item(i);
                    filemon->m_filesInDir--;
                    break;
                }
            }
        }
        else if(event_type == G_FILE_MONITOR_EVENT_CREATED)
        {
            int size = std::filesystem::is_directory(g_file_get_path(file)) ? -1 : std::filesystem::file_size(g_file_get_path(file));
            ListviewFile* item = new ListviewFile(g_file_get_basename(file), size, g_file_get_path(file));
            filemon->m_listview->add_item(item);
            filemon->m_filesInDir++;
        }
        else if(event_type == G_FILE_MONITOR_EVENT_CHANGED)
        {
            for(int i = 0; i < filemon->m_listview->get_item_count(); i++)
            {
                ListviewFile* listviewFile = (ListviewFile*)filemon->m_listview->get_item(i);
                if(listviewFile->get_path() == g_file_get_path(file))
                {
                    listviewFile->size(std::filesystem::is_directory(g_file_get_path(file)) ? -1 : std::filesystem::file_size(g_file_get_path(file)));
                    break;
                }
            }
        }
    }

    // Update the treeview
    Fl_Tree_Item* treeItem = filemon->m_tree->find_item(g_file_get_path(dir));
    if(treeItem && event_type == G_FILE_MONITOR_EVENT_DELETED)
    {
        treeItem->remove_child(g_file_get_basename(file));
    }
    if(treeItem && g_file_query_file_type(file, G_FILE_QUERY_INFO_NONE, nullptr) == G_FILE_TYPE_DIRECTORY && event_type == G_FILE_MONITOR_EVENT_CREATED)
    {
        std::filesystem::path newItem = g_file_get_path(file);
        Fl_Tree_Item* itemAdded = filemon->m_tree->add(treeItem, newItem.filename().string().c_str());
        for(auto const& entry : std::filesystem::directory_iterator(newItem))
        {
            if(entry.is_directory())
            {
                itemAdded->add(filemon->m_tree->prefs(), "");
                itemAdded->close();
                break;
            }
        }
        itemAdded->close();
    }

    filemon->redraw();
}

void Filemon::construct_menu(Fl_Menu_Bar* menu)
{
    menu->add("File/New File", FL_CTRL + FL_SHIFT + 'n', menu_cb, (void*)MENU_FILE_NEW_FILE);
    menu->add("File/New Folder", FL_CTRL + FL_ALT + 'n', menu_cb, (void*)MENU_FILE_NEW_FOLDER, FL_MENU_DIVIDER);
    menu->add("File/File Properties", FL_Enter + FL_ALT, menu_cb, (void*)MENU_FILE_FILE_PROPERTIES);
    menu->add("File/Folder Properties", 0, menu_cb, (void*)MENU_FILE_FOLDER_PROPERTIES, FL_MENU_DIVIDER);
    menu->add("File/Open terminal", FL_F + 4, menu_cb, (void*)MENU_FILE_OPEN_TERMINAL);
    menu->add("File/Quit", FL_CTRL + 'q', menu_cb, (void*)MENU_FILE_QUIT);

    menu->add("Edit/Cut", FL_CTRL + 'x', menu_cb, (void*)MENU_EDIT_CUT);
    menu->add("Edit/Copy", FL_CTRL + 'c', menu_cb, (void*)MENU_EDIT_COPY);
    menu->add("Edit/Paste", FL_CTRL + 'v', menu_cb, (void*)MENU_EDIT_PASTE);
    menu->add("Edit/Rename", FL_F + 2, menu_cb, (void*)MENU_EDIT_RENAME);
    menu->add("Edit/Trash", FL_Delete, menu_cb, (void*)MENU_EDIT_TRASH, FL_MENU_DIVIDER);
    menu->add("Edit/Preferences", 0, menu_cb, (void*)MENU_EDIT_PREFERENCES);

    menu->add("View/Show treeview", "", menu_cb, (void*)MENU_VIEW_TREE, FL_MENU_TOGGLE | FL_MENU_VALUE);
    menu->add("View/Show status bar", "", menu_cb, (void*)MENU_VIEW_STATUS, FL_MENU_TOGGLE | FL_MENU_VALUE);
    menu->add("View/Show hidden files", "", menu_cb, (void*)MENU_VIEW_SHOW_HIDDEN, FL_MENU_TOGGLE | FL_MENU_VALUE | FL_MENU_DIVIDER);
    menu->add("View/Big icons", "", menu_cb, (void*)MENU_VIEW_BIGICONS, FL_MENU_RADIO);
    menu->add("View/Small icons", "", menu_cb, (void*)MENU_VIEW_SMALLICONS, FL_MENU_RADIO);
    menu->add("View/List", "", menu_cb, (void*)MENU_VIEW_LIST, FL_MENU_RADIO);
    menu->add("View/Details", "", menu_cb, (void*)MENU_VIEW_DETAILS, FL_MENU_DIVIDER | FL_MENU_RADIO | FL_MENU_VALUE);
    menu->add("View/No highlights", "", menu_cb, (void*)MENU_VIEW_DETAILS_HIGHLIGHT_NONE, FL_MENU_RADIO);
    menu->add("View/Line highlights", "", menu_cb, (void*)MENU_VIEW_DETAILS_HIGHLIGHT_LINE, FL_MENU_RADIO);
    menu->add("View/Box highlights", "", menu_cb, (void*)MENU_VIEW_DETAILS_HIGHLIGHT_BOX, FL_MENU_RADIO | FL_MENU_VALUE);

    menu->add("Help/Shortcuts", 0, menu_cb, (void*)MENU_HELP_SHORTCUTS);
    menu->add("Help/About", 0, menu_cb, (void*)MENU_HELP_ABOUT);

    ((Fl_Menu_Item*)menu->find_item("View"))->callback(menu_cb, (void*)MENU_VIEW);
}

void Filemon::construct_toolbar(Fle_Toolbar* toolbar)
{
    Fle_Flat_Button* btn1 = new Fle_Flat_Button(0, 0, 24, 24, "@<-");
    Fle_Flat_Button* btn2 = new Fle_Flat_Button(0, 0, 24, 24, "@->");
    Fle_Flat_Button* btn3 = new Fle_Flat_Button(0, 0, 24, 24, "@8->");
    Fle_Flat_Button* btn4 = new Fle_Flat_Button(0, 0, 24, 24, "@search");
    Fle_Flat_Button* btn5 = new Fle_Flat_Button(0, 0, 24, 24, "@fileopen");
    toolbar->add_tool(btn1);
    toolbar->add_tool(btn2);
    toolbar->add_tool(btn3);
    toolbar->add_separator();
    toolbar->add_tool(btn4);
    toolbar->add_tool(btn5);

    btn1->callback(tool_cb, (void*)0);
    btn2->callback(tool_cb, (void*)1);
    btn3->callback(tool_cb, (void*)2);
    btn4->callback(tool_cb, (void*)3);
    btn5->callback(tool_cb, (void*)4);
}

void Filemon::tree_populate_dir(Fl_Tree_Item* directory, std::filesystem::path path)
{
    if(directory->user_data() == (void*)1)
    {
        return;
    }
    directory->remove_child("");
    directory->user_data((void*)1);;

    monitor_directory(path);

    for(auto const& entry : std::filesystem::directory_iterator(path))
    {
        if(entry.is_directory())
        {
            if(directory->find_child(entry.path().filename().c_str()) != -1)
            {
                continue;
            }
            Fl_Tree_Item* itemAdded = m_tree->add(directory, entry.path().filename().string().c_str());
            try
            {
                for(auto const& entry2 : std::filesystem::directory_iterator(entry.path()))
                {
                    if(entry2.is_directory())
                    {
                        itemAdded->add(m_tree->prefs(), "");
                        itemAdded->close();
                        break;
                    }
                }
            }
            catch(const std::filesystem::filesystem_error& e)
            {
                continue;
            }
        }
    }
}

void Filemon::monitor_directory(const std::filesystem::path &path)
{
    if(m_monitors.find(path) != m_monitors.end()) return;

    GFile* gfile = g_file_new_for_path(path.c_str());
    GFileMonitor* monitor = g_file_monitor_directory(gfile, G_FILE_MONITOR_NONE, nullptr, nullptr);
    g_object_unref(gfile);
    m_monitors[path] = monitor;
    g_signal_connect(monitor, "changed", G_CALLBACK(Filemon::dir_watch_cb), this);
}

bool Filemon::navigate_to_dir(const std::filesystem::path& path)
{
    m_listview->clear_items();
    m_filesInDir = 0;

    for(auto const& entry : std::filesystem::directory_iterator(path))
    {
        long fileSize;
        try
        {
            fileSize = entry.is_directory() ? -1 : entry.file_size();
        }
        catch(const std::filesystem::filesystem_error& e)
        {
            continue;
        }
        ListviewFile* item = new ListviewFile(entry.path().filename().c_str(), fileSize, entry.path());
        m_listview->add_item(item);
        m_filesInDir++;
    }

    monitor_directory(path);

    label(path.filename().c_str());
    if(strcmp(label(), "") == 0)
    {
        label("/");
    }

    std::string status = "Elements: " + std::to_string(m_filesInDir);
    m_status->copy_label(status.c_str());
    m_address->value(path.string().c_str());
    m_currentPath = path;
    m_history.push(path);
    m_listview->sort_items(true, 0);

    if(path != "/")
    {
        Fl_Tree_Item* treeItem = m_tree->find_item(path.string().c_str());

        if(treeItem == nullptr)
        {
            treeItem = m_tree->add(path.string().c_str());

            Fl_Tree_Item* i = treeItem;
            std::filesystem::path pathCopy = path;
            while(i != m_tree->root())
            {
                tree_populate_dir(i, pathCopy);
                pathCopy = pathCopy.parent_path();
                i = i->parent();
                i->open();
            }
        }

        m_tree->select_only(treeItem, false);

        m_tree->show_item(treeItem);
    }
    else
    {
        m_tree->deselect_all();
    }

    chdir(m_currentPath.string().c_str());

    return true;
}

bool Filemon::open_file(const std::filesystem::path &path)
{
    chdir(path.parent_path().c_str());
    GFile* gfile = g_file_new_for_path(path.c_str());
    GFileInfo* gfileinfo = g_file_query_info(gfile, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
    std::string mime(g_file_info_get_content_type(gfileinfo));
    g_object_unref(gfileinfo);
    g_object_unref(gfile);

    GAppInfo* gappinfo;

    if (mime == "application/x-executable")
    {
        std::string command = "\"" + path.string() + "\"";
        gappinfo = g_app_info_create_from_commandline(command.c_str(), nullptr, G_APP_INFO_CREATE_NONE, nullptr);
        g_app_info_launch(gappinfo, nullptr, nullptr, nullptr);
    }
    else
    {
        gappinfo = g_app_info_get_default_for_type(mime.c_str(), true);
        if(!gappinfo)
        {
            return false;
        }

        std::string uri = "file://" + path.string();
        GList* uris = nullptr;
        uris = g_list_append(uris, (gpointer)uri.c_str());
        g_app_info_launch_uris(gappinfo, uris, nullptr, nullptr);
        g_list_free(uris);
    }

    g_object_unref(gappinfo);

    chdir(m_currentPath.string().c_str());
    return true;
}

bool Filemon::open_terminal()
{
    GAppInfo* gappinfo;

    GError* error = nullptr;
    gappinfo = g_app_info_create_from_commandline(m_terminalEmulator.c_str(), nullptr, G_APP_INFO_CREATE_NONE, &error);
    if(error)
    {
        g_error_free(error);
        return false;
    }

                                                            
    if(!gappinfo)
    {
        return false;
    }

    g_app_info_launch(gappinfo, nullptr, nullptr, nullptr);
    g_object_unref(gappinfo);
    return true;
}

void Filemon::copy_selected_files(bool cut)
{
    std::string buffer;
    buffer += cut ? "cut\n" : "copy\n";
    std::vector<int> selected = m_listview->get_selected();
    for(int i = 0; i < selected.size(); i++)
    {
        ListviewFile* file = (ListviewFile*)m_listview->get_item(selected[i]);
        buffer += "file://" + file->get_path().string() + "\n";
    }

    std::string command;

    #ifdef FLTK_USE_WAYLAND
	if (fl_wl_display())
	{
        // Use wl-copy for Wayland clipboard support with proper MIME type
        command = "printf \"" + buffer + "\" | wl-copy --type x-special/gnome-copied-files";
        system(command.c_str());
        return;
    }
    #endif

    command += "printf \"" + buffer + "\" | xclip -sel clip -t x-special/gnome-copied-files";

    system(command.c_str());
}

void Filemon::paste_files()
{
    #ifdef FLTK_USE_WAYLAND
	if (fl_wl_display())
	{
        // Use wl-paste for Wayland clipboard support
        FILE* pipe = popen("wl-paste --type x-special/gnome-copied-files", "r");
        
        if (pipe == nullptr)
        {
            return;
        }

        std::stringstream buffer;
        char readBuffer[PATH_MAX];
        while (fgets(readBuffer, PATH_MAX, pipe) != nullptr)
        {
            buffer << readBuffer;
        }
        pclose(pipe);

        std::string line;
        bool cut = false;
        std::getline(buffer, line);
        if(line == "cut")
        {
            cut = true;
        }
        else if(line != "copy")
        {
            return;
        }
        
        while(std::getline(buffer, line))
        {
            if(line.substr(0, 7) != "file://") continue;
            std::filesystem::path path(line.substr(7));
            std::filesystem::copy(path, m_currentPath / path.filename(), std::filesystem::copy_options::overwrite_existing);
            if(cut)
            {
                std::filesystem::remove(path);
            }
        }

        return;
    }
    #endif

    // Unfortunately, xclip can sometimes halt, so we need to use timeout
    // This happens when the clipboard selection is owned by certain applications
    // among them all FLTK apps.
    FILE* pipe = popen("timeout 0.01s xclip -sel clip -o -t x-special/gnome-copied-files", "r");

    if (pipe == nullptr)
        return;

    std::stringstream buffer;
    char readBuffer[PATH_MAX];
    while (fgets(readBuffer, PATH_MAX, pipe) != nullptr)
    {
        buffer << readBuffer;
    }
    pclose(pipe);

    std::string line;
    bool cut = false;
    std::getline(buffer, line);
    if(line == "cut")
    {
        cut = true;
    }
    else if(line != "copy")
    {
        return;
    }
    
    while(std::getline(buffer, line))
    {
        if(line.substr(0, 7) != "file://") continue;
        std::filesystem::path path(line.substr(7));
        std::filesystem::copy(path, m_currentPath / path.filename(), std::filesystem::copy_options::overwrite_existing);
        if(cut)
        {
            std::filesystem::remove(path);
        }
    }
}

void Filemon::trash_file(const std::filesystem::path &path)
{
    GFile* gfile = g_file_new_for_path(path.c_str());
    GError* error = nullptr;
    g_file_trash(gfile, nullptr, &error);
    if(error)
    {
        g_error_free(error);
    }
    g_object_unref(gfile);
}

int Filemon::run()
{
    navigate_to_dir(std::filesystem::current_path());

    GMainLoop* loop = g_main_loop_new(nullptr, false);
    
    do
    {
        g_main_context_iteration(g_main_loop_get_context(loop), false);
    } while (Fl::wait() != 0);

    g_main_loop_unref(loop);

    for(auto const& entry : m_monitors)
    {
        g_object_unref(entry.second);
    }

    return 0;
}
