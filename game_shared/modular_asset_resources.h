#pragma once
#include "modular_weapons.h"
#include <set>
#include <sstream>

namespace modular {
inline std::set<std::string> ConditionalAssetPaths(const Weapon& w) {
    std::set<std::string> paths;
    for (const auto& a : w.assets) paths.insert(a.path);
    for (const auto& a : w.ammoOverrides)
        for (const auto& path : {a.viewModel,a.playerModel,a.worldModel,a.fireSound,
                                 a.reloadSound,a.sprite,a.hudSprite,a.killSprite})
            if (!path.empty()) paths.insert(path);
    return paths;
}
inline std::set<std::string> SpriteListResources(const std::string& text) {
    std::set<std::string> paths;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input,line)) {
        const auto comment=line.find("//");
        if (comment != std::string::npos) line.resize(comment);
        std::istringstream row(line);
        std::string role,name; int resolution,x,y,width,height;
        if (!(row >> role >> resolution >> name >> x >> y >> width >> height)) continue;
        if (name.empty() || name.size() > 63 || name.find("..") != std::string::npos ||
            name.front() == '/' || name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-/") != std::string::npos) continue;
        paths.insert("sprites/" + name + ".spr");
    }
    return paths;
}
} // namespace modular
