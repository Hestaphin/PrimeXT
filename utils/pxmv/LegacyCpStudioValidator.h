#pragma once

// Studio v10 input is untrusted on disk. Verify every region which the legacy
// collision-proxy extractor traverses BEFORE it uses typed pointers into it.
// This header intentionally has no UI/GL/PhysX dependencies so it can be fuzzed.
#include "studio.h"
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

namespace legacycp
{
class View
{
public:
    View(const void *bytes, size_t length) : m_bytes(static_cast<const unsigned char *>(bytes)), m_length(length) {}

    template<typename T>
    bool Array(std::int64_t offset, size_t count, const T *&out) const
    {
        out = nullptr;
        if(!m_bytes || offset < 0 || static_cast<std::uint64_t>(offset) > m_length) return false;
        const size_t at = static_cast<size_t>(offset);
        // The importer uses typed pointers to Studio structures. Reject
        // misaligned tables rather than invoking undefined behavior.
        if(at % alignof(T) != 0 || count > (m_length - at) / sizeof(T)) return false;
        out = reinterpret_cast<const T *>(m_bytes + at);
        return true;
    }

    bool Short(size_t offset, std::int16_t &value) const
    {
        if(offset > m_length || sizeof(value) > m_length - offset) return false;
        std::memcpy(&value, m_bytes + offset, sizeof(value));
        return true;
    }

    static bool Name(const char *name, size_t capacity)
    {
        return std::memchr(name, '\0', capacity) != nullptr;
    }

    size_t Length() const { return m_length; }
private:
    const unsigned char *m_bytes;
    size_t m_length;
};

// Validates the subset of Studio v10 needed by the _cp importer, including
// zero-frame bone animation values and each terminated triangle command stream.
inline bool Validate(const void *fileBytes, size_t fileLength, std::string &error)
{
    auto fail = [&](const char *reason) -> bool { error = reason; return false; };
    if(!fileBytes || fileLength < sizeof(studiohdr_t))
        return fail("Legacy collision proxy is truncated.");
    View physical(fileBytes, fileLength);
    const studiohdr_t *header = nullptr;
    if(!physical.Array(0, 1, header)) return fail("Legacy collision proxy header is invalid.");
    if(header->ident != IDSTUDIOHEADER || header->version != STUDIO_VERSION ||
       header->length < static_cast<int>(sizeof(studiohdr_t)) ||
       static_cast<std::uint64_t>(header->length) > fileLength)
        return fail("Legacy collision proxy is not a valid Studio v10 MDL.");
    // The declared Studio length, not trailing data, defines readable regions.
    View view(fileBytes, static_cast<size_t>(header->length));

    if(header->numbones <= 0 || header->numbones > MAXSTUDIOBONES)
        return fail("Legacy collision proxy has an invalid bone count.");
    const mstudiobone_t *bones = nullptr;
    if(!view.Array(header->boneindex, static_cast<size_t>(header->numbones), bones))
        return fail("Legacy collision proxy bone table extends outside the file.");
    for(int i = 0; i < header->numbones; ++i)
    {
        if(!View::Name(bones[i].name, sizeof(bones[i].name)) ||
           bones[i].parent < -1 || bones[i].parent >= i)
            return fail("Legacy collision proxy has an invalid bone name or hierarchy.");
        for(int axis = 0; axis < 6; ++axis)
            if(!std::isfinite(bones[i].value[axis]) || !std::isfinite(bones[i].scale[axis]))
                return fail("Legacy collision proxy contains a non-finite bone transform.");
    }

    if(header->numseq < 0 || header->numseqgroups < 0 || header->numseqgroups > MAXSTUDIOGROUPS)
        return fail("Legacy collision proxy has invalid sequence counts.");
    if(header->numseq > 0)
    {
        const mstudioseqdesc_t *seq = nullptr;
        if(!view.Array(header->seqindex, 1, seq))
            return fail("Legacy collision proxy sequence table extends outside the file.");
        if(seq->seqgroup != 0)
            return fail("Legacy _cp.mdl sequence 0 is external; importer requires sequence group 0.");
        const mstudioseqgroup_t *groups = nullptr;
        if(header->numseqgroups < 1 ||
           !view.Array(header->seqgroupindex, static_cast<size_t>(header->numseqgroups), groups))
            return fail("Legacy collision proxy sequence-group table extends outside the file.");
        const std::int64_t animOffset = static_cast<std::int64_t>(groups[0].data) + seq->animindex;
        const mstudioanim_t *anim = nullptr;
        if(!view.Array(animOffset, static_cast<size_t>(header->numbones), anim))
            return fail("Legacy collision proxy animation table extends outside the file.");
        for(int bone = 0; bone < header->numbones; ++bone)
            for(int axis = 0; axis < 6; ++axis)
                if(anim[bone].offset[axis] != 0)
                {
                    // Extractor reads value[1] (the first frame), hence TWO entries.
                    const std::int64_t valueOffset = animOffset +
                        static_cast<std::int64_t>(bone) * sizeof(mstudioanim_t) + anim[bone].offset[axis];
                    const mstudioanimvalue_t *values = nullptr;
                    if(!view.Array(valueOffset, 2, values))
                        return fail("Legacy collision proxy animation values extend outside the file.");
                }
    }

    if(header->numbodyparts <= 0 || header->numbodyparts > MAXSTUDIOBODYPARTS)
        return fail("Legacy collision proxy has an invalid bodypart count.");
    const mstudiobodyparts_t *parts = nullptr;
    if(!view.Array(header->bodypartindex, static_cast<size_t>(header->numbodyparts), parts))
        return fail("Legacy collision proxy bodypart table extends outside the file.");
    // Bound work even when multiple bodyparts or meshes overlap the same input
    // command stream. Otherwise a tiny malicious file can expand indefinitely.
    constexpr size_t kMaxImportedTriangles = 2000000;
    size_t triangleBudget = kMaxImportedTriangles;
    for(int p = 0; p < header->numbodyparts; ++p)
    {
        const auto &part = parts[p];
        if(!View::Name(part.name, sizeof(part.name)) ||
           part.nummodels < 0 || part.nummodels > MAXSTUDIOMODELS)
            return fail("Legacy collision proxy has an invalid bodypart name or model count.");
        if(part.nummodels == 0) continue;
        const mstudiomodel_t *models = nullptr;
        if(!view.Array(part.modelindex, static_cast<size_t>(part.nummodels), models))
            return fail("Legacy collision proxy model table extends outside the file.");
        for(int m = 0; m < part.nummodels; ++m)
        {
            const auto &model = models[m];
            if(model.numverts < 0 || model.numverts > MAXSTUDIOVERTS || model.nummesh < 0)
                return fail("Legacy collision proxy has an invalid vertex or mesh count.");
            if(!model.numverts || !model.nummesh) continue;
            const vec3_t *vertices = nullptr;
            const unsigned char *vertexBones = nullptr;
            const mstudiomesh_t *meshes = nullptr;
            if(!view.Array(model.vertindex, static_cast<size_t>(model.numverts), vertices) ||
               !view.Array(model.vertinfoindex, static_cast<size_t>(model.numverts), vertexBones) ||
               !view.Array(model.meshindex, static_cast<size_t>(model.nummesh), meshes))
                return fail("Legacy collision proxy vertex/mesh table extends outside the file.");
            for(int v = 0; v < model.numverts; ++v)
            {
                if(vertexBones[v] >= header->numbones)
                    return fail("Legacy collision proxy has an invalid vertex bone index.");
                for(int axis = 0; axis < 3; ++axis)
                    if(!std::isfinite(vertices[v][axis]))
                        return fail("Legacy collision proxy contains a non-finite vertex.");
            }
            for(int mesh = 0; mesh < model.nummesh; ++mesh)
            {
                // Command streams are signed 16-bit words terminated by zero.
                // Per vertex: index, normal index, s, t. Limit by remaining bytes.
                if(meshes[mesh].triindex < 0)
                    return fail("Legacy collision proxy has an invalid triangle-command offset.");
                size_t cursor = static_cast<size_t>(meshes[mesh].triindex);
                if(cursor % alignof(std::int16_t))
                    return fail("Legacy collision proxy has an unaligned triangle-command stream.");
                for(;;)
                {
                    std::int16_t signedCount = 0;
                    if(!view.Short(cursor, signedCount))
                        return fail("Legacy collision proxy triangle commands lack a terminator.");
                    cursor += sizeof(signedCount);
                    if(!signedCount) break;
                    const unsigned count = signedCount < 0 ? static_cast<unsigned>(-static_cast<int>(signedCount))
                                                          : static_cast<unsigned>(signedCount);
                    const size_t triangles = count > 2 ? count - 2 : 0;
                    if(triangles > triangleBudget)
                        return fail("Legacy collision proxy has excessive triangle geometry.");
                    triangleBudget -= triangles;
                    const size_t words = static_cast<size_t>(count) * 4;
                    if(cursor > view.Length() || words > (view.Length() - cursor) / sizeof(std::int16_t))
                        return fail("Legacy collision proxy triangle command extends outside the file.");
                    for(unsigned v = 0; v < count; ++v)
                    {
                        std::int16_t index = 0;
                        if(!view.Short(cursor + v * 8, index) || index < 0 || index >= model.numverts)
                            return fail("Legacy collision proxy contains an invalid triangle-command vertex.");
                    }
                    cursor += words * sizeof(std::int16_t);
                }
            }
        }
    }
    return true;
}
} // namespace legacycp
