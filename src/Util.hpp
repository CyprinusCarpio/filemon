#ifndef UTIL_H
#define UTIL_H
#include <string>
#include <filesystem>

std::string file_size_to_string(size_t bytes);
std::string get_mime_type(std::filesystem::path& path);
#endif