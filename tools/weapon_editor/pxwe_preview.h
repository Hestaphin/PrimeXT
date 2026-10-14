// Bounded GoldSrc resource readers and software previews; no window/engine API.
// Copyright (C) 2026 PrimeXT contributors. GPL-3.0-or-later.
#pragma once
#include "pxwe_project.h"
#include "pxwe_camera.h"
#include <cstdint>
#include <memory>

namespace pxwe {
using Bytes = std::vector<uint8_t>;
// Loose files precede pak*.pak (higher numbers first). Reads never leave root.
Bytes ReadResource(const fs::path& root, const std::string& relative);
struct Image {
    int width = 0, height = 0;
    std::vector<uint32_t> pixels; // 0xAARRGGBB (BGRA bytes for Win32 DIB)
};
struct StudioData;
class StudioPreview {
public:
    void Load(const fs::path& root, const std::string& relative);
    bool Loaded() const { return bool(data_); }
    const ModelInfo& Info() const;
    int SkinCount() const;
    int BodyCount() const;
    // Sequence 0..N-1, elapsed seconds, yaw/tilt radians; bounds stay stable.
    Image Render(int width, int height, int sequence, double seconds,
        double yaw, double tilt, int skin = 0, int body = 0) const;
    // J.11.4: adjustable orbit/orthographic/first-person preview, read-only to game files.
    Image Render(int width, int height, int sequence, double seconds,
        const PreviewCamera& camera, int skin = 0, int body = 0) const;
private:
    std::shared_ptr<const StudioData> data_;
};
struct SpriteFrame { Image image; double endSeconds = .1; int originX = 0, originY = 0; };
class SpritePreview {
public:
    void Load(const fs::path& root, const std::string& relative, int hudResolution = 640);
    const std::vector<SpriteFrame>& Frames() const { return frames_; }
    const SpriteFrame& Frame(double seconds) const;
    Image Render(int width, int height, double seconds) const;
private:
    std::vector<SpriteFrame> frames_;
    int format_ = 0;
};
enum class CurveMetric { Trajectory, Energy, Damage };
struct CurveSample { double distance = 0, height = 0, energy = 0, damage = 0; };
struct AmmoCurve { std::string id; double velocity = 0, muzzleDamage = 0; std::vector<CurveSample> samples; };
std::vector<AmmoCurve> ComparisonCurves(const modular::Registry& registry, Kind kind, const std::string& id);
} // namespace pxwe
