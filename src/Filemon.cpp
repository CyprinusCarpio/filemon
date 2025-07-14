#include "Filemon.hpp"
#include "File.hpp"
#include "Util.hpp"
#include "../icons/folder16.xpm"

#include <iostream>
#include <sys/types.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <unistd.h>

#include "xdgmime.h"

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
    m_inotifyFd = -1;

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
        case MENU_FILE_QUIT:
            exit(0);
            break;
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

    int watch = inotify_add_watch(m_inotifyFd, path.c_str(), IN_CREATE | IN_DELETE | IN_MOVE);
    m_watchMap[watch] = path;

    for(auto const& entry : std::filesystem::directory_iterator(path))
    {
        if(entry.is_directory())
        {
            if(directory->find_child(entry.path().filename().string().c_str()) != -1)
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

void Filemon::tree_update_branch(inotify_event* event)
{
    Fl_Tree_Item* treeItem = m_tree->find_item(m_watchMap[event->wd].string().c_str());
    if(treeItem == nullptr)
    {
        return;
    }
    if(event->mask & IN_CREATE || event->mask & IN_MOVED_TO)
    {
        std::filesystem::path newItem = m_watchMap[event->wd] / event->name;
        if(std::filesystem::is_directory(newItem))
        {
            Fl_Tree_Item* itemAdded = m_tree->add(treeItem, event->name);
            for(auto const& entry2 : std::filesystem::directory_iterator(newItem))
            {
                if(entry2.is_directory())
                {
                    itemAdded->add(m_tree->prefs(), "");
                    itemAdded->close();
                    break;
                }
            }
            itemAdded->close();
        }
    }
    else if(event->mask & IN_DELETE || event->mask & IN_MOVED_FROM)
    {
        treeItem->remove_child(event->name);
    }

    redraw();
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
            fileSize = entry.is_directory() ? -1 :entry.file_size();
        }
        catch(const std::filesystem::filesystem_error& e)
        {
            continue;
        }
        ListviewFile* item = new ListviewFile(entry.path().filename().string().c_str(), fileSize, entry.path());
        m_listview->add_item(item);
        m_filesInDir++;
    }

    int watch = inotify_add_watch(m_inotifyFd, path.string().c_str(), IN_CREATE | IN_DELETE | IN_MOVE);
    m_watchMap[watch] = path;

    label(path.filename().string().c_str());
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


    return true;
}

bool Filemon::open_file(const std::filesystem::path &path)
{
    struct stat st;
    if (stat(path.string().c_str(), &st) == 0 && (st.st_mode & S_IXUSR))
    {
        std::string command = "nohup \"" + path.string() + "\" &";
        if (popen(command.c_str(), "r") == nullptr)
        {
            return false;
        }
    }
    else
    {
        std::string command = "xdg-open \"" + path.string() + "\"";
        if (system(command.c_str()) == -1)
        {
            return false;
        }
    }
    return true;
}

int Filemon::run()
{
    m_inotifyFd = inotify_init();
    int flags = fcntl(m_inotifyFd, F_GETFL, 0);
    fcntl(m_inotifyFd, F_SETFL, flags | O_NONBLOCK);
    int l = 0, i = 0;
    char buffer[1024 * sizeof (inotify_event) + 16];
    if(m_inotifyFd == -1)
    {
        return 1;
    }

    navigate_to_dir(std::filesystem::current_path());
    
    while(true)
    {
        i = 0;
        int r = Fl::wait();
        if(r == 0)
        {
            break;
        }

        l = read(m_inotifyFd, buffer, 1024 * sizeof (inotify_event) + 16);
        
        while(i < l)
        {
            inotify_event* event = (inotify_event*)&buffer[i];
            if(event->len)
            {
                tree_update_branch(event);

                if(m_watchMap[event->wd] == m_currentPath)
                {
                    navigate_to_dir(m_currentPath);
                }
            }
            i += sizeof(inotify_event) + event->len;
        }
    }

    for(auto const& watch : m_watchMap)
    {
        inotify_rm_watch(m_inotifyFd, watch.first);
    }

    close(m_inotifyFd);

    xdg_mime_shutdown();

    return 0;
}