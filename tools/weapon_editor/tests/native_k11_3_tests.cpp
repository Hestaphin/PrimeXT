#include "pxwe_preview.h"
#include "native_preview_fixtures.h"
#include <iostream>
#include <stdexcept>
using namespace pxwe;
namespace f=preview_fixture;
static void Require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
static size_t Colored(const Image &image){size_t count=0;for(auto p:image.pixels)if(p==0xffff0000u||p==0xff00ff00u)++count;return count;}
static size_t Vertices(const f::Bytes &b){int part,model,vertices;std::memcpy(&part,b.data()+208,4);std::memcpy(&model,b.data()+part+72,4);std::memcpy(&vertices,b.data()+model+88,4);return size_t(vertices);}
int main(){
    const auto root=fs::temp_directory_path()/"pxwe-k113-eye";std::error_code ec;fs::remove_all(root,ec);
    try{
        auto bytes=f::Model(true);f::Put(root/"models/v_eye.mdl",bytes);StudioPreview preview;preview.Load(root,"models/v_eye.mdl");
        PreviewCamera camera;camera.SetPreset(CameraPreset::FirstPerson);
        Require(camera.fovDegrees==68,"first-person default must match pxmv weapon-origin FOV");
        const auto original=preview.Render(640,480,0,0,camera);Require(Colored(original)>0,"eye-origin model must be visible");
        size_t upper=0,lower=0;for(int y=0;y<480;++y)for(int x=0;x<640;++x){const auto p=original.pixels[size_t(y)*640+x];if(p==0xffff0000u||p==0xff00ff00u)(y<240?upper:lower)++;}
        Require(lower>0&&upper==0,"authored below-eye weapon must remain in the lower view");
        for(int axis=0;axis<3;++axis){f::F(bytes,112+axis*4,-500);f::F(bytes,124+axis*4,500);}
        f::Put(root/"models/v_bounds.mdl",bytes);preview.Load(root,"models/v_bounds.mdl");
        Require(preview.Render(640,480,0,0,camera).pixels==original.pixels,"viewmodel framing must ignore model bounding-box center and extent");
        const size_t vertices=Vertices(bytes);for(int i=0;i<4;++i)f::F(bytes,vertices+i*12,-20);
        f::Put(root/"models/v_behind.mdl",bytes);preview.Load(root,"models/v_behind.mdl");Require(Colored(preview.Render(640,480,0,0,camera))==0,"geometry behind the eye must be clipped");
        bytes=f::Model(true);const size_t at=Vertices(bytes);
        for(int i=0;i<4;++i){f::F(bytes,at+i*12,i<2?1:12);f::F(bytes,at+i*12+8,i<2?-2:4);}
        f::Put(root/"models/v_crossing.mdl",bytes);preview.Load(root,"models/v_crossing.mdl");Require(Colored(preview.Render(640,480,0,0,camera))>0,"triangles crossing the near plane must be clipped, not discarded");
        const auto size=PreviewCanvasSize(1920,1080);Require(size.first<=1024&&size.second<=1024&&std::abs(double(size.first)/size.second-1920./1080.)<.003,"raster cap must preserve viewport aspect");
        camera.Fit();Require(camera.panX==0&&camera.panY==0&&camera.zoom*camera.previewScale==1,"Fit must restore authored framing");
        Require(ReadResource(root,"models/v_eye.mdl")==f::Model(true),"preview cannot change source model bytes");
        fs::remove_all(root,ec);std::cout<<"PASS K.11.3 eye origin, pxmv FOV/shift, bounding independence, near clipping, aspect and read-only models\n";return 0;
    }catch(const std::exception &error){fs::remove_all(root,ec);std::cerr<<"FAIL K.11.3 "<<error.what()<<'\n';return 1;}
}
