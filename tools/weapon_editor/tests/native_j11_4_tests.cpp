// J.11.4 — portable editor-camera regression tests.
// Does not substitute for native Windows GUI / high-DPI acceptance testing.
#include "pxwe_preview.h"
#include "native_preview_fixtures.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace pxwe;
namespace f = preview_fixture;
static void Require(bool pass,const char* reason) { if (!pass) throw std::runtime_error(reason); }
static void Reject(const std::function<void()>& operation) {
    bool rejected = false; try { operation(); } catch (const std::exception&) { rejected = true; }
    Require(rejected,"invalid camera setting accepted");
}
int main() {
    const auto root = fs::temp_directory_path() / "pxwe-j114-camera-regression";
    std::error_code ec; fs::remove_all(root,ec); fs::create_directories(root);
    try {
        f::Put(root/"models/v_sample.mdl", f::Model());
        StudioPreview preview; preview.Load(root,"models/v_sample.mdl");
        PreviewCamera cam;
        const auto original = preview.Render(220,200,0,0,cam).pixels;
        cam.Rotate(17,-11);
        Require(preview.Render(220,200,0,0,cam).pixels!=original,"orbit drag did not rotate");
        const auto rotated=preview.Render(220,200,0,0,cam).pixels;
        cam.Pan(15,20,220,200);
        Require(preview.Render(220,200,0,0,cam).pixels!=rotated,"pan did not translate");
        cam.Fit(); Require(cam.panX==0 && cam.panY==0,"fit did not recenter");
        cam.ZoomBy(3);
        Require(preview.Render(220,200,0,0,cam).pixels!=rotated,"zoom did not affect preview");
        f::Put(root/"models/v_eye.mdl",f::Model(true));preview.Load(root,"models/v_eye.mdl");
        cam.SetPreset(CameraPreset::FirstPerson);
        const auto fps=preview.Render(220,200,0,0,cam).pixels;
        cam.SetFov(35);
        Require(preview.Render(220,200,0,0,cam).pixels!=fps,"FOV did not affect perspective");
        for (const auto preset: {CameraPreset::Front,CameraPreset::Back,CameraPreset::Left,CameraPreset::Right,CameraPreset::Top,CameraPreset::Bottom}) {
            cam.SetPreset(preset);
            Require(!cam.Perspective(),"orthographic preset is perspective");
            const auto img=preview.Render(220,200,0,0,cam);
            Require(img.pixels.size()==220u*200u,"invalid preset render dimensions");
        }
        preview.Load(root,"models/v_sample.mdl");
        cam.SetPreset(CameraPreset::Orbit); cam.SetScale(2);
        const auto twice=preview.Render(220,200,0,0,cam).pixels;
        cam.SetScale(1);
        Require(preview.Render(220,200,0,0,cam).pixels!=twice,"display-only scale not applied");
        const auto untouched=ReadResource(root,"models/v_sample.mdl");
        cam.SetScale(3);cam.ZoomBy(-8);cam.Pan(20,15,220,200);cam.Fit();cam.Reset();
        Require(cam.previewScale==1 && cam.zoom==1 && cam.panX==0 && cam.preset==CameraPreset::Orbit,"reset not complete");
        Require(ReadResource(root,"models/v_sample.mdl")==untouched,"camera edited model bytes");
        Reject([&]{cam.SetScale(0);});Reject([&]{cam.SetScale(9);});
        Reject([&]{cam.SetScale(std::numeric_limits<double>::quiet_NaN());});
        Reject([&]{cam.SetFov(25);});Reject([&]{cam.SetFov(111);});
        Reject([&]{cam.SetFov(std::numeric_limits<double>::infinity());});
        cam.zoom = std::numeric_limits<double>::infinity();
        Reject([&]{preview.Render(220,200,0,0,cam);});
        cam.Reset();
        Require(preview.Render(220,200,0,0,cam).pixels==original,"reset differs from original camera");
        fs::remove_all(root,ec);
        std::cout << "PASS J.11.4 camera controls, render modes, bounds and editor-only state\n";
        return 0;
    } catch (const std::exception& error) {
        fs::remove_all(root,ec);std::cerr << "FAIL J.11.4: " << error.what() << '\n';return 1;
    }
}
