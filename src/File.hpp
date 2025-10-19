#ifndef FILE_H
#define FILE_H

#include <FLE/Fle_Listview_Item.hpp>

#include <filesystem>
#include <string>
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
    std::string m_mimetype;
    
protected:
    bool is_greater(Fle_Listview_Item* other, int property) override;

    void draw_property(int i, int X, int Y, int W, int H) override;
public:
    ListviewFile(const std::filesystem::path& path, long size);
    ~ListviewFile();


    bool is_folder() const { return m_size == -1; }
    long size() const { return m_size; }
    void size(long size) { m_size = size; }
    std::filesystem::path get_path() const { return m_path; }
};

#endif