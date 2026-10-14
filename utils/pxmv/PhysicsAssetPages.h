#pragma once
#include <mx/mxWidget.h>
#include <vector>
#include <charconv>
#include <string>

// Pages share the dialog's native parent, so toolkit action routing is unchanged.
// Hidden controls keep their values and are still validated before Apply/Save.
class PhysicsAssetPages
{
    struct Entry { mxWidget *control; unsigned pages; bool advanced; };
    std::vector<Entry> m_entries;
public:
    template<class T> T *Add(T *control, unsigned pages, bool advanced=false)
    { m_entries.push_back({control,pages,advanced}); return control; }
    void Show(int page, bool advanced)
    {
        const unsigned bit = page >= 0 && page < 32 ? 1u << page : 0;
        for(const auto &entry : m_entries)
            entry.control->setVisible((entry.pages & bit) && (!entry.advanced || advanced));
    }
};

inline std::string PhysicsUiNumber(float value)
{
    char text[64];
    const auto converted = std::to_chars(text, text+sizeof(text), value);
    return converted.ec == std::errc() ? std::string(text, converted.ptr) : std::string("0");
}
