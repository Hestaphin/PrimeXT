// Editor-only camera state; never serialized into weapon or model definitions.
// Copyright (C) 2026 PrimeXT contributors. GPL-3.0-or-later.
#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace pxwe {
enum class CameraPreset { Orbit, FirstPerson, Front, Back, Left, Right, Top, Bottom };
struct PreviewCamera {
    CameraPreset preset = CameraPreset::Orbit;
    double yaw = .65, tilt = .35; // radians
    double zoom = 1.0;            // camera distance adjustment
    double panX = 0.0, panY = 0.0; // fractions of viewport dimensions
    double previewScale = 1.0;    // display-only multiplier, never changes the model
    double fovDegrees = 70.0;
    bool Perspective() const { return preset == CameraPreset::Orbit || preset == CameraPreset::FirstPerson; }
    void SetPreset(CameraPreset value) {
        preset = value; panX = panY = 0; zoom = 1;
        switch (value) {
        case CameraPreset::Orbit: yaw = .65; tilt = .35; break;
        case CameraPreset::FirstPerson: yaw = 0; tilt = 0; fovDegrees = 68; break;
        case CameraPreset::Front: yaw = 0; tilt = 0; break;
        case CameraPreset::Back: yaw = 3.141592653589793; tilt = 0; break;
        case CameraPreset::Left: yaw = -1.5707963267948966; tilt = 0; break;
        case CameraPreset::Right: yaw = 1.5707963267948966; tilt = 0; break;
        case CameraPreset::Top: yaw = 0; tilt = 1.5707963267948966; break;
        case CameraPreset::Bottom: yaw = 0; tilt = -1.5707963267948966; break;
        }
    }
    void Rotate(double dx, double dy) {
        if (!std::isfinite(dx) || !std::isfinite(dy)) throw std::invalid_argument("invalid camera drag");
        // Dragging a named preset switches to free orbit instead of silently changing it.
        preset = CameraPreset::Orbit;
        yaw = std::remainder(yaw + dx * .01, 6.283185307179586);
        tilt = std::clamp(tilt + dy * .01, -1.5707963267948966, 1.5707963267948966);
    }
    void Pan(double dxPixels, double dyPixels, int width, int height) {
        if (!std::isfinite(dxPixels) || !std::isfinite(dyPixels) || width <= 0 || height <= 0) return;
        panX = std::clamp(panX + dxPixels / width, -2.0, 2.0);
        panY = std::clamp(panY + dyPixels / height, -2.0, 2.0);
    }
    void ZoomBy(int wheelNotches) {
        zoom = std::clamp(zoom * std::pow(1.12, wheelNotches), .05, 20.0);
    }
    void Fit() { zoom = 1.0 / previewScale; panX = panY = 0.0; }
    void Reset() { *this = PreviewCamera{}; }
    void SetFov(double value) {
        if (!std::isfinite(value) || value < 30 || value > 110) throw std::invalid_argument("preview FOV must be 30..110 degrees");
        fovDegrees = value;
    }
    void SetScale(double value) {
        if (!std::isfinite(value) || value < .1 || value > 8) throw std::invalid_argument("preview scale must be 0.1..8");
        previewScale = value;
    }
    void Validate() const {
        if (!std::isfinite(yaw) || !std::isfinite(tilt) || !std::isfinite(zoom) || zoom < .05 || zoom > 20 ||
            !std::isfinite(panX) || !std::isfinite(panY) || !std::isfinite(previewScale) || previewScale < .1 || previewScale > 8 ||
            !std::isfinite(fovDegrees) || fovDegrees < 30 || fovDegrees > 110)
            throw std::invalid_argument("invalid camera settings");
    }
};
// Preserve the viewport aspect ratio when bounding software raster dimensions.
inline std::pair<int,int> PreviewCanvasSize(int width,int height) {
    const double factor=1024.0/std::max({1024,width,height});
    return {std::max(32,int(std::lround(width*factor))),std::max(32,int(std::lround(height*factor)))};
}
} // namespace pxwe
