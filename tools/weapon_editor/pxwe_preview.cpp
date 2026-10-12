// Copyright (C) 2026 PrimeXT contributors. GPL-3.0-or-later.
#include "pxwe_preview.h"
#include "cartridge_damage.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace pxwe {
namespace {
constexpr size_t kResourceLimit = 64 * 1024 * 1024;
[[noreturn]] void Bad(const std::string& what) { throw std::runtime_error("Preview: " + what); }
std::string Lower(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c += 'a' - 'A'; return s; }
void Path(const std::string& s) {
    if (s.empty() || s.size() > 240 || s.front() == '/' || s.find("..") != std::string::npos ||
        s.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_./-") != std::string::npos)
        Bad("resource path must be relative to the game folder");
}
void Range(const Bytes& b, size_t p, size_t n) { if (p > b.size() || n > b.size() - p) Bad("truncated or out-of-range resource data"); }
uint16_t U16(const Bytes& b, size_t p) { Range(b, p, 2); return uint16_t(b[p] | (uint16_t(b[p+1]) << 8)); }
int16_t S16(const Bytes& b, size_t p) { return static_cast<int16_t>(U16(b, p)); }
uint32_t U32(const Bytes& b, size_t p) {
    Range(b, p, 4); return uint32_t(b[p]) | (uint32_t(b[p+1]) << 8) | (uint32_t(b[p+2]) << 16) | (uint32_t(b[p+3]) << 24);
}
int32_t I32(const Bytes& b, size_t p) { return static_cast<int32_t>(U32(b,p)); }
double F32(const Bytes& b, size_t p) {
    const uint32_t bits = U32(b,p); float f; std::memcpy(&f,&bits,4);
    if (!std::isfinite(f) || std::abs(f) > 1e9) Bad("nonfinite or excessive numeric value");
    return f;
}
std::string Label(const Bytes& b, size_t p, size_t n) {
    Range(b,p,n); size_t len = 0; while (len < n && b[p+len]) ++len;
    return std::string(reinterpret_cast<const char*>(b.data()+p),len);
}
int Count(const Bytes& b, size_t p, int max, bool nonzero = false) {
    const int value = I32(b,p); if (value < (nonzero ? 1 : 0) || value > max) Bad("invalid table count"); return value;
}
size_t Table(const Bytes& b, size_t p, int count, size_t stride) {
    const int offset = I32(b,p); if (offset < 0) Bad("negative table offset");
    Range(b,size_t(offset),size_t(count)*stride); return size_t(offset);
}
Bytes FileBytes(const fs::path& path) {
    const auto size = fs::file_size(path); if (size > kResourceLimit) Bad("resource exceeds 64 MiB");
    Bytes b(static_cast<size_t>(size)); std::ifstream in(path,std::ios::binary);
    if (!in || (size && !in.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(size)))) Bad("cannot read " + path.u8string());
    return b;
}
void Inside(const fs::path& root, const fs::path& file) {
    const auto base = fs::weakly_canonical(fs::absolute(root)), target = fs::weakly_canonical(file);
    auto a = base.begin(), c = target.begin();
    for (; a != base.end(); ++a,++c) {
        if (c == target.end()) Bad("resource escapes game folder");
#ifdef _WIN32
        if (Lower(a->u8string()) != Lower(c->u8string())) Bad("resource escapes game folder");
#else
        if (*a != *c) Bad("resource escapes game folder");
#endif
    }
}
Bytes StudioHeader(Bytes b, bool sequence = false) {
    const size_t header = sequence ? 76 : 244; Range(b,0,header);
    if (Label(b,0,4) != (sequence ? "IDSQ" : "IDST") || I32(b,4) != 10) Bad("expected GoldSrc Studio v10");
    const int length = I32(b,72); if (length < int(header) || size_t(length) > b.size()) Bad("invalid Studio length");
    b.resize(size_t(length)); return b;
}
struct V { double x=0,y=0,z=0; V operator+(V b) const {return {x+b.x,y+b.y,z+b.z};} V operator-(V b) const {return {x-b.x,y-b.y,z-b.z};} V operator*(double n) const {return {x*n,y*n,z*n};} };
V Vector(const Bytes& b, size_t p) { return {F32(b,p),F32(b,p+4),F32(b,p+8)}; }
struct Q { double x=0,y=0,z=0,w=1; };
Q Euler(V a) {
    const double sx=std::sin(a.x*.5),cx=std::cos(a.x*.5),sy=std::sin(a.y*.5),cy=std::cos(a.y*.5),sz=std::sin(a.z*.5),cz=std::cos(a.z*.5);
    return {sx*cy*cz-cx*sy*sz,cx*sy*cz+sx*cy*sz,cx*cy*sz-sx*sy*cz,cx*cy*cz+sx*sy*sz};
}
Q Mix(Q a,Q b,double t) {
    double d=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
    if (d<0) {b={-b.x,-b.y,-b.z,-b.w};d=-d;}
    double x=1-t,y=t;
    if (d<.9995) {const double angle=std::acos(std::clamp(d,-1.,1.)),s=std::sin(angle);x=std::sin((1-t)*angle)/s;y=std::sin(t*angle)/s;}
    Q q{a.x*x+b.x*y,a.y*x+b.y*y,a.z*x+b.z*y,a.w*x+b.w*y};
    const double n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    return {q.x/n,q.y/n,q.z/n,q.w/n};
}
struct Matrix {
    double m[3][4]{};
    V Apply(V v) const {return {m[0][0]*v.x+m[0][1]*v.y+m[0][2]*v.z+m[0][3],m[1][0]*v.x+m[1][1]*v.y+m[1][2]*v.z+m[1][3],m[2][0]*v.x+m[2][1]*v.y+m[2][2]*v.z+m[2][3]};}
};
Matrix Transform(Q q,V p) {
    Matrix m;
    m.m[0][0]=1-2*(q.y*q.y+q.z*q.z);m.m[0][1]=2*(q.x*q.y-q.z*q.w);m.m[0][2]=2*(q.x*q.z+q.y*q.w);m.m[0][3]=p.x;
    m.m[1][0]=2*(q.x*q.y+q.z*q.w);m.m[1][1]=1-2*(q.x*q.x+q.z*q.z);m.m[1][2]=2*(q.y*q.z-q.x*q.w);m.m[1][3]=p.y;
    m.m[2][0]=2*(q.x*q.z-q.y*q.w);m.m[2][1]=2*(q.y*q.z+q.x*q.w);m.m[2][2]=1-2*(q.x*q.x+q.y*q.y);m.m[2][3]=p.z;return m;
}
Matrix Concat(const Matrix& a,const Matrix& b) {
    Matrix c;
    for (int i=0;i<3;++i) {for (int j=0;j<3;++j) for (int k=0;k<3;++k)c.m[i][j]+=a.m[i][k]*b.m[k][j];c.m[i][3]=a.m[i][3];for(int k=0;k<3;++k)c.m[i][3]+=a.m[i][k]*b.m[k][3];}
    return c;
}
struct Bone { int parent=-1; double value[6]{},scale[6]{}; };
struct Texture { int width=0,height=0;uint32_t flags=0;Bytes indices;std::vector<uint32_t> colors; };
struct Vertex { V p; std::array<int,4> bones{{0,-1,-1,-1}}; std::array<double,4> weights{{1,0,0,0}}; };
struct Corner { int vertex=0;double u=0,v=0;bool normalized=false; };
struct Triangle { std::array<Corner,3> c;int skinref=0; };
struct MeshModel { std::vector<Vertex> vertices;std::vector<Triangle> triangles; };
struct Part { int base=1;std::vector<MeshModel> models; };
struct Seq { Sequence info; int flags=0,group=0,blends=1,motion=0;size_t animation=0; };
double Half(uint16_t bits) {
    const int exp=(bits>>10)&31; const double fraction=(bits&1023)/1024.;
    if (exp==31) Bad("nonfinite half-float texture coordinate");
    return (bits&32768 ? -1:1)*(exp ? std::ldexp(1+fraction,exp-15):std::ldexp(fraction,-14));
}
double Anim(const Bytes& b,size_t record,int channel,int frame) {
    const uint16_t offset=U16(b,record+size_t(channel)*2); if (!offset) return 0;
    size_t p=record+offset;
    for (int run=0;run<65536;++run) {
        Range(b,p,2);const int valid=b[p],total=b[p+1];
        if (!total || !valid || valid>total) Bad("invalid animation run");
        Range(b,p,size_t(valid+1)*2);
        if (frame<total) return S16(b,p+size_t(std::min(frame+1,valid))*2);
        frame-=total;p+=size_t(valid+1)*2;
    }
    Bad("animation run limit");
}
Image Canvas(int w,int h) {
    if (w<1 || h<1 || w>2048 || h>2048) Bad("preview dimensions must be 1..2048");
    Image image;image.width=w;image.height=h;image.pixels.resize(size_t(w)*size_t(h));
    for(int y=0;y<h;++y) for(int x=0;x<w;++x)image.pixels[size_t(y)*w+x]=((x/16+y/16)&1)?0xffd5dce5:0xffe8edf3;
    return image;
}
uint32_t Blend(uint32_t target,uint32_t source,bool additive=false) {
    const unsigned a=source>>24;uint32_t result=0xff000000;
    for(int shift:{0,8,16}) {const unsigned s=(source>>shift)&255,t=(target>>shift)&255;
        result |= (additive ? std::min(255u,t+s*a/255) : (s*a+t*(255-a))/255)<<shift;}
    return result;
}
} // namespace

Bytes ReadResource(const fs::path& root,const std::string& relative) {
    Path(relative);if(root.empty())Bad("open a game folder first");
    const auto file=root/fs::u8path(relative);Inside(root,file);
    if(fs::is_regular_file(file)) return FileBytes(file);
    std::vector<std::pair<int,fs::path>> packs;
    for(const auto& item:fs::directory_iterator(root)) {
        const auto name=Lower(item.path().filename().u8string());
        if(name.size()<8 || name.substr(0,3)!="pak" || name.substr(name.size()-4)!=".pak")continue;
        const auto digits=name.substr(3,name.size()-7);if(digits.empty() || digits.size()>6 || digits.find_first_not_of("0123456789")!=std::string::npos)continue;
        packs.emplace_back(std::stoi(digits),item.path());
    }
    std::sort(packs.begin(),packs.end(),[](const auto& a,const auto& b){return a.first>b.first;});
    for(const auto& pack:packs) {
        Inside(root,pack.second);std::ifstream in(pack.second,std::ios::binary);Bytes header(12);
        if(!in.read(reinterpret_cast<char*>(header.data()),12) || Label(header,0,4)!="PACK")Bad("invalid PAK header");
        const uint32_t offset=U32(header,4),length=U32(header,8);const auto size=fs::file_size(pack.second);
        if(length%64 || length>64*65536 || uint64_t(offset)+length>size)Bad("invalid PAK directory");
        Bytes directory(length);in.seekg(offset);if(length && !in.read(reinterpret_cast<char*>(directory.data()),length))Bad("truncated PAK directory");
        for(size_t p=0;p<directory.size();p+=64)if(Lower(Label(directory,p,56))==Lower(relative)) {
            const uint32_t start=U32(directory,p+56),bytes=U32(directory,p+60);
            if(bytes>kResourceLimit || uint64_t(start)+bytes>size)Bad("invalid PAK resource span");
            Bytes b(bytes);in.clear();in.seekg(start);if(bytes && !in.read(reinterpret_cast<char*>(b.data()),bytes))Bad("truncated PAK resource");return b;
        }
    }
    Bad("resource not found: "+relative);
}
struct StudioData {
    ModelInfo info;Bytes header;std::vector<Bytes> groups;std::vector<Bone> bones;
    std::vector<Seq> sequences;std::vector<Texture> textures;std::vector<int> skins;
    std::vector<Part> parts;int skinrefs=0,skinfamilies=0,bodies=1;V low,high;
};
void StudioPreview::Load(const fs::path& root,const std::string& relative) {
    auto d=std::make_shared<StudioData>();d->header=StudioHeader(ReadResource(root,relative));const auto& b=d->header;
    d->info.name=Label(b,8,64);d->low=Vector(b,112);d->high=Vector(b,124);
    for(int i=0;i<3;++i) {d->info.mins[i]=float(F32(b,88+size_t(i)*4));d->info.maxs[i]=float(F32(b,100+size_t(i)*4));}
    const int boneCount=Count(b,140,128,true);const size_t bones=Table(b,144,boneCount,112);
    const int controllerCount = Count(b,148,32); const size_t controllerTable = Table(b,152,controllerCount,24);
    std::vector<double> adjustments;
    for (int i=0; i<controllerCount; ++i) {
        const size_t p = controllerTable + size_t(i)*24; const int type = I32(b,p+4), rest = Count(b,p+16,255);
        const double start = F32(b,p+8), end = F32(b,p+12);
        double value = type & 0x8000 ? start + rest * (360./256.) : start + (end-start) * (rest/255.);
        if (type & (8|16|32)) value *= 3.14159265358979323846/180.;
        adjustments.push_back(value);
    }
    for(int i=0;i<boneCount;++i) {
        const size_t p=bones+size_t(i)*112;Bone bone;bone.parent=I32(b,p+32);
        if(bone.parent < -1 || bone.parent>=i)Bad("bone parent must precede child");
        for(int j=0;j<6;++j) {
            bone.value[j]=F32(b,p+64+size_t(j)*4);bone.scale[j]=F32(b,p+88+size_t(j)*4);
            const int controller = I32(b,p+40+size_t(j)*4);
            if (controller < -1 || controller >= controllerCount) Bad("invalid bone controller");
            if (controller >= 0) bone.value[j] += adjustments[size_t(controller)];
        }
        d->bones.push_back(bone);
    }
    const int groupCount=Count(b,172,32,true);const size_t groupTable=Table(b,176,groupCount,104);
    d->groups.push_back(b);
    // Sequence groups are loaded lazily only if a sequence refers to them.
    d->groups.resize(size_t(groupCount));
    const int seqCount=Count(b,164,2048,true);const size_t sequences=Table(b,168,seqCount,176);
    size_t animationWork = 0, groupBytes = b.size();
    for(int i=0;i<seqCount;++i) {
        const size_t p=sequences+size_t(i)*176;Seq seq;
        seq.info={Label(b,p,32),float(F32(b,p+32)),Count(b,p+56,65536,true)};
        if(seq.info.fps<=0 || seq.info.fps>1000)Bad("invalid sequence FPS");
        seq.flags=I32(b,p+36);seq.motion=I32(b,p+68);seq.blends=Count(b,p+120,64,true);seq.group=I32(b,p+156);
        if(seq.group<0 || seq.group>=groupCount || I32(b,p+124)<0)Bad("invalid sequence group/animation");
        if(d->groups[size_t(seq.group)].empty()) {
            const auto path=Label(b,groupTable+size_t(seq.group)*104+32,64);
            d->groups[size_t(seq.group)]=StudioHeader(ReadResource(root,path),true);
            groupBytes += d->groups[size_t(seq.group)].size();
            if (groupBytes > 2 * kResourceLimit) Bad("sequence group allocation limit");
        }
        animationWork += size_t(boneCount) * size_t(seq.info.frames) * size_t(seq.blends);
        if (animationWork > kResourceLimit) Bad("animation validation work limit");
        seq.animation=size_t(I32(b,p+124));
        if(seq.group==0) {
            const int base=I32(b,groupTable+100);if(base<0)Bad("negative animation base");seq.animation+=size_t(base);
        }
        Range(d->groups[size_t(seq.group)],seq.animation,size_t(boneCount)*12*size_t(seq.blends));
        // Validate each RLE channel at both ends before any playback begins.
        for(int blend=0;blend<seq.blends;++blend)for(int bone=0;bone<boneCount;++bone)for(int channel=0;channel<6;++channel) {
            const size_t record=seq.animation+size_t(blend*boneCount+bone)*12;
            Anim(d->groups[size_t(seq.group)],record,channel,seq.info.frames-1);
        }
        const V lo=Vector(b,p+96),hi=Vector(b,p+108);
        d->low={std::min(d->low.x,lo.x),std::min(d->low.y,lo.y),std::min(d->low.z,lo.z)};
        d->high={std::max(d->high.x,hi.x),std::max(d->high.y,hi.y),std::max(d->high.z,hi.z)};
        d->sequences.push_back(seq);d->info.sequences.push_back(seq.info);
    }
    Bytes textureData=b;
    if(!Count(b,180,256)) {
        auto companion=fs::u8path(relative);companion.replace_filename(companion.stem().u8string()+"T.mdl");
        textureData=StudioHeader(ReadResource(root,companion.generic_u8string()));
    }
    const int textureCount=Count(textureData,180,256,true);const size_t textures=Table(textureData,184,textureCount,80);
    size_t totalTexels=0;
    for(int i=0;i<textureCount;++i) {
        const size_t p=textures+size_t(i)*80;Texture tex;tex.flags=U32(textureData,p+64);
        tex.width=Count(textureData,p+68,4096,true);tex.height=Count(textureData,p+72,4096,true);
        const size_t count=size_t(tex.width)*tex.height;totalTexels+=count;if(totalTexels>kResourceLimit)Bad("texture allocation limit");
        const size_t offset=Table(textureData,p+76,1,count+768);tex.indices.assign(textureData.begin()+offset,textureData.begin()+offset+count);
        for(int j=0;j<256;++j) {const size_t c=offset+count+size_t(j)*3;tex.colors.push_back(0xff000000 | (uint32_t(textureData[c])<<16) | (uint32_t(textureData[c+1])<<8) | textureData[c+2]);}
        d->textures.push_back(std::move(tex));
    }
    d->skinrefs=Count(textureData,192,256,true);d->skinfamilies=Count(textureData,196,256,true);
    const size_t skinTable=Table(textureData,200,d->skinrefs*d->skinfamilies,2);
    for(int i=0;i<d->skinrefs*d->skinfamilies;++i) {const int skin=S16(textureData,skinTable+size_t(i)*2);if(skin<0 || skin>=textureCount)Bad("invalid skin texture");d->skins.push_back(skin);}
    const int partCount=Count(b,204,32,true);const size_t partTable=Table(b,208,partCount,76);size_t totalVertices=0,totalTriangles=0;
    for(int part=0;part<partCount;++part) {
        const size_t p=partTable+size_t(part)*76;Part bp;const int count=Count(b,p+64,256,true);
        bp.base=Count(b,p+68,65536,true);d->bodies=std::max(d->bodies,bp.base*count);
        if (d->bodies > 4096) Bad("body selection limit");
        const size_t models=Table(b,p+72,count,112);
        for(int m=0;m<count;++m) {
            const size_t mp=models+size_t(m)*112;MeshModel mesh;const int n=Count(b,mp+80,16384);totalVertices+=size_t(n);
            if(totalVertices>262144)Bad("model vertex allocation limit");
            const size_t verts=Table(b,mp+88,n,12),boneIds=Table(b,mp+84,n,1);const int weights=I32(b,mp+104);
            if(weights<0)Bad("negative bone-weight offset");
            if(weights)Range(b,size_t(weights),size_t(n)*8);
            for(int v=0;v<n;++v) {
                Vertex vertex;vertex.p=Vector(b,verts+size_t(v)*12);
                if(weights) {
                    double sum=0;for(int k=0;k<4;++k) {const size_t wp=size_t(weights)+size_t(v)*8;
                        const int index=static_cast<int8_t>(b[wp+4+size_t(k)]);const double weight=b[wp+size_t(k)]/255.;
                        if(index < -1 || index>=boneCount || (index<0 && weight>0))Bad("invalid weighted bone");
                        vertex.bones[size_t(k)]=index;vertex.weights[size_t(k)]=weight;sum+=weight;}
                    if(sum<=0)Bad("empty vertex weights");
                    for(auto& weight:vertex.weights)weight/=sum;
                } else {vertex.bones[0]=b[boneIds+size_t(v)];if(vertex.bones[0]>=boneCount)Bad("invalid vertex bone");}
                mesh.vertices.push_back(vertex);
            }
            const int meshCount=Count(b,mp+72,4096),normalCount=Count(b,mp+92,65536);const size_t meshes=Table(b,mp+76,meshCount,20);
            Table(b,mp+100,normalCount,12);Table(b,mp+96,normalCount,1);
            for(int mi=0;mi<meshCount;++mi) {
                const size_t me=meshes+size_t(mi)*20;const int skinref=I32(b,me+8);if(skinref<0 || skinref>=d->skinrefs)Bad("invalid mesh skin reference");
                const auto& tex=d->textures[size_t(d->skins[size_t(skinref)])];size_t at=Table(b,me+4,1,2);int declared=Count(b,me,65536),triangles=0;
                for(int command=0;command<65536;++command) {
                    const int signedCount=S16(b,at);at+=2;if(!signedCount)break;
                    const int vertices=std::abs(signedCount);if(vertices<3)Bad("invalid triangle command");Range(b,at,size_t(vertices)*8);
                    std::vector<Corner> corners;
                    for(int vi=0;vi<vertices;++vi,at+=8) {
                        const int vertex=S16(b,at),normal=S16(b,at+2);
                        if(vertex<0 || vertex>=n || normal<0 || normal>=normalCount)Bad("invalid triangle vertex/normal");
                        const bool normalized = (tex.flags&0x80000000u)!=0;
                        const double u=normalized ? Half(U16(b,at+4)) : S16(b,at+4);
                        const double v=normalized ? Half(U16(b,at+6)) : S16(b,at+6);
                        corners.push_back({vertex,u,v,normalized});
                    }
                    for(int vi=2;vi<vertices;++vi) {
                        Triangle tri;tri.skinref=skinref;
                        tri.c=signedCount<0 ? std::array<Corner,3>{{corners[0],corners[size_t(vi-1)],corners[size_t(vi)]}} :
                            std::array<Corner,3>{{corners[size_t(vi-2)],corners[size_t(vi-1)],corners[size_t(vi)]}};
                        mesh.triangles.push_back(tri);++triangles;if(++totalTriangles>524288)Bad("triangle allocation limit");
                    }
                    if(command==65535)Bad("unterminated triangle commands");
                }
                if(triangles!=declared)Bad("triangle count mismatch");
            }
            bp.models.push_back(std::move(mesh));
        }
        d->parts.push_back(std::move(bp));
    }
    if(!totalTriangles)Bad("model contains no triangles");
    // Authored bounds can be empty; include bind-pose geometry for framing.
    std::vector<Matrix> bind;
    for(const auto& bone:d->bones) {auto t=Transform(Euler({bone.value[3],bone.value[4],bone.value[5]}),{bone.value[0],bone.value[1],bone.value[2]});if(bone.parent>=0)t=Concat(bind[size_t(bone.parent)],t);bind.push_back(t);}
    for(const auto& part:d->parts)for(const auto& mesh:part.models)for(const auto& v:mesh.vertices) {
        V p;for(int k=0;k<4;++k)if(v.bones[size_t(k)]>=0)p=p+bind[size_t(v.bones[size_t(k)])].Apply(v.p)*v.weights[size_t(k)];
        d->low={std::min(d->low.x,p.x),std::min(d->low.y,p.y),std::min(d->low.z,p.z)};d->high={std::max(d->high.x,p.x),std::max(d->high.y,p.y),std::max(d->high.z,p.z)};
    }
    data_=std::move(d); // Failed reload preserves the previous valid preview.
}
const ModelInfo& StudioPreview::Info() const {if(!data_)Bad("load a model first");return data_->info;}
int StudioPreview::SkinCount() const {return data_?data_->skinfamilies:0;}
int StudioPreview::BodyCount() const {return data_?data_->bodies:0;}
Image StudioPreview::Render(int width,int height,int sequence,double seconds,double yaw,double tilt,int skin,int body) const {
    PreviewCamera camera; camera.yaw = yaw; camera.tilt = tilt;
    return Render(width,height,sequence,seconds,camera,skin,body);
}
Image StudioPreview::Render(int width,int height,int sequence,double seconds,const PreviewCamera& camera,int skin,int body) const {
    Image image=Canvas(width,height);if(!data_)return image;const auto& d=*data_;
    if(sequence<0 || size_t(sequence)>=d.sequences.size() || skin<0 || skin>=d.skinfamilies || body<0 || body>=d.bodies ||
        !std::isfinite(seconds) || std::abs(seconds) > 1e9 || !std::isfinite(camera.yaw) || !std::isfinite(camera.tilt))Bad("invalid preview selection/time");
    camera.Validate();
    const auto& seq=d.sequences[size_t(sequence)];const double last=std::max(0,seq.info.frames-1);
    double frame=std::max(0.,seconds)*seq.info.fps;
    frame=(seq.flags&1) && last>0 ? std::fmod(frame,last) : std::min(frame,last);
    const int first=int(frame),next=std::min(first+1,seq.info.frames-1);const double t=frame-first;
    std::vector<Matrix> matrices;
    for(size_t i=0;i<d.bones.size();++i) {
        const auto& bone=d.bones[i];double a[6]{},b[6]{};const size_t record=seq.animation+i*12;
        for(int j=0;j<6;++j) {a[j]=bone.value[j]+Anim(d.groups[size_t(seq.group)],record,j,first)*bone.scale[j];b[j]=bone.value[j]+Anim(d.groups[size_t(seq.group)],record,j,next)*bone.scale[j];}
        V p{a[0]*(1-t)+b[0]*t,a[1]*(1-t)+b[1]*t,a[2]*(1-t)+b[2]*t};
        if(i==0) {if(seq.motion&1)p.x=0;if(seq.motion&2)p.y=0;if(seq.motion&4)p.z=0;}
        Matrix m=Transform(Mix(Euler({a[3],a[4],a[5]}),Euler({b[3],b[4],b[5]}),t),p);
        if(bone.parent>=0)m=Concat(matrices[size_t(bone.parent)],m);
        matrices.push_back(m);
    }
    const V center=(d.low+d.high)*.5,extent=d.high-d.low;
    const double radius=std::max({1.,extent.x,extent.y,extent.z});
    const double scale=std::min(width,height)*.75/radius*camera.zoom*camera.previewScale;
    const double yaw=camera.yaw,tilt=camera.tilt;
    const double cy=std::cos(yaw),sy=std::sin(yaw),ct=std::cos(tilt),st=std::sin(tilt);
    const double distance=radius*(camera.preset==CameraPreset::FirstPerson?1.35:3.5);
    const double fovScale=std::tan(70.*3.141592653589793/360.)/std::tan(camera.fovDegrees*3.141592653589793/360.);
    std::vector<double> depth(image.pixels.size(),-std::numeric_limits<double>::infinity());
    size_t rasterWork = 0;
    for(const auto& part:d.parts) {
        const auto& mesh=part.models[size_t((body/part.base)%int(part.models.size()))];std::vector<V> points;
        for(const auto& v:mesh.vertices) {
            V p;for(int k=0;k<4;++k)if(v.bones[size_t(k)]>=0)p=p+matrices[size_t(v.bones[size_t(k)])].Apply(v.p)*v.weights[size_t(k)];p=p-center;
            const double x=p.x*cy-p.y*sy,z=p.x*sy+p.y*cy;
            const double depth=z*ct+p.z*st;
            // Bounded perspective avoids singularities when a large v_model
            // reaches past the editor camera. Orthographic presets stay linear.
            const double perspective=camera.Perspective()?std::clamp(distance/std::max(radius*.25,distance-depth),.15,5.)*fovScale:1.;
            points.push_back({width*(.5+camera.panX)+x*scale*perspective,
                height*(.5+camera.panY)-(p.z*ct-z*st)*scale*perspective,depth});
        }
        for(const auto& tri:mesh.triangles) {
            const auto& tex=d.textures[size_t(d.skins[size_t(skin*d.skinrefs+tri.skinref)])];
            const V a=points[size_t(tri.c[0].vertex)],b=points[size_t(tri.c[1].vertex)],c=points[size_t(tri.c[2].vertex)];
            auto edge=[](V p,V q,double x,double y){return (x-p.x)*(q.y-p.y)-(y-p.y)*(q.x-p.x);};
            const double area=edge(a,b,c.x,c.y);if(std::abs(area)<1e-8)continue;
            const int xmin=int(std::clamp(std::floor(std::min({a.x,b.x,c.x})),0.,double(width-1))),xmax=int(std::clamp(std::ceil(std::max({a.x,b.x,c.x})),0.,double(width-1)));
            const int ymin=int(std::clamp(std::floor(std::min({a.y,b.y,c.y})),0.,double(height-1))),ymax=int(std::clamp(std::ceil(std::max({a.y,b.y,c.y})),0.,double(height-1)));
            rasterWork += size_t(xmax-xmin+1) * size_t(ymax-ymin+1);
            if (rasterWork > kResourceLimit) Bad("model exceeds software preview work limit");
            for(int y=ymin;y<=ymax;++y)for(int x=xmin;x<=xmax;++x) {
                const double wa=edge(b,c,x+.5,y+.5)/area,wb=edge(c,a,x+.5,y+.5)/area,wc=1-wa-wb;if(wa<0 || wb<0 || wc<0)continue;
                const double z=wa*a.z+wb*b.z+wc*c.z;const size_t pixel=size_t(y)*width+x;if(z<depth[pixel])continue;
                double u=wa*tri.c[0].u+wb*tri.c[1].u+wc*tri.c[2].u,v=wa*tri.c[0].v+wb*tri.c[1].v+wc*tri.c[2].v;
                if (tri.c[0].normalized) { u*=tex.width; v*=tex.height; }
                const int tx=int(std::fmod(std::fmod(std::floor(u),tex.width)+tex.width,tex.width));
                const int ty=int(std::fmod(std::fmod(std::floor(v),tex.height)+tex.height,tex.height));
                const uint8_t index=tex.indices[size_t(ty)*tex.width+tx];if((tex.flags&64) && index==255)continue;
                image.pixels[pixel]=Blend(image.pixels[pixel],tex.colors[index],(tex.flags&32)!=0);if(!(tex.flags&32))depth[pixel]=z;
            }
        }
    }
    return image;
}

namespace {
SpriteFrame SpriteSingle(const Bytes& b,size_t& at,const std::vector<uint32_t>& palette,int format,double end,size_t& pixels) {
    Range(b,at,16);SpriteFrame frame;frame.originX=I32(b,at);frame.originY=I32(b,at+4);
    if (frame.originX < -65536 || frame.originX > 65536 || frame.originY < -65536 || frame.originY > 65536) Bad("excessive sprite origin");
    frame.image.width=Count(b,at+8,4096,true);frame.image.height=Count(b,at+12,4096,true);at+=16;
    const size_t n=size_t(frame.image.width)*frame.image.height;pixels+=n;if(pixels>kResourceLimit/4)Bad("sprite allocation limit");Range(b,at,n);
    frame.image.pixels.reserve(n);
    for(size_t i=0;i<n;++i) {
        const unsigned index=b[at+i];if(index>=palette.size())Bad("sprite palette index out of range");uint32_t color=palette[index];
        if(format==3 && index==255)color&=0x00ffffff;
        if(format==2)color=(palette.back()&0x00ffffff)|(index<<24);
        frame.image.pixels.push_back(color);
    }
    at+=n;frame.endSeconds=end;return frame;
}
}
void SpritePreview::Load(const fs::path& root,const std::string& relative,int hudResolution) {
    Path(relative);std::string path=relative;int cropX=0,cropY=0,cropW=0,cropH=0;
    if(Lower(fs::u8path(path).extension().u8string())==".txt") {
        const auto bytes=ReadResource(root,path);if(bytes.size()>65536)Bad("HUD list too large");
        std::istringstream input(std::string(bytes.begin(),bytes.end()));int count=0;if(!(input>>count) || count<1 || count>1024)Bad("invalid HUD sprite list");
        int best=-1;for(int i=0;i<count;++i) {
            std::string role,stem;int resolution,x,y,w,h;if(!(input>>role>>resolution>>stem>>x>>y>>w>>h))Bad("truncated HUD sprite list");
            const int score=(resolution==hudResolution?4:resolution==640?2:0)+(role=="weapon" || role=="ammo"?1:0);
            if(score>best) {Path(stem);path="sprites/"+stem+".spr";cropX=x;cropY=y;cropW=w;cropH=h;best=score;}
        }
        if(cropX<0 || cropY<0 || cropW<1 || cropH<1)Bad("invalid HUD crop");
    }
    const Bytes b=ReadResource(root,path);Range(b,0,42);
    if(Label(b,0,4)!="IDSP" || I32(b,4)!=2)Bad("expected GoldSrc IDSP version 2 sprite");
    const int format=I32(b,12);if(format<0 || format>3)Bad("unsupported sprite texture format");
    const int count=Count(b,28,4096,true);Count(b,20,4096,true);Count(b,24,4096,true);
    const int colors=U16(b,40);if(colors<1 || colors>256)Bad("invalid sprite palette");Range(b,42,size_t(colors)*3);
    std::vector<uint32_t> palette;for(int i=0;i<colors;++i) {const size_t p=42+size_t(i)*3;palette.push_back(0xff000000|(uint32_t(b[p])<<16)|(uint32_t(b[p+1])<<8)|b[p+2]);}
    size_t at=42+size_t(colors)*3,pixels=0;double end=0;std::vector<SpriteFrame> frames;
    for(int i=0;i<count;++i) {
        const int type=I32(b,at);at+=4;
        if(type==0) {end+=.1;frames.push_back(SpriteSingle(b,at,palette,format,end,pixels));}
        else if(type==1 || type==2) {
            const int group=Count(b,at,4096,true);at+=4;std::vector<double> intervals;double previous=0;
            for(int j=0;j<group;++j) {const double v=F32(b,at);at+=4;if(v<=previous)Bad("sprite group intervals must increase");intervals.push_back(v);previous=v;}
            const double base=end;for(double interval:intervals)frames.push_back(SpriteSingle(b,at,palette,format,base+interval,pixels));end+=intervals.back();
        } else Bad("invalid sprite frame type");
        if(frames.size()>4096)Bad("sprite frame limit");
    }
    if(cropW)for(auto& frame:frames) {
        const auto& source=frame.image;
        if(cropX>source.width-cropW || cropY>source.height-cropH)Bad("HUD crop exceeds sprite");
        Image crop;crop.width=cropW;crop.height=cropH;
        for(int y=0;y<cropH;++y)crop.pixels.insert(crop.pixels.end(),source.pixels.begin()+size_t(y+cropY)*source.width+cropX,source.pixels.begin()+size_t(y+cropY)*source.width+cropX+cropW);
        frame.image=std::move(crop);
        frame.originX += cropX; frame.originY -= cropY;
    }
    frames_=std::move(frames); format_ = format;
}
const SpriteFrame& SpritePreview::Frame(double seconds) const {
    if(frames_.empty() || !std::isfinite(seconds))Bad("load a sprite / use a finite playback time");
    const double t=std::fmod(std::max(0.,seconds),frames_.back().endSeconds);
    for(const auto& frame:frames_)if(t<frame.endSeconds)return frame;
    return frames_.back();
}
Image SpritePreview::Render(int width,int height,double seconds) const {
    Image image=Canvas(width,height);const auto& source=Frame(seconds).image;
    int xmin=std::numeric_limits<int>::max(),ymin=xmin,xmax=std::numeric_limits<int>::min(),ymax=xmax;
    for(const auto& f:frames_) {
        // Origins are bounded during parsing; dimensions are at most 4096.
        xmin=std::min(xmin,f.originX);ymin=std::min(ymin,-f.originY);
        xmax=std::max(xmax,f.originX+f.image.width);ymax=std::max(ymax,-f.originY+f.image.height);
    }
    const double scale=std::min(double(width)*.85/(xmax-xmin),double(height)*.85/(ymax-ymin));
    const auto& frame=Frame(seconds);
    const int w=std::max(1,int(source.width*scale)),h=std::max(1,int(source.height*scale));
    const int left=int((width-(xmax-xmin)*scale)*.5+(frame.originX-xmin)*scale),top=int((height-(ymax-ymin)*scale)*.5+(-frame.originY-ymin)*scale);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
        const int sx=std::min(source.width-1,int(x/scale)),sy=std::min(source.height-1,int(y/scale));
        if(x+left<0 || x+left>=width || y+top<0 || y+top>=height)continue;
        const size_t dest=size_t(y+top)*width+x+left;const uint32_t pixel=source.pixels[size_t(sy)*source.width+sx];image.pixels[dest]=Blend(image.pixels[dest],pixel,format_==1);
    }
    return image;
}
std::vector<AmmoCurve> ComparisonCurves(const modular::Registry& registry,Kind kind,const std::string& id) {
    std::vector<std::pair<const modular::Cartridge*,double>> input;
    if(!registry.Valid())Bad("validate definitions before plotting");
    if(kind==Kind::Cartridge) {const auto* c=registry.FindCartridge(id);if(c)input.emplace_back(c,c->referenceVelocityMps);}
    else if(const auto* w=registry.FindWeapon(id))for(const auto& a:w->compatible)input.emplace_back(registry.FindCartridge(a.cartridgeId),a.muzzleVelocityMps);
    std::vector<AmmoCurve> result;
    for(const auto& item:input) {
        if(!item.first)Bad("unknown curve cartridge");
        const auto& c=*item.first;AmmoCurve curve;curve.id=c.id;curve.velocity=item.second;curve.muzzleDamage=modular::MuzzleDamage(c,item.second);
        const double initial=modular::KineticEnergyJ(c.massGrams,item.second);
        for(const auto& state:Flight(c,item.second)) {
            const double energy=modular::BallisticEnergy(state,c.massGrams*.001);
            curve.samples.push_back({state.position.x,state.position.z,energy,modular::ImpactDamage(energy,initial,c.diameterMm,c.schemaVersion>=3?c.damageCoefficient:0,c.damage)});
        }
        result.push_back(std::move(curve));
    }
    return result;
}
} // namespace pxwe
