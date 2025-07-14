#include "File.hpp"
#include "Util.hpp"

#include "../icons/folder16.xpm"
#include "../icons/folder32.xpm"
#include "../icons/img32.xpm"
#include "../icons/img16.xpm"
#include "../icons/bin32.xpm"
#include "../icons/bin16.xpm"

#include <FL/fl_draw.H>

#include <grp.h>
#include <pwd.h>
#include <iomanip>

#include "xdgmime.h"

bool ListviewFile::is_greater(Fle_Listview_Item* other, int property)
{
    ListviewFile* o = (ListviewFile*)other;
    if(property == FILE_PROPERTY_NAME)
    {
        if(m_size == -1 && o->m_size != -1)
        {
            return false;
        }
        if(m_size != -1 && o->m_size == -1)
        {
            return true;
        }
        return m_path > o->m_path;
    }
    else if (property == FILE_PROPERTY_SIZE)
    {
        if(m_size == -1 && o->m_size == -1)
        {
            return m_path > o->m_path;
        }
        return m_size > o->m_size;
    }
    else if(property == FILE_PROPERTY_GROUP)
    {
        std::string groupThis = getgrgid(m_stat.st_gid)->gr_name;
        std::string groupOther = getgrgid(o->m_stat.st_gid)->gr_name;
        if(groupThis == groupOther)
        {
            return m_path > o->m_path;
        }
        return groupThis > groupOther;
    }
    else if(property == FILE_PROPERTY_OWNER)
    {
        std::string ownerThis = getpwuid(m_stat.st_uid)->pw_name;
        std::string ownerOther = getpwuid(o->m_stat.st_uid)->pw_name;
        if(ownerThis == ownerOther)
        {
            return m_path > o->m_path;
        }
        return ownerThis > ownerOther;
    }
    else if(property == FILE_PROPERTY_MODIFIED)
    {
        return m_stat.st_mtime > o->m_stat.st_mtime;
    }
    return Fle_Listview_Item::is_greater(other, property);
}

void ListviewFile::draw_property(int i, int X, int Y, int W, int H)
{
    fl_color(textcolor());
    std::string s;
    if(i == FILE_PROPERTY_SIZE && m_size != -1)
    {
        s = file_size_to_string(m_size);
    }
    else if(i == FILE_PROPERTY_OWNER)
    {
        s = getpwuid(m_stat.st_uid)->pw_name;
    }
    else if(i == FILE_PROPERTY_GROUP)
    {
        s = getgrgid(m_stat.st_gid)->gr_name;
    }
    else if(i == FILE_PROPERTY_PERMISSIONS)
    {
        s = "----------";
        s[0] = m_size == -1 ? 'd' : '-';
        s[1] = (m_stat.st_mode & S_IRUSR) ? 'r' : '-';
        s[2] = (m_stat.st_mode & S_IWUSR) ? 'w' : '-';
        s[3] = (m_stat.st_mode & S_IXUSR) ? 'x' : '-';
        s[4] = (m_stat.st_mode & S_IRGRP) ? 'r' : '-';
        s[5] = (m_stat.st_mode & S_IWGRP) ? 'w' : '-';
        s[6] = (m_stat.st_mode & S_IXGRP) ? 'x' : '-';
        s[7] = (m_stat.st_mode & S_IROTH) ? 'r' : '-';
        s[8] = (m_stat.st_mode & S_IWOTH) ? 'w' : '-';
        s[9] = (m_stat.st_mode & S_IXOTH) ? 'x' : '-';

        fl_font(FL_COURIER, 14);
    }
    else if(i == FILE_PROPERTY_MODIFIED)
    {
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&m_stat.st_mtime), "%Y-%m-%d %H:%M");
        s = oss.str();

        fl_font(FL_COURIER, 11);
    }
    fl_draw(s.c_str(), X, Y, W, H, FL_ALIGN_LEFT);
}

ListviewFile::ListviewFile(std::string name, long size, std::filesystem::path path): Fle_Listview_Item(name.c_str()), m_size(size), m_path(path) 
{
    static Fl_Pixmap* folder32 = new Fl_Pixmap(folder32_xpm);
    static Fl_Pixmap* folder16 = new Fl_Pixmap(folder16_xpm);
    static Fl_Pixmap* img32 = new Fl_Pixmap(img32_xpm);
    static Fl_Pixmap* img16 = new Fl_Pixmap(img16_xpm);
    static Fl_Pixmap* bin32 = new Fl_Pixmap(bin32_xpm);
    static Fl_Pixmap* bin16 = new Fl_Pixmap(bin16_xpm);

    stat(m_path.c_str(), &m_stat);
    if(m_size == -1)
    {
        set_icon(folder16, folder32);
        return;
    }
    
    std::string mime = xdg_mime_get_mime_type_for_file(m_path.c_str(), &m_stat);

    if (mime == "application/x-executable" || mime == "application/x-sharedlib" || mime == "application/x-pie-executable" || mime == "application/x-shared-object")
    {
        set_icon(bin16, bin32);
    }
    else if(mime.substr(0, 5) == "image")
    {
        set_icon(img16, img32);
    }
}