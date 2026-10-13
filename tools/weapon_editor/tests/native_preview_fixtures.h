// Deterministic, redistributable Studio / sprite fixtures (no game assets).
#pragma once
#include "pxwe_preview.h"
#include <cstring>
#include <fstream>
namespace preview_fixture {
using pxwe::Bytes;
inline size_t Reserve(Bytes& b,size_t n) {const size_t p=b.size();b.resize(p+n);return p;}
inline void I(Bytes& b,size_t p,int n) {const auto v=static_cast<uint32_t>(n);for(int i=0;i<4;++i)b.at(p+size_t(i))=uint8_t(v>>(8*i));}
inline size_t Get(const Bytes& b,size_t p) {return size_t(b.at(p))|(size_t(b.at(p+1))<<8)|(size_t(b.at(p+2))<<16)|(size_t(b.at(p+3))<<24);}
inline void F(Bytes& b,size_t p,float f) {uint32_t v;std::memcpy(&v,&f,4);I(b,p,static_cast<int32_t>(v));}
inline void S(Bytes& b,size_t p,int n) {b.at(p)=uint8_t(n);b.at(p+1)=uint8_t(n>>8);}
inline void L(Bytes& b,size_t p,const char* label) {std::memcpy(b.data()+p,label,std::strlen(label));}
inline Bytes Model(bool viewmodel=false) {
    Bytes b(244);L(b,0,"IDST");I(b,4,10);L(b,8,"pxwe colored flag");
    for(int i=0;i<3;++i) {F(b,88+size_t(i)*4,-12);F(b,100+size_t(i)*4,18);F(b,112+size_t(i)*4,-12);F(b,124+size_t(i)*4,18);}
    const size_t bone=Reserve(b,112);I(b,140,1);I(b,144,int(bone));L(b,bone,"root");I(b,bone+32,-1);
    for(int i=0;i<6;++i) {I(b,bone+40+size_t(i)*4,-1);F(b,bone+88+size_t(i)*4,1);}
    const size_t seq=Reserve(b,352);I(b,164,2);I(b,168,int(seq));
    const size_t group=Reserve(b,104);I(b,172,1);I(b,176,int(group));L(b,group,"default");
    const size_t anim=Reserve(b,24);const size_t run=Reserve(b,8);S(b,anim+12,int(run-anim-12));b[run]=3;b[run+1]=3;S(b,run+2,0);S(b,run+4,6);S(b,run+6,0);
    for(int i=0;i<2;++i) {const size_t p=seq+size_t(i)*176;L(b,p,i?"translate":"idle");F(b,p+32,2);I(b,p+56,i?3:1);I(b,p+120,1);I(b,p+124,int(anim)+i*12);
        for(int axis=0;axis<3;++axis) {F(b,p+96+size_t(axis)*4,-12);F(b,p+108+size_t(axis)*4,18);}}
    const size_t tex=Reserve(b,160);I(b,180,2);I(b,184,int(tex));
    for(int t=0;t<2;++t) {const size_t p=tex+size_t(t)*80;L(b,p,t?"blue_yellow":"red_green");I(b,p+68,2);I(b,p+72,2);
        const size_t data=Reserve(b,4+768);I(b,p+76,int(data));b[data]=0;b[data+1]=1;b[data+2]=1;b[data+3]=0;
        if(t) {b[data+4+2]=255;b[data+4+3]=255;b[data+4+4]=255;}else{b[data+4]=255;b[data+4+4]=255;}}
    I(b,192,1);I(b,196,2);const size_t skins=Reserve(b,4);I(b,200,int(skins));S(b,skins,0);S(b,skins+2,1);
    const size_t part=Reserve(b,76),model=Reserve(b,112);I(b,204,1);I(b,208,int(part));L(b,part,"flag");I(b,part+64,1);I(b,part+68,1);I(b,part+72,int(model));L(b,model,"flag_mesh");
    const size_t mesh=Reserve(b,20);I(b,model+72,1);I(b,model+76,int(mesh));I(b,mesh,2);I(b,mesh+8,0);
    const size_t verts=Reserve(b,48),ids=Reserve(b,4),normal=Reserve(b,12),normalId=Reserve(b,1);
    I(b,model+80,4);I(b,model+84,int(ids));I(b,model+88,int(verts));I(b,model+92,1);I(b,model+96,int(normalId));I(b,model+100,int(normal));F(b,normal+4,1);
    const float orbitPoints[4][3]={{-10,0,-10},{10,0,-10},{10,0,10},{-10,0,10}};
    const float eyePoints[4][3]={{20,-10,-15},{20,10,-15},{20,10,-2},{20,-10,-2}};
    const auto &points=viewmodel?eyePoints:orbitPoints;
    for(int v=0;v<4;++v)for(int a=0;a<3;++a)F(b,verts+size_t(v)*12+size_t(a)*4,points[v][a]);
    const size_t commands=Reserve(b,36);I(b,mesh+4,int(commands));S(b,commands,-4);
    const int uv[4][2]={{0,0},{2,0},{2,2},{0,2}};
    for(int v=0;v<4;++v){const size_t p=commands+2+size_t(v)*8;S(b,p,v);S(b,p+2,0);S(b,p+4,uv[v][0]);S(b,p+6,uv[v][1]);}
    I(b,72,int(b.size()));return b;
}
inline Bytes Sprite(int format=3) {
    Bytes b(42+768);L(b,0,"IDSP");I(b,4,2);I(b,8,2);I(b,12,format);F(b,16,2);I(b,20,2);I(b,24,2);I(b,28,2);S(b,40,256);
    b[42]=255;b[46]=255;b[42+255*3]=0;b[42+255*3+1]=0;b[42+255*3+2]=255;
    for(int f=0;f<2;++f) {const size_t p=Reserve(b,24);I(b,p,0);I(b,p+4,-1);I(b,p+8,1);I(b,p+12,2);I(b,p+16,2);b[p+20]=uint8_t(f);b[p+21]=255;b[p+22]=uint8_t(1-f);b[p+23]=0;}
    return b;
}
inline Bytes Pack(const std::vector<std::pair<std::string,Bytes>>& files) {
    Bytes b(12);L(b,0,"PACK");std::vector<std::pair<size_t,size_t>> spans;
    for(const auto& f:files){const size_t p=Reserve(b,f.second.size());std::copy(f.second.begin(),f.second.end(),b.begin()+p);spans.emplace_back(p,f.second.size());}
    const size_t directory=Reserve(b,files.size()*64);I(b,4,int(directory));I(b,8,int(files.size()*64));
    for(size_t i=0;i<files.size();++i){const size_t p=directory+i*64;L(b,p,files[i].first.c_str());I(b,p+56,int(spans[i].first));I(b,p+60,int(spans[i].second));}return b;
}
inline void Put(const pxwe::fs::path& path,const Bytes& b) {pxwe::fs::create_directories(path.parent_path());std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));if(!f)throw std::runtime_error("fixture write failed");}
inline void Put(const pxwe::fs::path& path,const std::string& s) {Put(path,Bytes(s.begin(),s.end()));}
inline void Ppm(const pxwe::fs::path& path,const pxwe::Image& image) {
    std::ofstream out(path,std::ios::binary);out<<"P6\n"<<image.width<<' '<<image.height<<"\n255\n";
    for(auto pixel:image.pixels){const char rgb[3]={char(pixel>>16),char(pixel>>8),char(pixel)};out.write(rgb,3);}
}
} // namespace preview_fixture
