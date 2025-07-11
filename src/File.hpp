#ifndef FILE_H
#define FILE_H

#include <FLE/Fle_Listview_Item.hpp>

#include <filesystem>
#include <sys/stat.h>

enum FileProperty
{
    FILE_PROPERTY_NAME = -1,
    FILE_PROPERTY_SIZE,
    FILE_PROPERTY_OWNER,
    FILE_PROPERTY_GROUP,
    FILE_PROPERTY_PERMISSIONS,
    FILE_PROPERTY_MODIFIED,
};

class ListviewFile: public Fle_Listview_Item
{
    long m_size;
    std::filesystem::path m_path;
    struct stat m_stat;
protected:
    bool is_greater(Fle_Listview_Item* other, int property) override;

    void draw_property(int i, int X, int Y, int W, int H) override;
public:
    ListviewFile(std::string name, long size, std::filesystem::path path);
    bool is_folder() { return m_size == -1; }
    long size() { return m_size; }
    std::filesystem::path get_path() { return m_path; }
};

#endif