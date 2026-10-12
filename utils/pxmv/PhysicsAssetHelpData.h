#pragma once
#include <cstddef>
#include <string>
#include <vector>

// The single source of truth for editor, generator and tuning help.
// Records are intentionally plain C++ so portable tests can audit coverage
// without Qt, mxToolKit or the PhysX SDK.
namespace physhelp
{
struct Entry
{
    const char *section;
    const char *name;
    const char *units;
    const char *brief;
    const char *details;
    const char *example;
};
const std::vector<Entry> &Catalog();
const Entry *Find(const std::string &section, const std::string &name);
std::vector<std::string> Sections();
std::vector<std::string> Wrap(const std::string &text, std::size_t width);
}
