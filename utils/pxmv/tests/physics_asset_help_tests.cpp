// Portable regression test: g++ -std=c++17 PhysicsAssetHelpData.cpp tests/physics_asset_help_tests.cpp -o /tmp/physhelp_test
#include "../PhysicsAssetHelpData.h"
#include <cassert>
#include <iostream>
#include <set>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    const auto &catalog = physhelp::Catalog();
    const auto sections = physhelp::Sections();
    assert(catalog.size() >= 90 && "Coverage regressed: missing detailed field instructions");
    assert(sections.size() == 7 && "Every UI surface plus onboarding/tools needs a section");
    std::set<std::string> keys;
    for(const auto &entry : catalog)
    {
        assert(entry.section && *entry.section && entry.name && *entry.name);
        assert(entry.units && *entry.units && entry.brief && *entry.brief);
        assert(entry.details && std::string(entry.details).size() >= 80);
        assert(entry.example && *entry.example);
        assert(keys.insert(std::string(entry.section)+"/"+entry.name).second && "Duplicate catalog field");
        assert(physhelp::Find(entry.section,entry.name)==&entry);
        for(const auto &line : physhelp::Wrap(std::string(entry.details),51))
            assert(line.size()<=51 && "Detail list would be clipped horizontally");
    }
    const std::vector<std::pair<const char*,const char*>> needed={
        {"Start here","What is a physics body?"},{"Start here","What is .phys?"},{"Start here","What is PhysX?"},
        {"Bodies","Shape"},{"Bodies","Mass"},{"Bodies","Hitgroup"},{"Bodies","Drive max force"},
        {"Joints","Hinge axis XYZ"},{"Joints","D6 anchor XYZ"},{"Joints","T / Y / Z motions"},
        {"Generator","Chain edges"},{"Generator","Expand branches"},{"Generator","Mass-ratio warn"},
        {"Generator","Frame mode"},{"Tuning","Continuous collision detection"},{"Tuning","Rest margin"},
        {"Tuning","Self collision"},{"Tools","Simulate / Reset"},{"Troubleshooting","Missing or extra generated body"}
    };
    for(const auto &key:needed) assert(physhelp::Find(key.first,key.second));
    if(argc>1 && std::string(argv[1])=="--markdown")
    {
        std::cout << "# Physics Asset Editor - integrated help reference (J.10.6.10.9)\n\n";
        std::cout << "This document mirrors the catalog compiled into pxmv. Open **Help / ?** in the main Physics Asset Editor, Ragdoll Template Generator, or Physics Settings. The Help window is available without a network connection.\n\n";
        std::string last;
        for(const auto &e:catalog)
        {
            if(last!=e.section){last=e.section;std::cout << "\n## "<<last<<"\n\n";}
            std::cout<<"### "<<e.name<<"\n\n**Brief:** "<<e.brief<<"\n\n**Units/values:** "<<e.units<<"\n\n"<<e.details<<"\n\n**Example:** "<<e.example<<"\n\n";
        }
    }
    else
        std::cout << "PASS " << catalog.size() << " documented fields/topics, " << sections.size() << " sections, all required examples and line wrapping\n";
}
