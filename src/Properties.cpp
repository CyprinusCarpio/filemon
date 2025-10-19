#include "Properties.hpp"
#include "Util.hpp"

#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Output.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Check_Button.H>
#include <FL/fl_ask.H> 

#include <string>
#include <set>
#include <iostream>

#include <pwd.h>
#include <grp.h>
#include <unistd.h>

struct Properties_WND_Pointers
{
    std::string path;
    struct stat st;
    Fl_Input* ownerInput;
    Fl_Input* groupInput;
    Fl_Check_Button* ownerReadCheckButton;
    Fl_Check_Button* ownerWriteCheckButton;
    Fl_Check_Button* ownerExecuteCheckButton;
    Fl_Check_Button* groupReadCheckButton;
    Fl_Check_Button* groupWriteCheckButton;
    Fl_Check_Button* groupExecuteCheckButton;
    Fl_Check_Button* otherReadCheckButton;
    Fl_Check_Button* otherWriteCheckButton;
    Fl_Check_Button* otherExecuteCheckButton;
    Fl_Check_Button* applyRecursively;
};

void close_cb(Fl_Widget* w, void* data)
{
    Fl_Double_Window* wnd;

    if(data == (void*)1)
    {
        wnd = (Fl_Double_Window*)w->window();
    }
    else
        wnd = (Fl_Double_Window*)w;
    
    Properties_WND_Pointers* extra = (Properties_WND_Pointers*)wnd->user_data();

    wnd->hide();

    delete extra;
    Fl::delete_widget(wnd);
}

void apply_cb(Fl_Widget* w, void* data)
{
    Fl_Double_Window* wnd = (Fl_Double_Window*)w->window();
    Properties_WND_Pointers* extra = (Properties_WND_Pointers*)wnd->user_data();

    bool applyRecursively = extra->applyRecursively->value();

    if(strcmp(extra->ownerInput->value(), getpwuid(extra->st.st_uid)->pw_name) != 0)
    {
        passwd* user = getpwnam(extra->ownerInput->value());
        if(user && chown(extra->path.c_str(), user->pw_uid, extra->st.st_gid) != 0)
        {
            fl_message_title("Error");
            fl_alert("Failed to change owner");
        }
    }

    if(strcmp(extra->groupInput->value(), getgrgid(extra->st.st_gid)->gr_name) != 0)
    {
        group* group = getgrnam(extra->groupInput->value());
        if(group && chown(extra->path.c_str(), extra->st.st_uid, group->gr_gid) != 0)
        {
            fl_message_title("Error");
            fl_alert("Failed to change group");
        }
    }

    if(extra->ownerReadCheckButton->value()) { extra->st.st_mode |= S_IRUSR; } else { extra->st.st_mode &= ~S_IRUSR; }
    if(extra->ownerWriteCheckButton->value()) { extra->st.st_mode |= S_IWUSR; } else { extra->st.st_mode &= ~S_IWUSR; }
    if(extra->ownerExecuteCheckButton->value()) { extra->st.st_mode |= S_IXUSR; } else { extra->st.st_mode &= ~S_IXUSR; }
    if(extra->groupReadCheckButton->value()) { extra->st.st_mode |= S_IRGRP; } else { extra->st.st_mode &= ~S_IRGRP; }
    if(extra->groupWriteCheckButton->value()) { extra->st.st_mode |= S_IWGRP; } else { extra->st.st_mode &= ~S_IWGRP; }
    if(extra->groupExecuteCheckButton->value()) { extra->st.st_mode |= S_IXGRP; } else { extra->st.st_mode &= ~S_IXGRP; }
    if(extra->otherReadCheckButton->value()) { extra->st.st_mode |= S_IROTH; } else { extra->st.st_mode &= ~S_IROTH; }
    if(extra->otherWriteCheckButton->value()) { extra->st.st_mode |= S_IWOTH; } else { extra->st.st_mode &= ~S_IWOTH; }
    if(extra->otherExecuteCheckButton->value()) { extra->st.st_mode |= S_IXOTH; } else { extra->st.st_mode &= ~S_IXOTH; }

    if(chmod(extra->path.c_str(), extra->st.st_mode) != 0)
    {
        fl_message_title("Error");
        fl_alert("Failed to change permissions");
    }

    w->hide();
    delete extra;
    Fl::delete_widget(wnd);
}


void open_properties_wnd(std::vector<std::filesystem::path> paths)
{
    std::set<std::string> mimetypes;

    for(std::filesystem::path& path : paths)
    {
        mimetypes.insert(get_mime_type(path));
    }

    size_t totalSize = 0;
    for(std::filesystem::path& path : paths)
    {
        if(std::filesystem::is_directory(path))
        {
            continue;
        }
        totalSize += std::filesystem::file_size(path);
    }

    std::string createdString = "";
    std::string modifiedString = "";
    Properties_WND_Pointers* extra = new Properties_WND_Pointers;
    struct stat st;

    if(paths.size() == 1)
    {
        stat(paths[0].c_str(), &st);
        extra->st = st;
        extra->path = paths[0].u8string();

        std::ostringstream oss;
        oss << std::put_time(std::localtime(&st.st_ctime), "%Y-%m-%d %H:%M");
        createdString = oss.str();
        oss.str("");
        oss << std::put_time(std::localtime(&st.st_mtime), "%Y-%m-%d %H:%M");
        modifiedString = oss.str();
    }

    Fl_Double_Window* wnd = new Fl_Double_Window(320, 260, "Properties");
    wnd->callback(close_cb, nullptr);
    wnd->set_non_modal();
    wnd->user_data(extra);

    Fl_Button* cancel = new Fl_Button(245, 229, 70, 26, "Cancel");
    cancel->callback(close_cb, (void*)1);

    Fl_Button* apply = new Fl_Button(170, 229, 70, 26, "Apply");
    apply->callback(apply_cb, nullptr);

    Fl_Tabs* tabs = new Fl_Tabs(0, 0, 320, 225);

    Fl_Group* information = new Fl_Group(10, 26, 300, 175, "Information");
    Fl_Box* name = new Fl_Box(10, 30, 90, 26, "File name:"); name->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Output* nameOutput = new Fl_Output(90, 30, 225, 26, ""); nameOutput->value(paths.size() == 1 ? paths[0].filename().c_str() : "Multiple files selected");

    Fl_Box* path = new Fl_Box(10, 60, 90, 26, "Path:"); path->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Output* pathOutput = new Fl_Output(90, 60, 225, 26, ""); pathOutput->value(paths[0].parent_path().c_str());

    Fl_Box* type = new Fl_Box(10, 90, 90, 26, "Type:"); type->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Output* typeOutput = new Fl_Output(90, 90, 225, 26, ""); typeOutput->value(mimetypes.size() == 1 ? mimetypes.begin()->c_str() : "Multiple types selected");

    Fl_Box* size = new Fl_Box(10, 120, 90, 26, "Size:"); size->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Output* sizeOutput = new Fl_Output(90, 120, 225, 26, ""); sizeOutput->value(file_size_to_string(totalSize).c_str());

    Fl_Box* modified = new Fl_Box(10, 150, 90, 26, "Modified:"); modified->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Output* modifiedOutput = new Fl_Output(90, 150, 225, 26, ""); modifiedOutput->value(modifiedString.c_str());

    Fl_Box* created = new Fl_Box(10, 180, 90, 26, "Created:"); created->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    Fl_Output* createdOutput = new Fl_Output(90, 180, 225, 26, ""); createdOutput->value(createdString.c_str());
    information->end();

    if(paths.size() == 1)
    {
        Fl_Group* permissions = new Fl_Group(10, 26, 300, 175, "Permissions");
        
        Fl_Box* owner = new Fl_Box(10, 30, 90, 26, "Owner:"); owner->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        Fl_Input* ownerInput = new Fl_Input(90, 30, 225, 26, ""); ownerInput->value(getpwuid(st.st_uid)->pw_name);
        extra->ownerInput = ownerInput;

        Fl_Box* group = new Fl_Box(10, 60, 90, 26, "Group:"); group->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        Fl_Input* groupInput = new Fl_Input(90, 60, 225, 26, ""); groupInput->value(getgrgid(st.st_gid)->gr_name);
        extra->groupInput = groupInput;

        Fl_Box* ownerPerms = new Fl_Box(10, 100, 90, 26, "Owner:"); ownerPerms->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        Fl_Check_Button* ownerReadCheckButton = new Fl_Check_Button(90, 100, 26, 26, "Read");
        ownerReadCheckButton->value(st.st_mode & S_IRUSR);
        extra->ownerReadCheckButton = ownerReadCheckButton;
        Fl_Check_Button* ownerWriteCheckButton = new Fl_Check_Button(150, 100, 26, 26, "Write");
        ownerWriteCheckButton->value(st.st_mode & S_IWUSR);
        extra->ownerWriteCheckButton = ownerWriteCheckButton;
        Fl_Check_Button* ownerExecuteCheckButton = new Fl_Check_Button(210, 100, 26, 26, "Execute");
        ownerExecuteCheckButton->value(st.st_mode & S_IXUSR);
        extra->ownerExecuteCheckButton = ownerExecuteCheckButton;

        Fl_Box* groupPerms = new Fl_Box(10, 130, 90, 26, "Group:"); groupPerms->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        Fl_Check_Button* groupReadCheckButton = new Fl_Check_Button(90, 130, 26, 26, "Read");
        groupReadCheckButton->value(st.st_mode & S_IRGRP);
        extra->groupReadCheckButton = groupReadCheckButton;
        Fl_Check_Button* groupWriteCheckButton = new Fl_Check_Button(150, 130, 26, 26, "Write");
        groupWriteCheckButton->value(st.st_mode & S_IWGRP);
        extra->groupWriteCheckButton = groupWriteCheckButton;
        Fl_Check_Button* groupExecuteCheckButton = new Fl_Check_Button(210, 130, 26, 26, "Execute");
        groupExecuteCheckButton->value(st.st_mode & S_IXGRP);
        extra->groupExecuteCheckButton = groupExecuteCheckButton;

        Fl_Box* otherPerms = new Fl_Box(10, 160, 90, 26, "Other:"); otherPerms->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        Fl_Check_Button* otherReadCheckButton = new Fl_Check_Button(90, 160, 26, 26, "Read");
        otherReadCheckButton->value(st.st_mode & S_IROTH);
        extra->otherReadCheckButton = otherReadCheckButton;
        Fl_Check_Button* otherWriteCheckButton = new Fl_Check_Button(150, 160, 26, 26, "Write");
        otherWriteCheckButton->value(st.st_mode & S_IWOTH);
        extra->otherWriteCheckButton = otherWriteCheckButton;
        Fl_Check_Button* otherExecuteCheckButton = new Fl_Check_Button(210, 160, 26, 26, "Execute");
        otherExecuteCheckButton->value(st.st_mode & S_IXOTH);
        extra->otherExecuteCheckButton = otherExecuteCheckButton;

        Fl_Check_Button* applyRecursively = new Fl_Check_Button(10, 190, 300, 26, "Apply changes recursively");
        applyRecursively->value(false);
        extra->applyRecursively = applyRecursively;

        if(!std::filesystem::is_directory(paths[0]))
        {
            applyRecursively->hide();
        }
        else
        {
            ownerExecuteCheckButton->hide();
            groupExecuteCheckButton->hide();
            otherExecuteCheckButton->hide();
        }

        permissions->end();
    }

    tabs->end();

    wnd->show();
}
