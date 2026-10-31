// anim_cook: turns a raw skeletal export (.skraw, written by py/pack_skeletal.py in Blender)
// into the runtime files the game loads:
//
//   <out>.skel         ozz skeleton archive
//   <out>.<name>.anim  ozz animation archive per Blender action (keyframes optimized)
//   <out>.skin         skinned mesh (include/render/skinned_mesh.h), joint indices and inverse
//                      bind matrices remapped to the ozz skeleton's joint order
//
// Usage: anim_cook <input.skraw> <output path without extension>
// Built by build_tools.bat; run by cook.bat.

#include "render/skinned_mesh.h"

#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/animation_optimizer.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    using ozz::animation::offline::RawAnimation;
    using ozz::animation::offline::RawSkeleton;

    struct Transform
    {
        float translation[3];
        float rotation[4]; // x, y, z, w
        float scale[3];
    };

    struct Joint
    {
        std::string name;
        int32_t     parent;
        Transform   rest;
    };

    struct Clip
    {
        std::string            name;
        float                  frameRate;
        uint32_t               frameCount;
        std::vector<Transform> frames; // frameCount x jointCount
    };

    struct RawVertex
    {
        float    position[3];
        float    normal[3];
        uint16_t joints[4];
        float    weights[4];
        uint8_t  color[4];
    };
    static_assert(sizeof(RawVertex) == 52);

    struct RawSkeletal
    {
        std::vector<Joint>     joints;
        std::vector<Clip>      clips;
        std::vector<RawVertex> vertices;
        std::vector<uint32_t>  indices;
        std::vector<float>     inverseBinds; // jointCount x 12
    };

    [[noreturn]] void Fail(const std::string& msg)
    {
        throw std::runtime_error(msg);
    }

    class Reader
    {
    public:
        explicit Reader(std::vector<uint8_t> bytes) : m_bytes(std::move(bytes)) {}

        void Read(void* out, size_t size)
        {
            if (size > m_bytes.size() - m_pos)
                Fail("unexpected end of file");
            memcpy(out, m_bytes.data() + m_pos, size);
            m_pos += size;
        }

        template <typename T>
        T Read()
        {
            T value;
            Read(&value, sizeof(value));
            return value;
        }

        std::string ReadString()
        {
            const uint32_t length = Read<uint32_t>();
            std::string    s(length, '\0');
            Read(s.data(), length);
            return s;
        }

        bool AtEnd() const { return m_pos == m_bytes.size(); }

    private:
        std::vector<uint8_t> m_bytes;
        size_t               m_pos = 0;
    };

    RawSkeletal ReadRawFile(const char* path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
            Fail(std::string("can't open ") + path);
        Reader in(std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()));

        char magic[4];
        in.Read(magic, 4);
        if (memcmp(magic, "DXSR", 4) != 0 || in.Read<uint32_t>() != 2)
            Fail("not a version 2 .skraw file (re-export with py/pack_skeletal.py)");

        RawSkeletal raw;
        raw.joints.resize(in.Read<uint32_t>());
        for (size_t j = 0; j < raw.joints.size(); ++j)
        {
            Joint& joint = raw.joints[j];
            joint.name   = in.ReadString();
            joint.parent = in.Read<int32_t>();
            joint.rest   = in.Read<Transform>();
            if (joint.parent >= (int32_t)j)
                Fail("joint '" + joint.name + "' comes before its parent");
        }

        raw.clips.resize(in.Read<uint32_t>());
        for (Clip& clip : raw.clips)
        {
            clip.name       = in.ReadString();
            clip.frameRate  = in.Read<float>();
            clip.frameCount = in.Read<uint32_t>();
            if (clip.frameCount < 2 || !(clip.frameRate > 0.0f))
                Fail("animation '" + clip.name + "' has no duration");
            clip.frames.resize((size_t)clip.frameCount * raw.joints.size());
            in.Read(clip.frames.data(), clip.frames.size() * sizeof(Transform));
        }

        raw.vertices.resize(in.Read<uint32_t>());
        in.Read(raw.vertices.data(), raw.vertices.size() * sizeof(RawVertex));
        raw.indices.resize(in.Read<uint32_t>());
        in.Read(raw.indices.data(), raw.indices.size() * sizeof(uint32_t));
        raw.inverseBinds.resize(raw.joints.size() * 12);
        in.Read(raw.inverseBinds.data(), raw.inverseBinds.size() * sizeof(float));
        if (!in.AtEnd())
            Fail("trailing data");
        return raw;
    }

    ozz::math::Transform ToOzz(const Transform& t)
    {
        ozz::math::Transform out;
        out.translation = ozz::math::Float3(t.translation[0], t.translation[1], t.translation[2]);
        out.rotation    = ozz::math::Quaternion(t.rotation[0], t.rotation[1], t.rotation[2], t.rotation[3]);
        out.scale       = ozz::math::Float3(t.scale[0], t.scale[1], t.scale[2]);
        return out;
    }

    RawSkeleton::Joint BuildRawJoint(const RawSkeletal& raw, int32_t index)
    {
        RawSkeleton::Joint joint;
        joint.name      = raw.joints[index].name.c_str();
        joint.transform = ToOzz(raw.joints[index].rest);
        for (size_t child = 0; child < raw.joints.size(); ++child)
        {
            if (raw.joints[child].parent == index)
                joint.children.push_back(BuildRawJoint(raw, (int32_t)child));
        }
        return joint;
    }

    template <typename T>
    void WriteArchive(const std::string& path, const T& object)
    {
        ozz::io::File file(path.c_str(), "wb");
        if (!file.opened())
            Fail("can't write " + path);
        ozz::io::OArchive archive(&file);
        archive << object;
    }

    // Keeps file names portable: anything unusual becomes '_'.
    std::string SanitizeName(const std::string& name)
    {
        std::string out = name;
        for (char& c : out)
        {
            const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
            if (!ok)
                c = '_';
        }
        return out.empty() ? "unnamed" : out;
    }

    size_t CountKeys(const RawAnimation& animation)
    {
        size_t keys = 0;
        for (const RawAnimation::JointTrack& track : animation.tracks)
            keys += track.translations.size() + track.rotations.size() + track.scales.size();
        return keys;
    }

    void Cook(const char* inputPath, const std::string& outputBase)
    {
        const RawSkeletal raw = ReadRawFile(inputPath);
        if (raw.joints.empty())
            Fail("no joints");

        // Skeleton. ozz decides the final joint order, so everything else is remapped by name.
        RawSkeleton rawSkeleton;
        for (size_t j = 0; j < raw.joints.size(); ++j)
        {
            if (raw.joints[j].parent < 0)
                rawSkeleton.roots.push_back(BuildRawJoint(raw, (int32_t)j));
        }
        if (!rawSkeleton.Validate())
            Fail("invalid skeleton");
        const ozz::unique_ptr<ozz::animation::Skeleton> skeleton = ozz::animation::offline::SkeletonBuilder()(rawSkeleton);
        if (!skeleton)
            Fail("SkeletonBuilder failed");
        if ((uint32_t)skeleton->num_joints() > kMaxSkinJoints)
            Fail("too many joints for 8-bit skin indices");

        std::unordered_map<std::string, int> ozzIndexOf;
        for (int i = 0; i < skeleton->num_joints(); ++i)
            ozzIndexOf[skeleton->joint_names()[i]] = i;
        std::vector<int> toOzz(raw.joints.size());
        for (size_t j = 0; j < raw.joints.size(); ++j)
            toOzz[j] = ozzIndexOf.at(raw.joints[j].name);

        WriteArchive(outputBase + ".skel", *skeleton);
        printf("    skeleton: %d joints\n", skeleton->num_joints());

        // Animations: one track per joint, one key per exported frame, then optimized.
        for (const Clip& clip : raw.clips)
        {
            RawAnimation animation;
            animation.name     = clip.name.c_str();
            animation.duration = (float)(clip.frameCount - 1) / clip.frameRate;
            animation.tracks.resize(skeleton->num_joints());
            for (uint32_t f = 0; f < clip.frameCount; ++f)
            {
                const float time = f == clip.frameCount - 1 ? animation.duration : (float)f / clip.frameRate;
                for (size_t j = 0; j < raw.joints.size(); ++j)
                {
                    const ozz::math::Transform t = ToOzz(clip.frames[f * raw.joints.size() + j]);
                    RawAnimation::JointTrack& track = animation.tracks[toOzz[j]];
                    track.translations.push_back({ time, t.translation });
                    track.rotations.push_back({ time, t.rotation });
                    track.scales.push_back({ time, t.scale });
                }
            }
            if (!animation.Validate())
                Fail("invalid animation '" + clip.name + "'");

            RawAnimation optimized;
            if (!ozz::animation::offline::AnimationOptimizer()(animation, *skeleton, &optimized))
                Fail("AnimationOptimizer failed for '" + clip.name + "'");
            const ozz::unique_ptr<ozz::animation::Animation> runtime = ozz::animation::offline::AnimationBuilder()(optimized);
            if (!runtime)
                Fail("AnimationBuilder failed for '" + clip.name + "'");

            WriteArchive(outputBase + "." + SanitizeName(clip.name) + ".anim", *runtime);
            printf("    animation '%s': %.2f s, %zu -> %zu keys\n", clip.name.c_str(), animation.duration,
                   CountKeys(animation), CountKeys(optimized));
        }

        // Skinned mesh.
        SkinnedMeshFileHeader header = {};
        memcpy(header.magic, "DXSM", 4);
        header.version     = kSkinnedMeshFileVersion;
        header.vertexCount = (uint32_t)raw.vertices.size();
        header.indexCount  = (uint32_t)raw.indices.size();
        header.jointCount  = (uint32_t)skeleton->num_joints();
        if (header.vertexCount == 0 || header.indexCount == 0 || header.indexCount % 3 != 0)
            Fail("invalid mesh");

        std::vector<SkinnedVertex> vertices(raw.vertices.size());
        float boundsMin[3] = { INFINITY, INFINITY, INFINITY };
        float boundsMax[3] = { -INFINITY, -INFINITY, -INFINITY };
        for (size_t v = 0; v < raw.vertices.size(); ++v)
        {
            const RawVertex& in  = raw.vertices[v];
            SkinnedVertex&   out = vertices[v];
            memcpy(out.position, in.position, sizeof(out.position));
            memcpy(out.normal, in.normal, sizeof(out.normal));
            memcpy(out.weights, in.weights, sizeof(out.weights));
            memcpy(out.color, in.color, sizeof(out.color));
            for (int i = 0; i < 4; ++i)
            {
                if (in.joints[i] >= raw.joints.size())
                    Fail("vertex joint index out of range");
                out.joints[i] = (uint8_t)toOzz[in.joints[i]];
            }
            for (int a = 0; a < 3; ++a)
            {
                boundsMin[a] = std::min(boundsMin[a], in.position[a]);
                boundsMax[a] = std::max(boundsMax[a], in.position[a]);
            }
        }
        for (uint32_t index : raw.indices)
        {
            if (index >= header.vertexCount)
                Fail("index out of range");
        }

        float radiusSq = 0.0f;
        for (int a = 0; a < 3; ++a)
            header.sphereCenter[a] = (boundsMin[a] + boundsMax[a]) * 0.5f;
        for (const SkinnedVertex& v : vertices)
        {
            const float dx = v.position[0] - header.sphereCenter[0];
            const float dy = v.position[1] - header.sphereCenter[1];
            const float dz = v.position[2] - header.sphereCenter[2];
            radiusSq = std::max(radiusSq, dx * dx + dy * dy + dz * dz);
        }
        header.sphereRadius = std::sqrt(radiusSq);

        std::vector<float> inverseBinds(raw.inverseBinds.size());
        for (size_t j = 0; j < raw.joints.size(); ++j)
            memcpy(&inverseBinds[toOzz[j] * 12], &raw.inverseBinds[j * 12], 12 * sizeof(float));

        const std::string skinPath = outputBase + ".skin";
        std::ofstream     skin(skinPath, std::ios::binary);
        skin.write(reinterpret_cast<const char*>(&header), sizeof(header));
        skin.write(reinterpret_cast<const char*>(vertices.data()), vertices.size() * sizeof(SkinnedVertex));
        skin.write(reinterpret_cast<const char*>(raw.indices.data()), raw.indices.size() * sizeof(uint32_t));
        skin.write(reinterpret_cast<const char*>(inverseBinds.data()), inverseBinds.size() * sizeof(float));
        if (!skin)
            Fail("can't write " + skinPath);
        printf("    skin: %u vertices, %u triangles\n", header.vertexCount, header.indexCount / 3);
    }
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: anim_cook <input.skraw> <output path without extension>\n");
        return 2;
    }
    try
    {
        Cook(argv[1], argv[2]);
    }
    catch (const std::exception& e)
    {
        fprintf(stderr, "  %s: %s\n", argv[1], e.what());
        return 1;
    }
    return 0;
}
