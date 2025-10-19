#include "Filemon.hpp"
#include "File.hpp"
#include "Util.hpp"
#include "Properties.hpp"
#include "../icons/folder16.xpm"
#include "../icons/device16.xpm"
#include "../icons/unmount16.xpm"
#include "../icons/filemon.xpm"
#include "../icons/toolbar_back.xpm"
#include "../icons/toolbar_forward.xpm"
#include "../icons/toolbar_up.xpm"
#include "../icons/toolbar_search.xpm"
#include "../icons/toolbar_folders.xpm"
#include "../icons/toolbar_devices.xpm"
#include "../icons/toolbar_home.xpm"

#include <FL/Fl_Text_Display.H>
#include <FL/fl_ask.H> 

#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <regex>
#include <algorithm>

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
    MENU_FILE_OPEN_FILE,

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
    MENU_VIEW_SEARCH_PANE,
    MENU_VIEW_VOLUMES,
    MENU_VIEW_ADDRESSBAR,
    MENU_VIEW_TOOLBAR,
    MENU_VIEW_SHOW_HIDDEN,
    MENU_VIEW_BIGICONS,
    MENU_VIEW_SMALLICONS,
    MENU_VIEW_LIST,
    MENU_VIEW_DETAILS,
    MENU_VIEW_DETAILS_HIGHLIGHT_NONE,
    MENU_VIEW_DETAILS_HIGHLIGHT_LINE,
    MENU_VIEW_DETAILS_HIGHLIGHT_BOX,
    MENU_VIEW_SORT_BY_NAME,
    MENU_VIEW_SORT_BY_DATE_MODIFIED,
    MENU_VIEW_SORT_BY_PERMISSIONS,
    MENU_VIEW_SORT_BY_GROUP,
    MENU_VIEW_SORT_BY_OWNER,
    MENU_VIEW_SORT_BY_SIZE,
    MENU_VIEW_SORT_ASCENDING,
    MENU_VIEW_SORT_DESCENDING,

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
    DOCK_SEARCH,
    DOCK_VOLUMES,
};

void about_wnd_close_callback(Fl_Widget* w, void* d)
{
    w->window()->hide();
}

void about_wnd_github_callback(Fl_Widget* w, void* d)
{
    g_app_info_launch_default_for_uri("https://github.com/CyprinusCarpio/filemon", nullptr, nullptr);
}

class Icon_Listview : public Fle_Listview
{
    Filemon* m_filemon;
public:
    Icon_Listview(int X, int Y, int W, int H, const char* l) : Fle_Listview(X, Y, W, H, l) 
    {
        m_filemon = nullptr;
    };

    int handle(int event) override
    {
        int ret = Fle_Listview::handle(event);

        if(event == FL_PUSH && Fl::event_button() == FL_RIGHT_MOUSE)
        {
            if(m_filemon == nullptr) m_filemon = (Filemon*)window();
            m_filemon->popup_menu();
            return 1;
        }
        return ret;
    }
};

class Flat_Menu_Bar : public Fl_Menu_Bar
{
    Filemon* m_filemon;
public:
    Flat_Menu_Bar(int X, int Y, int W, int H, const char* l) : Fl_Menu_Bar(X, Y, W, H, l) 
    {
        m_filemon = (Filemon*)window();
    };

protected:
    void draw() override
    {
        fl_push_clip(x() + 3, y() + 3, w() - 6, h() - 6);
        Fl_Menu_Bar::draw();
        fl_pop_clip();
    }

    int handle(int event) override
    {
        if(event == FL_PUSH)
        {
            int selectedItems = m_filemon->m_listview->get_selected_count();

            static Fl_Menu_Item* cut = (Fl_Menu_Item*)find_item("Edit/Cut");
            static Fl_Menu_Item* copy = (Fl_Menu_Item*)find_item("Edit/Copy");
            static Fl_Menu_Item* paste = (Fl_Menu_Item*)find_item("Edit/Paste");
            static Fl_Menu_Item* rename = (Fl_Menu_Item*)find_item("Edit/Rename");
            static Fl_Menu_Item* trash = (Fl_Menu_Item*)find_item("Edit/Trash");

            if(selectedItems > 0)
            {
                cut->activate();
                copy->activate();
                trash->activate();
            }
            else
            {
                cut->deactivate();
                copy->deactivate();
                trash->deactivate();
            }
            if(selectedItems == 1)
            {
                rename->activate();
            }
            else
            {
                rename->deactivate();
            }

            static Fl_Menu_Item* showTreeview = (Fl_Menu_Item*)find_item("View/Show treeview");
            static Fl_Menu_Item* showSearch = (Fl_Menu_Item*)find_item("View/Show search pane");
            static Fl_Menu_Item* showVolumes = (Fl_Menu_Item*)find_item("View/Show volumes");
            static Fl_Menu_Item* showStatusbar = (Fl_Menu_Item*)find_item("View/Show status bar");
            static Fl_Menu_Item* showAddressbar = (Fl_Menu_Item*)find_item("View/Show address bar");
            static Fl_Menu_Item* showToolbar = (Fl_Menu_Item*)find_item("View/Show toolbar");
            static Fl_Menu_Item* showHiddenFiles = (Fl_Menu_Item*)find_item("View/Show hidden files");

            showTreeview->value(!m_filemon->m_treeDockGroup->hidden());
            showSearch->value(!m_filemon->m_searchGroup->hidden());
            showVolumes->value(!m_filemon->m_volumesGroup->hidden());
            showStatusbar->value(!m_filemon->m_statusGroup->hidden());
            showAddressbar->value(!m_filemon->m_addressGroup->hidden());
            showToolbar->value(!m_filemon->m_toolbarGroup->hidden());
            showHiddenFiles->value(m_filemon->m_preferences.m_showHiddenFiles);
        }

        return Fl_Menu_Bar::handle(event);
    }
};

class DeviceTreeItem : public Fl_Tree_Item
{
    bool m_volumeMounted;

public:
    DeviceTreeItem(bool mounted, Fl_Tree* tree) : Fl_Tree_Item(tree)
    {
        m_volumeMounted = mounted;
    }

    int draw_item_content(int render) override
    {
        int X=label_x(), Y=label_y(), W=label_w(), H=label_h();
        std::string text = label();
        int lw=0, lh=0;
        fl_measure(text.c_str(), lw, lh);

        if (render)
        {
            fl_color(drawbgcolor()); 
            fl_rectf(X,Y,W,H);
            fl_font(labelfont(), labelsize());
            fl_color(drawfgcolor());
            fl_draw(text.c_str(), X,Y,W,H, FL_ALIGN_LEFT);

            static Fl_Pixmap* unmount16 = new Fl_Pixmap(unmount16_xpm);

            if(m_volumeMounted)
            {
                unmount16->draw(X + W - unmount16->w() - 3, Y + (H - unmount16->h()) / 2, unmount16->w(), unmount16->h());
            }
        }

        return X + lw;
    }

    void mount(bool mount)
    {
        m_volumeMounted = mount;

        tree()->redraw();
    }
};

Filemon::Filemon(int W, int H, const char* l): Fl_Double_Window(W, H, l)
{
    m_folder16 = new Fl_Pixmap(folder16_xpm);
    m_device16 = new Fl_Pixmap(device16_xpm);
    m_toolbarBack24 = new Fl_Pixmap(toolbar_back_xpm);
    m_toolbarForward24 = new Fl_Pixmap(toolbar_forward_xpm);
    m_toolbarUp24 = new Fl_Pixmap(toolbar_up_xpm);
    m_toolbarSearch24 = new Fl_Pixmap(toolbar_search_xpm);
    m_toolbarFolders24 = new Fl_Pixmap(toolbar_folders_xpm);
    m_toolbarDevices24 = new Fl_Pixmap(toolbar_devices_xpm);
    m_toolbarHome24 = new Fl_Pixmap(toolbar_home_xpm);
    m_filemonLogo = new Fl_Pixmap(filemon_xpm);

    m_dockHost = new Fle_Dock_Host(0, 0, W, H, l, FLE_DOCK_ALLDIRS);
    resizable(m_dockHost);
    end();

    m_listview = new Icon_Listview(0, 0, 0, 0, "");
    m_listview->callback(listview_cb, (void*)this);
    m_listview->set_property_widths({80, 80, 80, 100, 120});
    m_listview->set_property_order({FILE_PROPERTY_SIZE, FILE_PROPERTY_OWNER, FILE_PROPERTY_GROUP, FILE_PROPERTY_PERMISSIONS, FILE_PROPERTY_MODIFIED});
    m_listview->add_property_name("Size");
    m_listview->add_property_name("Owner");
    m_listview->add_property_name("Group");
    m_listview->add_property_name("Permission");
    m_listview->add_property_name("Modified");
    m_listview->item_tooltips(true);
    m_dockHost->add_work_widget(m_listview);
    
    m_treeDockGroup = new Fle_Dock_Group(m_dockHost, DOCK_TREE, "Folders", FLE_DOCK_DETACHABLE | FLE_DOCK_FLEXIBLE, FLE_DOCK_LEFT, FLE_DOCK_RIGHT, 150, 180, false);
    m_tree = new Fl_Tree(0, 0, 0, 0, "");
    m_tree->callback(tree_cb, (void*)this);
    m_tree->showroot(false);
    m_tree->sortorder(FL_TREE_SORT_ASCENDING);
    m_tree->root_label("/");
    m_tree->usericon(m_folder16);
    m_treeDockGroup->add_band_widget(m_tree);

    m_searchGroup = new Fle_Dock_Group(m_dockHost, DOCK_SEARCH, "Search", FLE_DOCK_DETACHABLE | FLE_DOCK_FLEXIBLE, FLE_DOCK_LEFT, FLE_DOCK_RIGHT, 180, 180, false);
    Fl_Group* searchGroup = new Fl_Group(0, 0, 0, 0, "");
    searchGroup->end();
    m_searchGroup->add_band_widget(searchGroup);
    construct_search_pane(searchGroup);
    m_searchGroup->hide_group();

    m_volumesGroup = new Fle_Dock_Group(m_dockHost, DOCK_VOLUMES, "Volumes", FLE_DOCK_DETACHABLE | FLE_DOCK_FLEXIBLE, FLE_DOCK_LEFT, FLE_DOCK_RIGHT, 180, 180, false);
    m_volumesTree = new Fl_Tree(0, 0, 0, 0, "");
    m_volumesTree->showroot(false);
    m_volumesTree->usericon(m_device16);
    m_volumesTree->callback(volumes_tree_cb, (void*)this);
    m_volumesTree->item_reselect_mode(FL_TREE_SELECTABLE_ALWAYS);
    m_volumesGroup->add_band_widget(m_volumesTree);
    m_volumesGroup->hide_group();

    m_menuGroup = new Fle_Dock_Group(m_dockHost, DOCK_MENU, "Menu", FLE_DOCK_NO_HOR_LABEL, FLE_DOCK_TOP, FLE_DOCK_BOTTOM, 200, 26, false);
    Flat_Menu_Bar* menu = new Flat_Menu_Bar(0, 0, 0, 0, "");
    m_menuGroup->add_band_widget(menu, 1, 1, 1, 1);
    construct_menu(menu);

    m_toolbarGroup = new Fle_Dock_Group(m_dockHost, DOCK_TOOLBAR, "Toolbar", FLE_DOCK_FLEXIBLE | FLE_DOCK_NO_HOR_LABEL, FLE_DOCK_TOP, FLE_DOCK_BOTTOM, 185, 32, true);
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

    m_aboutWindow = new Fl_Double_Window(256, 350, "About Filemon");
    Fl_Box* logo = new Fl_Box(0, 0, 256, 130);
    logo->image(m_filemonLogo);
    Fl_Text_Display* text = new Fl_Text_Display(0, 140, 256, 170);
    Fl_Text_Buffer* buffer = new Fl_Text_Buffer();
    buffer->text(
        "Filemon 0.1.0\n"
        "A lightweight X11/Wayland file browser for Linux based on the FLTK toolkit."
    );
    text->buffer(buffer);
    text->wrap_mode(2, 0);
    text->box(FL_FLAT_BOX);
    text->color(FL_BACKGROUND_COLOR);

    Fl_Button* githubButton = new Fl_Button(100, 318, 70, 26, "GitHub");
    githubButton->callback(about_wnd_github_callback);
    Fl_Button* closeButton = new Fl_Button(256 - 70 - 10, 318, 70, 26, "Close");
    closeButton->callback(about_wnd_close_callback);
}

Filemon::~Filemon()
{
    delete m_folder16;
    delete m_device16;
    delete m_toolbarBack24;
    delete m_toolbarForward24;
    delete m_toolbarUp24;
    delete m_toolbarSearch24;
    delete m_toolbarFolders24;
    delete m_toolbarDevices24;
    delete m_toolbarHome24;
    delete m_filemonLogo;
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
        case MENU_FILE_NEW_FILE:
            filemon->new_file();
            break;
        case MENU_FILE_NEW_FOLDER:
            filemon->new_folder();
            break;
        case MENU_FILE_OPEN_TERMINAL:
            filemon->open_terminal();
            break;
        case MENU_FILE_QUIT:
            exit(0);
            break;
        case MENU_FILE_FILE_PROPERTIES:
            filemon->open_file_properties();
            break;
        case MENU_FILE_FOLDER_PROPERTIES:
            open_properties_wnd({filemon->m_currentPath});
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
            break;
        }
        case MENU_EDIT_RENAME:
            filemon->rename_selected();
            break;
        case MENU_EDIT_PREFERENCES:
        {
            show_preferences_wnd(&filemon->m_preferences);
            break;
        }
        case MENU_VIEW_TREE:
            if(filemon->m_treeDockGroup->hidden())
            {
                filemon->m_treeDockGroup->show_group();
            }
            else
                filemon->m_treeDockGroup->hide_group();

            break;
        case MENU_VIEW_SEARCH_PANE:
            if(filemon->m_searchGroup->hidden())
            {
                filemon->m_searchGroup->show_group();
            }
            else
                filemon->m_searchGroup->hide_group();

            break;
        case MENU_VIEW_VOLUMES:
            if(filemon->m_volumesGroup->hidden())
            {
                filemon->m_volumesGroup->show_group();
            }
            else
            {
                filemon->m_volumesGroup->hide_group();
            }
            break;
        case MENU_VIEW_STATUS:
            if(filemon->m_statusGroup->hidden())
            {
                filemon->m_statusGroup->show_group();
            }
            else
                filemon->m_statusGroup->hide_group();

            break;
        case MENU_VIEW_ADDRESSBAR:
            if(filemon->m_addressGroup->hidden())
            {
                filemon->m_addressGroup->show_group();
            }
            else
                filemon->m_addressGroup->hide_group();

            break; 
        case MENU_VIEW_TOOLBAR:
            if(filemon->m_toolbarGroup->hidden())
            {
                filemon->m_toolbarGroup->show_group();
            }
            else
                filemon->m_toolbarGroup->hide_group();
            
            break;
        case MENU_VIEW_SHOW_HIDDEN:
            filemon->show_hidden_files(!filemon->m_preferences.m_showHiddenFiles);
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
        case MENU_HELP_SHORTCUTS:
            fl_message("TODO");
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
        case 3:
            if(filemon->m_searchGroup->hidden())
            {
                filemon->m_searchGroup->show_group();
            }
            else
            {
                filemon->m_searchGroup->hide_group();
            }
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
        case 5:
            if(filemon->m_volumesGroup->hidden())
            {
                filemon->m_volumesGroup->show_group();
            }
            else
            {
                filemon->m_volumesGroup->hide_group();
            }
            break;
        case 6:
            filemon->navigate_to_dir(getenv("HOME"));
            break;
    }
}

void Filemon::popup_cb(Fl_Widget* w, void* data)
{
    Filemon* filemon = (Filemon*)w;

    size_t item = (size_t)data;

    switch(item)
    {
        case MENU_FILE_NEW_FILE:
            filemon->new_file();
            break;
        case MENU_FILE_FILE_PROPERTIES:
            filemon->open_file_properties();
            break;
        case MENU_FILE_NEW_FOLDER:
            filemon->new_folder();
            break;
        case MENU_FILE_OPEN_TERMINAL:
            filemon->open_terminal();
            break;
        case MENU_EDIT_PASTE:
            filemon->paste_files();
            break;
        case MENU_EDIT_SELECT_ALL:
            filemon->select_all(true);
            break;
        case MENU_VIEW_SORT_BY_NAME:
            filemon->m_listview->sort_items(filemon->m_listview->get_sort_direction(), -1);
            break;
        case MENU_VIEW_SORT_BY_DATE_MODIFIED:
            filemon->m_listview->sort_items(filemon->m_listview->get_sort_direction(), 4);
            break;
        case MENU_VIEW_SORT_BY_PERMISSIONS:
            filemon->m_listview->sort_items(filemon->m_listview->get_sort_direction(), 3);
            break;
        case MENU_VIEW_SORT_BY_GROUP:
            filemon->m_listview->sort_items(filemon->m_listview->get_sort_direction(), 2);
            break;
        case MENU_VIEW_SORT_BY_OWNER:
            filemon->m_listview->sort_items(filemon->m_listview->get_sort_direction(), 1);
            break;
        case MENU_VIEW_SORT_BY_SIZE:
            filemon->m_listview->sort_items(filemon->m_listview->get_sort_direction(), 0);
            break;
        case MENU_VIEW_SORT_ASCENDING:
            filemon->m_listview->sort_items(true, filemon->m_listview->get_sorted_by_property());
            break;
        case MENU_VIEW_SORT_DESCENDING:
            filemon->m_listview->sort_items(false, filemon->m_listview->get_sorted_by_property());
            break;
        case MENU_VIEW_SHOW_HIDDEN:
            filemon->show_hidden_files(!filemon->m_preferences.m_showHiddenFiles);
            break;
        case MENU_FILE_OPEN_FILE:
        {
            std::vector<int> selected = filemon->m_listview->get_selected();
            if(selected.size() > 0)
            {
                ListviewFile* file = (ListviewFile*)filemon->m_listview->get_item(selected[0]);
                filemon->open_file(file->get_path());
            }
            break;
        }
        case MENU_EDIT_CUT:
            filemon->copy_selected_files(true);
            break;
        case MENU_EDIT_COPY:
            filemon->copy_selected_files(false);
            break;
        case MENU_EDIT_TRASH:
        {
            std::vector<int> selected = filemon->m_listview->get_selected();
            if(selected.size() > 0)
            {
                ListviewFile* file = (ListviewFile*)filemon->m_listview->get_item(selected[0]);
                filemon->trash_file(file->get_path());
            }
            break;
        }
        case MENU_EDIT_RENAME:
            filemon->rename_selected();
            break;
    }
}

void Filemon::search_cb(Fl_Widget *w, void *data)
{
    Filemon* filemon = (Filemon*)data;
    filemon->navigate_to_dir(filemon->m_currentPath);
}

void Filemon::volumes_tree_cb(Fl_Widget *w, void *data)
{
    Filemon* filemon = (Filemon*)data;
    Fl_Tree* tree = (Fl_Tree*)w;
    Fl_Tree_Item* item = (Fl_Tree_Item*)tree->callback_item();

    if(item->user_data() == nullptr) 
        return;

    if(tree->callback_reason() == FL_TREE_REASON_SELECTED || tree->callback_reason() == FL_TREE_REASON_RESELECTED)
    {
        GVolume* gvolume = (GVolume*)item->user_data();
        GMount* gmount = g_volume_get_mount(gvolume);

        int x = item->label_x() + item->label_w() - 20;
        int y = item->label_y();
        int w = 20;
        int h = item->label_h();

        if(gmount != nullptr && Fl::event_inside(x, y, w, h))
        {
            g_mount_unmount_with_operation(gmount, G_MOUNT_UNMOUNT_NONE, nullptr, nullptr, nullptr, nullptr);

            g_object_unref(gmount);

            return;
        }
        else if(gmount == nullptr)
        {
            if(!g_volume_can_mount(gvolume))
            {
                fl_message_title("Error");
                fl_alert("Cannot mount volume");

                return;
            }

            g_volume_mount(gvolume, G_MOUNT_MOUNT_NONE, nullptr, nullptr, Filemon::mount_finished_cb, filemon);

            return;
        }

        filemon->navigate_to_dir(g_file_get_path(g_mount_get_root(gmount)));

        g_object_unref(gmount);
    }
}

void Filemon::dir_watch_cb(GFileMonitor* monitor, GFile* file, GFile* other_file, GFileMonitorEvent event_type, gpointer user_data)
{
    Filemon* filemon = (Filemon*)user_data;

    GFile* dir = g_file_get_parent(file);
    const char* path = g_file_get_path(file);
    const char* dirPath = g_file_get_path(dir);
    const char* basename = g_file_get_basename(file);

    // If the file is in the current directory, update the listview
    if(strcmp(dirPath, filemon->m_currentPath.c_str()) == 0)
    {
        if(event_type == G_FILE_MONITOR_EVENT_DELETED)
        {
            for(int i = 0; i < filemon->m_listview->get_item_count(); i++)
            {
                ListviewFile* listviewFile = (ListviewFile*)filemon->m_listview->get_item(i);
                if(listviewFile->get_path() == path)
                {
                    filemon->m_listview->remove_item(i);
                    filemon->m_filesInDir--;
                    break;
                }
            }
        }
        else if(event_type == G_FILE_MONITOR_EVENT_CREATED)
        {
            int size = -1;
            if (g_file_query_file_type(file, G_FILE_QUERY_INFO_NONE, nullptr) != G_FILE_TYPE_DIRECTORY) 
            {
                GFileInfo* info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
                if (info) 
                {
                    size = g_file_info_get_size(info);
                    g_object_unref(info);
                }
            }
            ListviewFile* item = new ListviewFile(path, size);
            filemon->m_listview->add_item(item);
            filemon->m_filesInDir++;
        }
        else if(event_type == G_FILE_MONITOR_EVENT_CHANGED)
        {
            for(int i = 0; i < filemon->m_listview->get_item_count(); i++)
            {
                ListviewFile* listviewFile = (ListviewFile*)filemon->m_listview->get_item(i);
                if(listviewFile->get_path() == path)
                {
                    int size = -1;
                    if (g_file_query_file_type(file, G_FILE_QUERY_INFO_NONE, nullptr) != G_FILE_TYPE_DIRECTORY) 
                    {
                        GFileInfo* info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
                        if (info) 
                        {
                            size = g_file_info_get_size(info);
                            g_object_unref(info);
                        }
                    }
                    listviewFile->size(size);
                    break;
                }
            }
        }
    }

    // Update the treeview
    Fl_Tree_Item* treeItem = filemon->m_tree->find_item(path);
    if(treeItem && event_type == G_FILE_MONITOR_EVENT_DELETED)
    {
        treeItem->remove_child(basename);
    }
    if(treeItem && g_file_query_file_type(file, G_FILE_QUERY_INFO_NONE, nullptr) == G_FILE_TYPE_DIRECTORY && event_type == G_FILE_MONITOR_EVENT_CREATED)
    {
        Fl_Tree_Item* itemAdded = filemon->m_tree->add(treeItem, basename);
        
        // Use GIO to check if the new directory has subdirectories
        GFile* newItemGFile = g_file_new_for_path(path);
        GFileEnumerator* enumerator = g_file_enumerate_children(newItemGFile, 
            G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE,
            G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
        
        if (enumerator) 
        {
            GFileInfo* info;
            while ((info = g_file_enumerator_next_file(enumerator, nullptr, nullptr))) 
            {
                if (g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY) 
                {
                    itemAdded->add(filemon->m_tree->prefs(), "");
                    itemAdded->close();
                    g_object_unref(info);
                    break;
                }
                g_object_unref(info);
            }
            g_object_unref(enumerator);
        }
        g_object_unref(newItemGFile);
        
        itemAdded->close();
    }

    g_object_unref(dir);
    g_free((gpointer)path);
    g_free((gpointer)dirPath);
    g_free((gpointer)basename);

    filemon->redraw();
}

void Filemon::volume_added_cb(GVolumeMonitor* monitor, GVolume* volume, gpointer user_data)
{
    Filemon* filemon = (Filemon*)user_data;

    std::string newEntry;
    GDrive* gdrive = g_volume_get_drive(volume);
    if(gdrive != nullptr)
    {
        newEntry = std::string(g_drive_get_name(gdrive)) + "/" + g_volume_get_name(volume);
    }
    else
    {
        newEntry = g_volume_get_name(volume);
    }

    DeviceTreeItem* item = new DeviceTreeItem(g_volume_get_mount(volume) != nullptr, filemon->m_volumesTree);
    item->user_data((void*)volume);
    item->label(g_volume_get_name(volume));
    filemon->m_volumesTree->add(newEntry.c_str(), item);

    g_object_unref(gdrive);
}

void Filemon::volume_removed_cb(GVolumeMonitor* monitor, GVolume* volume, gpointer user_data)
{
    Filemon* filemon = (Filemon*)user_data;

    for(Fl_Tree_Item* item = filemon->m_volumesTree->first(); item != nullptr; item = item->next())
    {
        if(item->user_data() == (void*)volume)
        {
            filemon->m_volumesTree->remove(item);
            g_object_unref(volume);
            break;
        }
    }
}

void Filemon::volume_changed_cb(GVolumeMonitor* monitor, GVolume* volume, gpointer user_data)
{
    Filemon* filemon = (Filemon*)user_data;

    for(Fl_Tree_Item* item = filemon->m_volumesTree->first(); item != nullptr; item = item->next())
    {
        if(item->user_data() == (void*)volume)
        {
            item->label(g_volume_get_name(volume));
            break;
        }
    }
}

void Filemon::mount_added_cb(GVolumeMonitor* monitor, GMount* mount, gpointer user_data)
{
    Filemon* filemon = (Filemon*)user_data;

    GVolume* volume = g_mount_get_volume(mount);

    for(Fl_Tree_Item* item = filemon->m_volumesTree->first(); item != nullptr; item = item->next())
    {
        if(item->user_data() == (void*)volume)
        {
            ((DeviceTreeItem*)item)->mount(true);
            return;
        }
    }
}

void Filemon::mount_removed_cb(GVolumeMonitor* monitor, GMount* mount, gpointer user_data)
{
    Filemon* filemon = (Filemon*)user_data;
    
    const char* mountName = g_mount_get_name(mount);

    for(Fl_Tree_Item* item = filemon->m_volumesTree->first(); item != nullptr; item = item->next())
    {
        if(strcmp(item->label(), mountName) == 0)
        {
            ((DeviceTreeItem*)item)->mount(false);
            return;
        }
    }
}

void Filemon::mount_finished_cb(GObject *source_object, GAsyncResult *res, gpointer data)
{
    Filemon* filemon = (Filemon*)data;

    GVolume* gvolume = (GVolume*)source_object;

    bool success = g_volume_mount_finish(gvolume, res, nullptr);

    if(!success)
    {
        fl_message_title("Error");
        fl_alert("Cannot mount volume");

        return;
    }

    GMount* gmount = g_volume_get_mount(gvolume);

    filemon->navigate_to_dir(g_file_get_path(g_mount_get_root(gmount)));

    g_object_unref(gmount);
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

    menu->add("View/Show treeview", FL_CTRL + 't', menu_cb, (void*)MENU_VIEW_TREE, FL_MENU_TOGGLE | FL_MENU_VALUE);
    menu->add("View/Show search pane", FL_CTRL + 'f', menu_cb, (void*)MENU_VIEW_SEARCH_PANE, FL_MENU_TOGGLE);
    menu->add("View/Show volumes", "", menu_cb, (void*)MENU_VIEW_VOLUMES, FL_MENU_TOGGLE);
    menu->add("View/Show status bar", "", menu_cb, (void*)MENU_VIEW_STATUS, FL_MENU_TOGGLE | FL_MENU_VALUE);
    menu->add("View/Show address bar", "", menu_cb, (void*)MENU_VIEW_ADDRESSBAR, FL_MENU_TOGGLE | FL_MENU_VALUE);
    menu->add("View/Show toolbar", "", menu_cb, (void*)MENU_VIEW_TOOLBAR, FL_MENU_TOGGLE | FL_MENU_VALUE);
    menu->add("View/Show hidden files", FL_CTRL + 'h', menu_cb, (void*)MENU_VIEW_SHOW_HIDDEN, FL_MENU_TOGGLE | FL_MENU_VALUE | FL_MENU_DIVIDER);
    menu->add("View/Big icons", FL_SHIFT + 'q', menu_cb, (void*)MENU_VIEW_BIGICONS, FL_MENU_RADIO);
    menu->add("View/Small icons",FL_SHIFT + 'w', menu_cb, (void*)MENU_VIEW_SMALLICONS, FL_MENU_RADIO);
    menu->add("View/List", FL_SHIFT + 'e', menu_cb, (void*)MENU_VIEW_LIST, FL_MENU_RADIO);
    menu->add("View/Details", FL_SHIFT + 'r', menu_cb, (void*)MENU_VIEW_DETAILS, FL_MENU_DIVIDER | FL_MENU_RADIO | FL_MENU_VALUE);
    menu->add("View/No highlights", "", menu_cb, (void*)MENU_VIEW_DETAILS_HIGHLIGHT_NONE, FL_MENU_RADIO);
    menu->add("View/Line highlights", "", menu_cb, (void*)MENU_VIEW_DETAILS_HIGHLIGHT_LINE, FL_MENU_RADIO);
    menu->add("View/Box highlights", "", menu_cb, (void*)MENU_VIEW_DETAILS_HIGHLIGHT_BOX, FL_MENU_RADIO | FL_MENU_VALUE);

    menu->add("Help/Shortcuts", 0, menu_cb, (void*)MENU_HELP_SHORTCUTS);
    menu->add("Help/About", 0, menu_cb, (void*)MENU_HELP_ABOUT);
}

void Filemon::construct_toolbar(Fle_Toolbar* toolbar)
{
    Fle_Flat_Button* btn1 = new Fle_Flat_Button(0, 0, 24, 24, ""); btn1->image(m_toolbarBack24); btn1->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_TEXT_NEXT_TO_IMAGE);
    Fle_Flat_Button* btn2 = new Fle_Flat_Button(0, 0, 24, 24, ""); btn2->image(m_toolbarForward24); btn2->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_TEXT_NEXT_TO_IMAGE);
    Fle_Flat_Button* btn3 = new Fle_Flat_Button(0, 0, 24, 24, ""); btn3->image(m_toolbarUp24); btn3->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_TEXT_NEXT_TO_IMAGE);
    Fle_Flat_Button* btn4 = new Fle_Flat_Button(0, 0, 24, 24, ""); btn4->image(m_toolbarSearch24); btn4->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_TEXT_NEXT_TO_IMAGE);
    Fle_Flat_Button* btn5 = new Fle_Flat_Button(0, 0, 24, 24, ""); btn5->image(m_toolbarFolders24); btn5->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_TEXT_NEXT_TO_IMAGE);
    Fle_Flat_Button* btn6 = new Fle_Flat_Button(0, 0, 24, 24, ""); btn6->image(m_toolbarDevices24); btn6->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_TEXT_NEXT_TO_IMAGE);
    Fle_Flat_Button* btn7 = new Fle_Flat_Button(0, 0, 24, 24, ""); btn7->image(m_toolbarHome24); btn7->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_TEXT_NEXT_TO_IMAGE);
    toolbar->add_tool(btn1);
    toolbar->add_tool(btn2);
    toolbar->add_tool(btn3);
    toolbar->add_separator();
    toolbar->add_tool(btn4);
    toolbar->add_tool(btn5);
    toolbar->add_tool(btn6);
    toolbar->add_tool(btn7);

    btn1->callback(tool_cb, (void*)0);
    btn2->callback(tool_cb, (void*)1);
    btn3->callback(tool_cb, (void*)2);
    btn4->callback(tool_cb, (void*)3);
    btn5->callback(tool_cb, (void*)4);
    btn6->callback(tool_cb, (void*)5);
    btn7->callback(tool_cb, (void*)6);
}

void Filemon::construct_search_pane(Fl_Group *pane)
{
    pane->begin();
    int x = pane->x();
    int y = pane->y();
    int w = pane->w();
    int h = pane->h();

    m_searchFor = new Fl_Input(x + 10, y + 20, w - 20, 26, "Search for: ");
    m_searchFor->align(FL_ALIGN_TOP_LEFT);
    m_searchFor->callback(search_cb, this);

    m_excludeFromSearch = new Fl_Input(x + 10, y + 70, w - 20, 26, "Exclude: ");
    m_excludeFromSearch->align(FL_ALIGN_TOP_LEFT);
    m_excludeFromSearch->callback(search_cb, this);

    m_caseSensitive = new Fl_Check_Button(x + 10, y + 100, w - 20, 26, "Case sensitive");
    m_caseSensitive->callback(search_cb, this);
    m_regex = new Fl_Check_Button(x + 10, y + 130, w - 20, 26, "Regular expression");
    m_regex->callback(search_cb, this);

    Fl_Box* resizableBox = new Fl_Box(x, y + h - 1, w, 1);
    pane->end();
    pane->resizable(resizableBox);
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
    try
    {
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

            if(!m_preferences.m_showHiddenFiles && *entry.path().filename().u8string().begin() == '.')
                continue;

            if(filter_file(entry.path()))
                continue;
            
            ListviewFile* item = new ListviewFile(entry.path(), fileSize);
            m_listview->add_item(item);
            m_filesInDir++;
        }
    }
    catch(const std::filesystem::filesystem_error& e)
    {
        fl_message_title("Error");
        fl_alert("Error: %s", e.what());
        navigate_to_dir(m_currentPath);

        return false;
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
    if(m_preferences.m_terminalEmulator.empty())
    {
        fl_message_title("No terminal emulator set");
        fl_alert("Please select a terminal emulator first.");
        return false;
    }

    GAppInfo* gappinfo;

    GError* error = nullptr;
    gappinfo = g_app_info_create_from_commandline(m_preferences.m_terminalEmulator.c_str(), nullptr, G_APP_INFO_CREATE_NONE, &error);
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

bool Filemon::filter_file(const std::filesystem::path &path)
{
    std::string searchFor = m_searchFor->value();
    std::string excludeFromSearch = m_excludeFromSearch->value();
    bool caseSensitive = m_caseSensitive->value();
    bool regex = m_regex->value();

    if(searchFor.empty() && excludeFromSearch.empty())
    {
        return false;
    }

    std::string filename = path.filename().string();

    // Check exclude patterns first
    if (!excludeFromSearch.empty()) 
    {
        try 
        {
            if (regex) 
            {
                std::regex excludeRegex(excludeFromSearch, 
                    caseSensitive ? std::regex_constants::ECMAScript : 
                                   std::regex_constants::ECMAScript | std::regex_constants::icase);
                if (std::regex_search(filename, excludeRegex)) 
                {
                    return true; // Filter out (exclude) this file
                }
            } 
            else 
            {
                std::string searchStr = excludeFromSearch;
                std::string fileStr = filename;
                
                if (!caseSensitive) 
                {
                    std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);
                    std::transform(fileStr.begin(), fileStr.end(), fileStr.begin(), ::tolower);
                }
                
                if (fileStr.find(searchStr) != std::string::npos) 
                {
                    return true; // Filter out (exclude) this file
                }
            }
        } 
        catch (const std::regex_error& e) 
        {
            // If regex is invalid, fall back to literal search
            std::string searchStr = excludeFromSearch;
            std::string fileStr = filename;
            
            if (!caseSensitive) 
            {
                std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);
                std::transform(fileStr.begin(), fileStr.end(), fileStr.begin(), ::tolower);
            }
            
            if (fileStr.find(searchStr) != std::string::npos) 
            {
                return true; // Filter out (exclude) this file
            }
        }
    }

    // If searchFor is empty, we only care about exclusions
    if (searchFor.empty()) 
    {
        return false;
    }

    // Check inclusion patterns
    try 
    {
        if (regex) 
        {
            std::regex searchRegex(searchFor,
                caseSensitive ? std::regex_constants::ECMAScript :
                               std::regex_constants::ECMAScript | std::regex_constants::icase);
            if (std::regex_search(filename, searchRegex)) 
            {
                return false; // Don't filter out (include) this file
            }
        } 
        else 
        {
            std::string searchStr = searchFor;
            std::string fileStr = filename;
            
            if (!caseSensitive) 
            {
                std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);
                std::transform(fileStr.begin(), fileStr.end(), fileStr.begin(), ::tolower);
            }
            
            if (fileStr.find(searchStr) != std::string::npos) 
            {
                return false; // Don't filter out (include) this file
            }
        }
    } 
    catch (const std::regex_error& e) 
    {
        // If regex is invalid, fall back to literal search
        std::string searchStr = searchFor;
        std::string fileStr = filename;
        
        if (!caseSensitive) 
        {
            std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);
            std::transform(fileStr.begin(), fileStr.end(), fileStr.begin(), ::tolower);
        }
        
        if (fileStr.find(searchStr) != std::string::npos) 
        {
            return false; // Don't filter out (include) this file
        }
    }

    // If we have a search pattern but didn't match, filter out
    return !searchFor.empty();
}

void Filemon::popup_menu()
{
    int numberSelected = m_listview->get_selected().size();

    Fl_Menu_Item* popup;

    std::string mime;
    GList* canBeOpenedWith = nullptr;

    if(numberSelected == 0)
    {
        popup = new Fl_Menu_Item[19]
        {
            {"Open Terminal", 0, Filemon::popup_cb, (void*)MENU_FILE_OPEN_TERMINAL, FL_MENU_DIVIDER},
            {"Create new", 0, nullptr, 0, FL_SUBMENU | FL_MENU_DIVIDER},
                {"File", 0, Filemon::popup_cb, (void*)MENU_FILE_NEW_FILE},
                {"Folder", 0, Filemon::popup_cb, (void*)MENU_FILE_NEW_FOLDER},
                {0},
            {"Paste", 0, Filemon::popup_cb, (void*)MENU_EDIT_PASTE, FL_MENU_DIVIDER},
            {"Select all", 0, Filemon::popup_cb, (void*)MENU_EDIT_SELECT_ALL, FL_MENU_DIVIDER},
            {"Sort by", 0, nullptr, 0, FL_SUBMENU},
                {"Name", 0, Filemon::popup_cb, (void*)MENU_VIEW_SORT_BY_NAME, FL_MENU_RADIO | (m_listview->get_sorted_by_property() == -1 ? FL_MENU_VALUE : 0)},
                {"Date modified", 0, Filemon::popup_cb, (void*)MENU_VIEW_SORT_BY_DATE_MODIFIED, FL_MENU_RADIO | (m_listview->get_sorted_by_property() == 4 ? FL_MENU_VALUE : 0)},
                {"Permissions", 0, Filemon::popup_cb, (void*)MENU_VIEW_SORT_BY_PERMISSIONS, FL_MENU_RADIO | (m_listview->get_sorted_by_property() == 3 ? FL_MENU_VALUE : 0)},
                {"Group", 0, Filemon::popup_cb, (void*)MENU_VIEW_SORT_BY_GROUP, FL_MENU_RADIO | (m_listview->get_sorted_by_property() == 2 ? FL_MENU_VALUE : 0)},
                {"Owner", 0, Filemon::popup_cb, (void*)MENU_VIEW_SORT_BY_OWNER, FL_MENU_RADIO | (m_listview->get_sorted_by_property() == 1 ? FL_MENU_VALUE : 0)},
                {"Size", 0, Filemon::popup_cb, (void*)MENU_VIEW_SORT_BY_SIZE, FL_MENU_DIVIDER | FL_MENU_RADIO | (m_listview->get_sorted_by_property() == 0 ? FL_MENU_VALUE : 0)},
                {"Sort ascending", 0, Filemon::popup_cb, (void*)MENU_VIEW_SORT_ASCENDING, FL_MENU_RADIO | (m_listview->get_sort_direction() == 1 ? FL_MENU_VALUE : 0)},
                {"Sort descending", 0, Filemon::popup_cb, (void*)MENU_VIEW_SORT_DESCENDING, FL_MENU_RADIO | (m_listview->get_sort_direction() == 0 ? FL_MENU_VALUE : 0)},
                {0},
            {"Show hidden files", 0, Filemon::popup_cb, (void*)MENU_VIEW_SHOW_HIDDEN, FL_MENU_TOGGLE | (m_preferences.m_showHiddenFiles ? FL_MENU_VALUE : 0)},
            {0}
        };
    }
    else
    {
        bool openWithSubmenuEnabled = numberSelected == 1;
        if(numberSelected == 1)
        {
            ListviewFile* file = (ListviewFile*)m_listview->get_item(m_listview->get_selected()[0]);
            GFile* gfile = g_file_new_for_path(file->get_path().c_str());
            GFileInfo* gfileinfo = g_file_query_info(gfile, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
            mime = g_file_info_get_content_type(gfileinfo);
            g_object_unref(gfileinfo);
            g_object_unref(gfile);
            if(mime == "application/x-executable" || mime == "application/x-sharedlib" || mime == "application/x-pie-executable" || mime == "application/x-shared-object")
            {
                openWithSubmenuEnabled = false;
            }
            else if(mime == "inode/directory")
            {
                openWithSubmenuEnabled = false;
            }
        }
        popup = new Fl_Menu_Item[14]
        {
            {"Open file", 0, Filemon::popup_cb, (void*)MENU_FILE_OPEN_FILE, (numberSelected == 1 ? 0 : FL_MENU_INACTIVE)},
            {"Open with...", 0, Filemon::popup_cb, 0, FL_MENU_DIVIDER | FL_SUBMENU | (openWithSubmenuEnabled ? 0 : FL_MENU_INACTIVE)},
            {},
            {"Cut", 0, Filemon::popup_cb, (void*)MENU_EDIT_CUT},
            {"Copy", 0, Filemon::popup_cb, (void*)MENU_EDIT_COPY},
            {"Trash", 0, Filemon::popup_cb, (void*)MENU_EDIT_TRASH},
            {"Rename", 0, Filemon::popup_cb, (void*)MENU_EDIT_RENAME, FL_MENU_DIVIDER | (numberSelected == 1 ? 0 : FL_MENU_INACTIVE)},
            {"File properties", 0, Filemon::popup_cb, (void*)MENU_FILE_FILE_PROPERTIES},
            {}
        };
        if(openWithSubmenuEnabled)
        {
            canBeOpenedWith = g_app_info_get_all_for_type(mime.c_str());
            int i = 0;
            for(GList* l = canBeOpenedWith; l != nullptr; l = l->next) 
            {
                GAppInfo* appInfo = (GAppInfo*)l->data;
                std::string entry = "Open with.../" + std::string(g_app_info_get_name(appInfo));
                popup->add(entry.c_str(), 0, 0, (void*)(100 + i));
                i++;
                if(i == 5) break;
            }
        }
    }

    const Fl_Menu_Item* m = popup->popup(Fl::event_x(), Fl::event_y(), 0, 0, 0);
    if(m)
    {
        size_t userdata = (size_t)m->user_data();
        if(userdata >= 100)
        {
            GAppInfo* appInfo = (GAppInfo*)g_list_nth_data(canBeOpenedWith, userdata - 100);
            std::string uri = "file://" + ((ListviewFile*)m_listview->get_item(m_listview->get_selected()[0]))->get_path().string();
            GList* uris = nullptr;
            uris = g_list_append(uris, (gpointer)uri.c_str());
            g_app_info_launch_uris(appInfo, uris, nullptr, nullptr);
            g_list_free(uris);
        }
        else
            m->do_callback(this, m->user_data());
    }

    delete[] popup;

    if(canBeOpenedWith)
    {
        for(GList* l = canBeOpenedWith; l != nullptr; l = l->next) 
        {
            GAppInfo* appInfo = (GAppInfo*)l->data;
            g_object_unref(appInfo);
        }

        g_list_free(canBeOpenedWith);
    }
}

void Filemon::open_file_properties()
{
    std::vector<int> selected = m_listview->get_selected();
    std::vector<std::filesystem::path> paths;
    for(int i = 0; i < selected.size(); i++)
    {
        ListviewFile* file = (ListviewFile*)m_listview->get_item(selected[i]);
        paths.push_back(file->get_path());
    }
    if(paths.size() > 0)
    {
        open_properties_wnd(paths);
    }
}

void Filemon::new_file()
{
    int r;
    std::string filename = fl_input_str(r, 255, "Enter the new file name:", "");
    if(filename.empty())
        return;
    fl_message_title("New file");
    std::filesystem::path newFilePath = m_currentPath.string() + "/" + filename;
    if(r == 0)
    {
        fl_message_title("Error");
        if(std::filesystem::exists(newFilePath))
        {
            fl_alert(std::filesystem::is_directory(newFilePath) ? "A directory with that name already exists." : "A file with that name already exists.");

            return;
        }

        std::ofstream newFile(newFilePath);
        if(newFile.good())
        {
            newFile.close();
        }
        else
        {
            fl_alert("Error: %s", strerror(errno));
        }
    }
}

void Filemon::new_folder()
{
    int r;
    std::string foldername = fl_input_str(r, 255, "Enter the new folder name:", "");
    if(foldername.empty())
        return;
    fl_message_title("New folder");
    std::filesystem::path newFolderPath = m_currentPath.string() + "/" + foldername;
    if(r == 0)
    {
        fl_message_title("Error");
        if(std::filesystem::exists(newFolderPath))
        {
            fl_alert(std::filesystem::is_directory(newFolderPath) ? "A directory with that name already exists." : "A file with that name already exists.");

            return;
        }

        try
        {
            std::filesystem::create_directory(newFolderPath);
        }
        catch(const std::filesystem::filesystem_error& e)
        {
            fl_alert("Error: %s", e.what());
        }
    }
}

void Filemon::rename_selected()
{
    int r;
    fl_message_title("Rename");
    std::string filename = fl_input_str(r, 255, "Enter the new file name:", "");
    if(filename.empty())
        return;

    std::filesystem::path newFilePath = m_currentPath.string() + "/" + filename;
    if(r == 0)
    {
        fl_message_title("Error");
        if(std::filesystem::exists(newFilePath))
        {
            fl_alert(std::filesystem::is_directory(newFilePath) ? "A directory with that name already exists." : "A file with that name already exists.");

            return;
        }

        std::vector<int> selected = m_listview->get_selected();
        for(int i = 0; i < selected.size(); i++)
        {
            ListviewFile* file = (ListviewFile*)m_listview->get_item(selected[i]);
            std::filesystem::rename(file->get_path(), newFilePath);
        }
    }
}

void Filemon::select_all(bool select)
{
    for(int i = 0; i < m_listview->get_item_count(); i++)
    {
        m_listview->select_item(i, select);
    }
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
        fl_message_title("Error");
        fl_alert("Error: %s", error->message);
        g_error_free(error);
    }
    g_object_unref(gfile);
}

void Filemon::show_hidden_files(bool show)
{
    m_preferences.m_showHiddenFiles = show;
    navigate_to_dir(m_currentPath);
}

int Filemon::run()
{
    navigate_to_dir(std::filesystem::current_path());

    GMainLoop* loop = g_main_loop_new(nullptr, false);

    GVolumeMonitor* volumeMonitor = g_volume_monitor_get();

    g_signal_connect(volumeMonitor, "volume-added", G_CALLBACK(Filemon::volume_added_cb), this);
    g_signal_connect(volumeMonitor, "volume-removed", G_CALLBACK(Filemon::volume_removed_cb), this);
    g_signal_connect(volumeMonitor, "volume-changed", G_CALLBACK(Filemon::volume_changed_cb), this);
    g_signal_connect(volumeMonitor, "mount-added", G_CALLBACK(Filemon::mount_added_cb), this);
    g_signal_connect(volumeMonitor, "mount-removed", G_CALLBACK(Filemon::mount_removed_cb), this);

    GList* volumes = g_volume_monitor_get_volumes(volumeMonitor);

    for(GList* volume = volumes; volume != nullptr; volume = volume->next)
    {
        GVolume* gvolume = (GVolume*)volume->data;
        GDrive* gdrive = g_volume_get_drive(gvolume);

        std::string entry;
        if(gdrive != nullptr)
        {
            entry = std::string(g_drive_get_name(gdrive)) + "/" + g_volume_get_name(gvolume);
        }
        else 
            entry = g_volume_get_name(gvolume);

        DeviceTreeItem* item = new DeviceTreeItem(g_volume_get_mount(gvolume) != nullptr, m_volumesTree);
        item->user_data((void*)gvolume);
        item->label(g_volume_get_name(gvolume));
        m_volumesTree->add(entry.c_str(), item);

        //g_object_unref(gvolume);

        if(gdrive != nullptr)
            g_object_unref(gdrive);
    }
    g_list_free(volumes);

    do
    {
        g_main_context_iteration(g_main_loop_get_context(loop), false);
    } while (Fl::wait() != 0);

    g_main_loop_unref(loop);
    g_object_unref(volumeMonitor);

    for(auto const& entry : m_monitors)
    {
        g_object_unref(entry.second);
    }

    return 0;
}
