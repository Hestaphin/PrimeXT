// Synchronous damage attribution. The selected icon belongs to the shot, even
// when its owner has switched weapons before a delayed impact.
#pragma once
#include "modular_asset_selection.h"
#include <utility>
struct ModularDamageSource {
    const void* owner = nullptr;
    const modular::Weapon* weapon = nullptr;
    std::string killSprite;
};
inline const ModularDamageSource* g_modularDamageSource = nullptr;
class ScopedModularDamage {
public:
    explicit ScopedModularDamage(ModularDamageSource source)
        : source_(std::move(source)), previous_(g_modularDamageSource) { g_modularDamageSource = &source_; }
    ~ScopedModularDamage() { g_modularDamageSource = previous_; }
    ScopedModularDamage(const ScopedModularDamage&) = delete;
    ScopedModularDamage& operator=(const ScopedModularDamage&) = delete;
private:
    ModularDamageSource source_;
    const ModularDamageSource* previous_;
};
