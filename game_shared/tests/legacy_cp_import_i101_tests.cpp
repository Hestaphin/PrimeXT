// Run under ASan/UBSan as well as an ordinary portable C++ build.
// This exercises the same validator that the pxmv _cp import path calls.
#include "../../utils/pxmv/LegacyCpStudioValidator.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

namespace
{
template<typename T> size_t Append(std::vector<unsigned char> &data, size_t count = 1)
{
    const size_t mask = alignof(T) - 1;
    const size_t offset = (data.size() + mask) & ~mask;
    data.resize(offset + sizeof(T) * count);
    return offset;
}
template<typename T> T &At(std::vector<unsigned char> &d, size_t at)
{
    return *reinterpret_cast<T *>(d.data() + at);
}
struct Model
{
    std::vector<unsigned char> bytes;
    size_t bones, parts, models, vertices, vbones, meshes, command;
    explicit Model(bool animation = false)
    {
        bytes.resize(sizeof(studiohdr_t));
        bones = Append<mstudiobone_t>(bytes);
        parts = Append<mstudiobodyparts_t>(bytes);
        models = Append<mstudiomodel_t>(bytes);
        vertices = Append<vec3_t>(bytes, 3);
        vbones = Append<unsigned char>(bytes, 3);
        meshes = Append<mstudiomesh_t>(bytes);
        command = Append<std::int16_t>(bytes, 14); // strip(3), 3x4 words, terminator
        const size_t seq = animation ? Append<mstudioseqdesc_t>(bytes) : 0;
        const size_t group = animation ? Append<mstudioseqgroup_t>(bytes) : 0;
        const size_t anim = animation ? Append<mstudioanim_t>(bytes) : 0;
        const size_t values = animation ? Append<mstudioanimvalue_t>(bytes, 2) : 0;
        auto &h = At<studiohdr_t>(bytes, 0);
        h.ident = IDSTUDIOHEADER; h.version = STUDIO_VERSION;
        h.length = static_cast<int>(bytes.size());
        h.numbones = 1; h.boneindex = static_cast<int>(bones);
        h.numbodyparts = 1; h.bodypartindex = static_cast<int>(parts);
        h.numseq = animation ? 1 : 0;
        h.seqindex = static_cast<int>(seq); h.numseqgroups = animation ? 1 : 0;
        h.seqgroupindex = static_cast<int>(group);
        auto &bone = At<mstudiobone_t>(bytes, bones);
        std::memcpy(bone.name, "origin", 7); bone.parent = -1;
        auto &part = At<mstudiobodyparts_t>(bytes, parts);
        std::memcpy(part.name, "body", 5);
        part.nummodels = 1; part.modelindex = static_cast<int>(models);
        auto &model = At<mstudiomodel_t>(bytes, models);
        model.numverts = 3; model.nummesh = 1;
        model.vertindex = static_cast<int>(vertices);
        model.vertinfoindex = static_cast<int>(vbones);
        model.meshindex = static_cast<int>(meshes);
        At<mstudiomesh_t>(bytes, meshes).triindex = static_cast<int>(command);
        auto *words = reinterpret_cast<std::int16_t *>(bytes.data() + command);
        words[0] = 3; words[1] = 0; words[5] = 1; words[9] = 2; words[13] = 0;
        if(animation)
        {
            At<mstudioseqdesc_t>(bytes, seq).animindex = static_cast<int>(anim);
            At<mstudioseqgroup_t>(bytes, group).data = 0;
            At<mstudioanim_t>(bytes, anim).offset[0] = static_cast<unsigned short>(values - anim);
            auto *v = reinterpret_cast<mstudioanimvalue_t *>(bytes.data() + values);
            v[0].num.valid = 1; v[0].num.total = 1; v[1].value = 10;
        }
    }
};
void Good(Model &m)
{
    std::string error;
    if(!legacycp::Validate(m.bytes.data(), m.bytes.size(), error))
    {
        std::cerr << "valid test model rejected: " << error << '\n';
        std::abort();
    }
}
void Bad(const Model &m)
{
    std::string error;
    if(legacycp::Validate(m.bytes.data(), m.bytes.size(), error))
    {
        std::cerr << "malformed model accepted\n";
        std::abort();
    }
    assert(!error.empty());
}
}
int main()
{
    { Model m; Good(m); }
    { Model m(true); Good(m); }
    // Original heap-buffer-overflow reproducer: one bone table immediately
    // outside an otherwise legitimate Studio header-only file.
    {
        Model m;
        m.bytes.resize(sizeof(studiohdr_t));
        At<studiohdr_t>(m.bytes, 0).boneindex = sizeof(studiohdr_t);
        At<studiohdr_t>(m.bytes, 0).length = sizeof(studiohdr_t);
        Bad(m);
    }
    { Model m; At<studiohdr_t>(m.bytes, 0).boneindex = std::numeric_limits<int>::max(); Bad(m); }
    { Model m; At<studiohdr_t>(m.bytes, 0).boneindex = -1; Bad(m); }
    { Model m; At<studiohdr_t>(m.bytes, 0).length = sizeof(studiohdr_t); Bad(m); }
    { Model m; At<mstudiobone_t>(m.bytes, m.bones).parent = 1; Bad(m); }
    { Model m; std::memset(At<mstudiobone_t>(m.bytes, m.bones).name, 'X', sizeof(mstudiobone_t::name)); Bad(m); }
    { Model m; At<studiohdr_t>(m.bytes, 0).bodypartindex = std::numeric_limits<int>::max(); Bad(m); }
    { Model m; At<mstudiobodyparts_t>(m.bytes, m.parts).modelindex = std::numeric_limits<int>::max(); Bad(m); }
    { Model m; At<mstudiobodyparts_t>(m.bytes, m.parts).nummodels = -1; Bad(m); }
    { Model m; At<mstudiomodel_t>(m.bytes, m.models).vertindex = std::numeric_limits<int>::max(); Bad(m); }
    { Model m; At<mstudiomodel_t>(m.bytes, m.models).vertinfoindex = -3; Bad(m); }
    { Model m; At<mstudiomodel_t>(m.bytes, m.models).numverts = std::numeric_limits<int>::max(); Bad(m); }
    { Model m; At<mstudiomodel_t>(m.bytes, m.models).meshindex = std::numeric_limits<int>::max(); Bad(m); }
    { Model m; At<mstudiomesh_t>(m.bytes, m.meshes).triindex = std::numeric_limits<int>::max(); Bad(m); }
    { Model m; At<mstudiomesh_t>(m.bytes, m.meshes).triindex = static_cast<int>(m.command + 1); Bad(m); }
    { Model m; At<std::int16_t>(m.bytes, m.command) = std::numeric_limits<std::int16_t>::min(); Bad(m); }
    { Model m; At<std::int16_t>(m.bytes, m.command + 2) = 30000; Bad(m); }
    { Model m; At<std::int16_t>(m.bytes, m.command + 26) = 1; Bad(m); } // no terminator
    { Model m(true); At<studiohdr_t>(m.bytes, 0).seqindex = std::numeric_limits<int>::max(); Bad(m); }
    { Model m(true); At<studiohdr_t>(m.bytes, 0).seqgroupindex = std::numeric_limits<int>::max(); Bad(m); }
    { Model m(true); At<mstudioseqdesc_t>(m.bytes, At<studiohdr_t>(m.bytes, 0).seqindex).seqgroup = 1; Bad(m); }
    { Model m(true); At<mstudioseqdesc_t>(m.bytes, At<studiohdr_t>(m.bytes, 0).seqindex).animindex = std::numeric_limits<int>::max(); Bad(m); }
    { Model m(true); auto hdr = At<studiohdr_t>(m.bytes, 0); At<mstudioanim_t>(m.bytes, At<mstudioseqdesc_t>(m.bytes, hdr.seqindex).animindex).offset[0] = 65535; Bad(m); }
    // Truncations and random mutations must not read beyond buffers.
    { Model m; for(size_t n = 0; n < m.bytes.size(); ++n) { std::string e; legacycp::Validate(m.bytes.data(), n, e); } }
    // Deterministic mutation corpus: any return value is acceptable, but a
    // sanitizer fault, infinite loop or out-of-bounds read is not.
    {
        Model seed(true);
        std::mt19937 generator(10101u);
        for(int attempt = 0; attempt < 5000; ++attempt)
        {
            std::vector<unsigned char> mutated = seed.bytes;
            for(int edit = 0; edit < 3; ++edit)
            {
                const size_t offset = static_cast<size_t>(generator()) % mutated.size();
                mutated[offset] = static_cast<unsigned char>(generator());
            }
            std::string error;
            legacycp::Validate(mutated.data(), mutated.size(), error);
        }
    }
    std::cout << "I.10.1 legacy _cp studio validation: 25 targeted cases, truncation sweep, 5000 mutations PASS\n";
}
