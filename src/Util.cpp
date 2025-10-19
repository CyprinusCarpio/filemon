#include "Util.hpp"

#include <cmath>

#include <glib-2.0/gio/gio.h>

std::string file_size_to_string(size_t bytes) 
{
    const char* units[] = {"B", "K", "M", "G", "T"};
    int unitIndex = 0;
    float bytesf = bytes;
    while (bytesf >= 1024 && unitIndex < 4) 
    {
        bytesf /= 1024;
        unitIndex++;
    }
    
    if(unitIndex == 0)
        return std::to_string(bytes) + " " + units[unitIndex];

    bytesf = std::round(bytesf * 10) / 10;
    std::string sizeStr = std::to_string(bytesf);\
    sizeStr.erase(sizeStr.find_last_of('.') + 2);
    return sizeStr + " " + units[unitIndex];
}

std::string get_mime_type(std::filesystem::path& path) 
{
    GFile* gfile = g_file_new_for_path(path.c_str());
    GFileInfo* gfileinfo = g_file_query_info(gfile, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
    std::string mime(g_file_info_get_content_type(gfileinfo));
    g_object_unref(gfileinfo);
    g_object_unref(gfile);

    return mime;
}