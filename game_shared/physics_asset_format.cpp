#include "physics_asset_format.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#ifndef _WIN32
#include <strings.h>
#endif

namespace physicsasset
{
namespace
{
struct Token
{
    std::string text;
    int line = 1;
};

static std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return value;
}

static bool IsFinite(float value) { return std::isfinite(value); }
static bool IsFinite(const Vec3 &value) { return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z); }

static bool SetValidationError(std::string *error, const std::string &message)
{
    if(error) *error = message;
    return false;
}

class Lexer
{
public:
    explicit Lexer(std::string_view input) : m_input(input) {}

    bool Next(Token &out)
    {
        SkipWhitespaceAndComments();
        if(m_pos >= m_input.size())
            return false;

        out.line = m_line;
        const char c = m_input[m_pos];
        if(c == '{' || c == '}')
        {
            out.text.assign(1, c);
            ++m_pos;
            return true;
        }

        if(c == '"')
        {
            ++m_pos;
            std::string value;
            while(m_pos < m_input.size())
            {
                const char ch = m_input[m_pos++];
                if(ch == '"')
                {
                    out.text = std::move(value);
                    return true;
                }
                if(ch == '\\' && m_pos < m_input.size())
                {
                    const char escaped = m_input[m_pos++];
                    if(escaped == 'n') value.push_back('\n');
                    else if(escaped == 't') value.push_back('\t');
                    else value.push_back(escaped);
                }
                else
                {
                    if(ch == '\n') ++m_line;
                    value.push_back(ch);
                }
            }
            SetError("unterminated quoted string", out.line);
            return false;
        }

        const size_t begin = m_pos;
        while(m_pos < m_input.size())
        {
            const char ch = m_input[m_pos];
            if(std::isspace(static_cast<unsigned char>(ch)) || ch == '{' || ch == '}')
                break;
            if(ch == '/' && m_pos + 1 < m_input.size() && m_input[m_pos + 1] == '/')
                break;
            ++m_pos;
        }
        out.text.assign(m_input.substr(begin, m_pos - begin));
        return !out.text.empty();
    }

    const std::string &Error() const { return m_error; }

private:
    void SkipWhitespaceAndComments()
    {
        for(;;)
        {
            while(m_pos < m_input.size() && std::isspace(static_cast<unsigned char>(m_input[m_pos])))
            {
                if(m_input[m_pos] == '\n') ++m_line;
                ++m_pos;
            }
            if(m_pos + 1 < m_input.size() && m_input[m_pos] == '/' && m_input[m_pos + 1] == '/')
            {
                m_pos += 2;
                while(m_pos < m_input.size() && m_input[m_pos] != '\n') ++m_pos;
                continue;
            }
            if(m_pos < m_input.size() && m_input[m_pos] == '#')
            {
                while(m_pos < m_input.size() && m_input[m_pos] != '\n') ++m_pos;
                continue;
            }
            break;
        }
    }

    void SetError(const char *message, int line)
    {
        std::ostringstream ss;
        ss << "line " << line << ": " << message;
        m_error = ss.str();
    }

    std::string_view m_input;
    size_t m_pos = 0;
    int m_line = 1;
    std::string m_error;
};

class Parser
{
public:
    explicit Parser(std::string_view text) : m_lexer(text) {}

    bool Run(AssetDef &out, std::string *error)
    {
        out = AssetDef{};
        Token token;
        if(!Read(token) || token.text != "physics_asset")
            return Fail(token, "expected 'physics_asset'", error);
        if(!Expect("{", error)) return false;

        bool closed = false;
        bool sawVersion = false;
        while(Read(token))
        {
            if(token.text == "}") { closed = true; break; }
            if(token.text == "version")
            {
                if(sawVersion) return Fail(token, "duplicate version declaration", error);
                if(!Read(token)) return Fail(token, "missing version value", error);
                if(!ToInt(token, out.version, error)) return false;
                sawVersion = true;
            }
            else if(token.text == "collision")
            {
                CollisionDef collision;
                if(!ParseCollision(collision, error)) return false;
                out.collisions.push_back(std::move(collision));
            }
            else if(token.text == "body")
            {
                BodyDef body;
                if(!ParseBody(body, error)) return false;
                out.bodies.push_back(std::move(body));
            }
            else if(token.text == "joint")
            {
                JointDef joint;
                if(!ParseJoint(joint, error)) return false;
                out.joints.push_back(std::move(joint));
            }
            else
            {
                return Fail(token, "unknown top-level token '" + token.text + "'", error);
            }
        }

        if(!closed) return Fail(token, "missing closing '}'", error);
        if(!sawVersion) return Fail(token, "missing physics asset version", error);
        Token trailing;
        if(Read(trailing)) return Fail(trailing, "unexpected token after physics_asset block", error);
        if(!m_lastError.empty()) return Fail(trailing, m_lastError, error);

        std::string validationError;
        if(!Validate(out, &validationError))
            return Fail(token, validationError, error);
        return true;
    }

private:
    bool Read(Token &token)
    {
        if(m_lexer.Next(token)) return true;
        if(!m_lexer.Error().empty()) m_lastError = m_lexer.Error();
        return false;
    }

    bool Expect(const char *expected, std::string *error)
    {
        Token token;
        if(!Read(token) || token.text != expected)
            return Fail(token, std::string("expected '") + expected + "'", error);
        return true;
    }

    bool ParseCollision(CollisionDef &collision, std::string *error)
    {
        Token token;
        if(!Read(token)) return Fail(token, "missing collision name", error);
        collision.name = token.text;
        if(!Expect("{", error)) return false;
        bool closed = false;
        while(Read(token))
        {
            if(token.text == "}") { closed = true; break; }
            if(token.text == "type")
            {
                if(!Read(token) || !ParseCollisionGeometryType(token.text.c_str(), collision.type))
                    return Fail(token, "unknown collision geometry type", error);
            }
            else if(token.text == "bodygroup")
            {
                if(!Read(token)) return Fail(token, "missing bodygroup name", error);
                collision.bodyGroup = token.text;
            }
            else if(token.text == "variant")
            {
                if(!Read(token)) return Fail(token, "missing bodygroup variant", error);
                if(!ToInt(token, collision.variant, error)) return false;
            }
            else if(token.text == "bone")
            {
                if(!Read(token)) return Fail(token, "missing collision bone name", error);
                collision.bone = token.text;
            }
            else if(token.text == "disabled")
            {
                int value = 0;
                if(!Read(token)) return Fail(token, "missing disabled value", error);
                if(!ToInt(token, value, error) || (value != 0 && value != 1)) return Fail(token, "disabled must be 0 or 1", error);
                collision.disabled = value != 0;
            }
            else if(token.text == "vertex")
            {
                Vec3 vertex;
                if(!ReadVec3(vertex, error)) return false;
                collision.vertices.push_back(vertex);
            }
            else if(token.text == "triangle")
            {
                Token a, b, c;
                int ia = 0, ib = 0, ic = 0;
                if(!Read(a) || !Read(b) || !Read(c)) return Fail(c, "incomplete triangle", error);
                if(!ToInt(a, ia, error) || !ToInt(b, ib, error) || !ToInt(c, ic, error)) return false;
                if(ia < 0 || ib < 0 || ic < 0) return Fail(a, "triangle indices must be non-negative", error);
                collision.indices.push_back(static_cast<std::uint32_t>(ia));
                collision.indices.push_back(static_cast<std::uint32_t>(ib));
                collision.indices.push_back(static_cast<std::uint32_t>(ic));
            }
            else return Fail(token, "unknown collision key '" + token.text + "'", error);
        }
        if(!closed) return Fail(token, "missing collision closing '}'", error);
        return true;
    }

    bool ParseBody(BodyDef &body, std::string *error)
    {
        Token token;
        if(!Read(token)) return Fail(token, "missing body name", error);
        body.name = token.text;
        if(!Expect("{", error)) return false;
        bool closed = false;
        while(Read(token))
        {
            if(token.text == "}") { closed = true; break; }
            if(token.text == "bone")
            {
                if(!Read(token)) return Fail(token, "missing bone name", error);
                body.bone = token.text;
            }
            else if(token.text == "shape")
            {
                if(!Read(token) || !ParseShapeType(token.text.c_str(), body.shape.type))
                    return Fail(token, "unknown shape type", error);
            }
            else if(token.text == "half_extents") { if(!ReadVec3(body.shape.halfExtents, error)) return false; }
            else if(token.text == "radius") { if(!ReadFloat(body.shape.radius, error)) return false; }
            else if(token.text == "half_height") { if(!ReadFloat(body.shape.halfHeight, error)) return false; }
            else if(token.text == "offset") { if(!ReadVec3(body.offset, error)) return false; }
            else if(token.text == "angles") { if(!ReadVec3(body.angles, error)) return false; }
            else if(token.text == "mass") { if(!ReadFloat(body.mass, error)) return false; }
            else if(token.text == "part") { if(!Read(token)) return Fail(token, "missing part name", error); body.part = token.text; }
            else if(token.text == "prop_drive") { if(!Read(token)) return Fail(token, "missing prop drive mode", error); if(token.text == "animated") body.propDrive = PropDriveMode::Animated; else if(token.text == "driven") body.propDrive = PropDriveMode::Driven; else if(token.text == "free") body.propDrive = PropDriveMode::Free; else return Fail(token, "unknown prop drive mode", error); }
            else if(token.text == "drive_strength") { if(!ReadFloat(body.driveStrength, error)) return false; }
            else if(token.text == "drive_damping") { if(!ReadFloat(body.driveDamping, error)) return false; }
            else if(token.text == "drive_max_force") { if(!ReadFloat(body.driveMaxForce, error)) return false; }
            else if(token.text == "linear_damping") { if(!ReadFloat(body.linearDamping, error)) return false; }
            else if(token.text == "angular_damping") { if(!ReadFloat(body.angularDamping, error)) return false; }
            else if(token.text == "hitgroup")
            {
                if(!Read(token)) return Fail(token, "missing hitgroup value", error);
                if(!ToInt(token, body.hitGroup, error)) return false;
            }
            else return Fail(token, "unknown body key '" + token.text + "'", error);
        }
        if(!closed) return Fail(token, "missing body closing '}'", error);
        return true;
    }

    bool ParseJoint(JointDef &joint, std::string *error)
    {
        Token token;
        if(!Read(token)) return Fail(token, "missing joint name", error);
        joint.name = token.text;
        if(!Expect("{", error)) return false;
        bool closed = false;
        while(Read(token))
        {
            if(token.text == "}") { closed = true; break; }
            if(token.text == "parent") { if(!Read(token)) return Fail(token, "missing parent body", error); joint.parent = token.text; }
            else if(token.text == "child") { if(!Read(token)) return Fail(token, "missing child body", error); joint.child = token.text; }
            else if(token.text == "kind") { if(!Read(token)) return Fail(token, "missing joint kind", error); if(token.text == "hinge") joint.kind = JointKind::Hinge; else if(token.text == "d6") joint.kind = JointKind::D6; else return Fail(token, "unknown joint kind", error); }
            else if(token.text == "hinge_min") { if(!ReadFloat(joint.hingeMin, error)) return false; }
            else if(token.text == "hinge_max") { if(!ReadFloat(joint.hingeMax, error)) return false; }
            else if(token.text == "hinge_anchor") { if(!ReadVec3(joint.hingeAnchor, error)) return false; }
            else if(token.text == "hinge_axis") { if(!ReadVec3(joint.hingeAxis, error)) return false; }
            else if(token.text == "break_impulse") { if(!ReadFloat(joint.breakImpulse, error)) return false; }
            else if(token.text == "release_damage") { if(!ReadFloat(joint.releaseDamage, error)) return false; }
            else if(token.text == "break_damage") { if(!ReadFloat(joint.breakDamage, error)) return false; }
            else if(token.text == "swing1") { if(!ReadFloat(joint.swing1, error)) return false; }
            else if(token.text == "swing2") { if(!ReadFloat(joint.swing2, error)) return false; }
            else if(token.text == "twist_low") { if(!ReadFloat(joint.twistLow, error)) return false; }
            else if(token.text == "twist_high") { if(!ReadFloat(joint.twistHigh, error)) return false; }
            else return Fail(token, "unknown joint key '" + token.text + "'", error);
        }
        if(!closed) return Fail(token, "missing joint closing '}'", error);
        return true;
    }

    bool ReadVec3(Vec3 &v, std::string *error)
    {
        Token x, y, z;
        if(!Read(x) || !Read(y) || !Read(z)) return Fail(z, "incomplete vector", error);
        return ToFloat(x, v.x, error) && ToFloat(y, v.y, error) && ToFloat(z, v.z, error);
    }

    bool ReadFloat(float &value, std::string *error)
    {
        Token token;
        if(!Read(token)) return Fail(token, "missing numeric value", error);
        return ToFloat(token, value, error);
    }

    bool ToFloat(const Token &token, float &value, std::string *error)
    {
        char *end = nullptr;
        errno = 0;
        value = std::strtof(token.text.c_str(), &end);
        if(errno || !end || end == token.text.c_str() || *end != '\0' || !std::isfinite(value))
            return Fail(token, "invalid finite number '" + token.text + "'", error);
        return true;
    }

    bool ToInt(const Token &token, int &value, std::string *error)
    {
        char *end = nullptr;
        errno = 0;
        long parsed = std::strtol(token.text.c_str(), &end, 10);
        if(errno || !end || end == token.text.c_str() || *end != '\0' || parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max())
            return Fail(token, "invalid integer '" + token.text + "'", error);
        value = static_cast<int>(parsed);
        return true;
    }

    bool Fail(const Token &token, const std::string &message, std::string *error)
    {
        if(error)
        {
            if(!m_lastError.empty()) *error = m_lastError;
            else
            {
                std::ostringstream ss;
                ss << "line " << token.line << ": " << message;
                *error = ss.str();
            }
        }
        return false;
    }

    Lexer m_lexer;
    std::string m_lastError;
};

static std::string Quote(const std::string &value)
{
    std::string out = "\"";
    for(char c : value)
    {
        if(c == '\\' || c == '"') out.push_back('\\');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

static void WriteVec3(std::ostringstream &ss, const Vec3 &v)
{
    ss << v.x << ' ' << v.y << ' ' << v.z;
}
}

std::string BuildAssetPath(const std::string &modelPath)
{
    std::string path = modelPath;
    const size_t slash = path.find_last_of("/\\");
    const size_t dot = path.find_last_of('.');
    if(dot != std::string::npos && (slash == std::string::npos || dot > slash))
        path.resize(dot);
    path += ".phys";
    return path;
}

bool Validate(const AssetDef &asset, std::string *error)
{
    if(asset.version != kLegacyVersion && asset.version != kCollisionVersion && asset.version != kBoneCollisionVersion && asset.version != kCurrentVersion)
        return SetValidationError(error, "unsupported physics asset version");
    if(asset.version == kLegacyVersion && !asset.collisions.empty())
        return SetValidationError(error, "collision sections require physics asset version 2 or newer");
    if(asset.bodies.empty() && asset.collisions.empty())
        return SetValidationError(error, "physics asset has neither collision geometry nor articulated bodies");

    std::unordered_set<std::string> collisionNames;
    for(const CollisionDef &collision : asset.collisions)
    {
        // Do not silently cook an invalid programmatically-generated enum as
        // triangle_mesh. File parsing already rejects invalid type strings.
        if(collision.type != CollisionGeometryType::TriangleMesh && collision.type != CollisionGeometryType::Convex)
            return SetValidationError(error, "invalid collision geometry type");
        if(collision.name.empty())
            return SetValidationError(error, "collision section requires a name");
        if(!collisionNames.insert(Lower(collision.name)).second)
            return SetValidationError(error, "duplicate collision name '" + collision.name + "'");
        if(asset.version < kBoneCollisionVersion && !collision.bone.empty())
            return SetValidationError(error, "bone-bound collision requires physics asset version 3");
        if(collision.disabled)
        {
            if(!collision.vertices.empty() || !collision.indices.empty())
                return SetValidationError(error, "disabled collision '" + collision.name + "' must not contain geometry");
        }
        else
        {
            if(collision.vertices.size() < 3)
                return SetValidationError(error, "collision '" + collision.name + "' requires at least three vertices");
            if(collision.indices.empty() || (collision.indices.size() % 3) != 0)
                return SetValidationError(error, "collision '" + collision.name + "' requires triangle-list indices");
        }
        if(collision.bodyGroup.empty())
        {
            if(collision.variant != -1)
                return SetValidationError(error, "collision '" + collision.name + "' has a variant without a bodygroup");
        }
        else if(collision.variant < 0)
        {
            return SetValidationError(error, "collision '" + collision.name + "' bodygroup requires a non-negative variant");
        }
        for(const Vec3 &v : collision.vertices)
            if(!IsFinite(v)) return SetValidationError(error, "collision '" + collision.name + "' contains a non-finite vertex");
        for(std::uint32_t index : collision.indices)
            if(index >= collision.vertices.size()) return SetValidationError(error, "collision '" + collision.name + "' has an out-of-range triangle index");
    }

    if(asset.bodies.empty())
    {
        if(!asset.joints.empty()) return SetValidationError(error, "physics asset has joints but no articulated bodies");
        return true;
    }

    std::unordered_map<std::string, int> bodyNames;
    std::unordered_set<std::string> bodyBones;
    std::unordered_set<std::string> propPartNames;
    for(size_t i = 0; i < asset.bodies.size(); ++i)
    {
        const BodyDef &body = asset.bodies[i];
        if(body.propDrive != PropDriveMode::None && body.propDrive != PropDriveMode::Animated &&
           body.propDrive != PropDriveMode::Driven && body.propDrive != PropDriveMode::Free)
            return SetValidationError(error, "invalid prop drive mode");
        if(!body.part.empty() && !propPartNames.insert(Lower(body.part)).second)
            return SetValidationError(error, "duplicate interactive prop part '" + body.part + "'");
        if(body.name.empty() || body.bone.empty())
            return SetValidationError(error, "body requires name and bone");
        const std::string key = Lower(body.name);
        if(bodyNames.find(key) != bodyNames.end())
            return SetValidationError(error, "duplicate body name '" + body.name + "'");
        bodyNames.emplace(key, static_cast<int>(i));
        const std::string boneKey = Lower(body.bone);
        if(!bodyBones.insert(boneKey).second)
            return SetValidationError(error, "multiple bodies reference bone '" + body.bone + "'; one rigid body per bone is supported");

        if(!IsFinite(body.offset) || !IsFinite(body.angles) || !IsFinite(body.mass) ||
           !IsFinite(body.linearDamping) || !IsFinite(body.angularDamping))
            return SetValidationError(error, "body '" + body.name + "' contains non-finite values");
        if(body.mass <= 0.0f)
            return SetValidationError(error, "body '" + body.name + "' mass must be positive");
        if(body.linearDamping < 0.0f || body.angularDamping < 0.0f)
            return SetValidationError(error, "body '" + body.name + "' damping must be non-negative");
        if(asset.version < 4 && (body.propDrive != PropDriveMode::None || !body.part.empty() || body.driveStrength != 0.0f || body.driveDamping != 0.0f || body.driveMaxForce != 0.0f))
            return SetValidationError(error, "interactive prop metadata requires version 4");
        if(!IsFinite(body.driveStrength) || !IsFinite(body.driveDamping) || !IsFinite(body.driveMaxForce) ||
           body.driveStrength < 0.0f || body.driveDamping < 0.0f || body.driveMaxForce < 0.0f)
            return SetValidationError(error, "invalid prop drive parameters");
        if(body.propDrive == PropDriveMode::Driven && body.driveStrength <= 0.0f)
            return SetValidationError(error, "driven body needs positive drive_strength");
        if(body.hitGroup < 0 || body.hitGroup > 255)
            return SetValidationError(error, "body '" + body.name + "' hitgroup must be in the range 0..255");

        switch(body.shape.type)
        {
        case ShapeType::Box:
            if(!IsFinite(body.shape.halfExtents) || body.shape.halfExtents.x <= 0.0f || body.shape.halfExtents.y <= 0.0f || body.shape.halfExtents.z <= 0.0f)
                return SetValidationError(error, "body '" + body.name + "' box half_extents must be finite and positive");
            break;
        case ShapeType::Sphere:
            if(!IsFinite(body.shape.radius) || body.shape.radius <= 0.0f)
                return SetValidationError(error, "body '" + body.name + "' sphere radius must be finite and positive");
            break;
        case ShapeType::Capsule:
            if(!IsFinite(body.shape.radius) || !IsFinite(body.shape.halfHeight) || body.shape.radius <= 0.0f || body.shape.halfHeight < 0.0f)
                return SetValidationError(error, "body '" + body.name + "' capsule radius must be positive and half_height non-negative");
            break;
        default:
            return SetValidationError(error, "body '" + body.name + "' has an invalid shape type");
        }
    }

    std::unordered_set<std::string> jointNames;
    std::unordered_map<int, int> parentOf;
    for(const JointDef &joint : asset.joints)
    {
        if(joint.kind != JointKind::D6 && joint.kind != JointKind::Hinge)
            return SetValidationError(error, "invalid joint kind");
        if(joint.name.empty() || joint.parent.empty() || joint.child.empty())
            return SetValidationError(error, "joint requires name, parent, and child");
        const std::string jointKey = Lower(joint.name);
        if(!jointNames.insert(jointKey).second)
            return SetValidationError(error, "duplicate joint name '" + joint.name + "'");

        const auto parentIt = bodyNames.find(Lower(joint.parent));
        const auto childIt = bodyNames.find(Lower(joint.child));
        if(parentIt == bodyNames.end() || childIt == bodyNames.end())
            return SetValidationError(error, "joint '" + joint.name + "' references an unknown body");
        if(parentIt->second == childIt->second)
            return SetValidationError(error, "joint '" + joint.name + "' cannot connect a body to itself");

        if(asset.version < 4 && (joint.kind != JointKind::D6 || joint.breakImpulse != 0.0f || joint.releaseDamage != 0.0f || joint.breakDamage != 0.0f || joint.hingeMin != 0.0f || joint.hingeMax != 90.0f ||
            joint.hingeAnchor.x != 0.0f || joint.hingeAnchor.y != 0.0f || joint.hingeAnchor.z != 0.0f ||
            joint.hingeAxis.x != 1.0f || joint.hingeAxis.y != 0.0f || joint.hingeAxis.z != 0.0f))
            return SetValidationError(error, "interactive joint metadata requires version 4");
        if(!IsFinite(joint.hingeMin) || !IsFinite(joint.hingeMax) ||
           !IsFinite(joint.breakImpulse) || joint.breakImpulse < 0.0f ||
           !IsFinite(joint.releaseDamage) || joint.releaseDamage < 0.0f ||
           !IsFinite(joint.breakDamage) || joint.breakDamage < 0.0f)
            return SetValidationError(error, "invalid hinge limits, release damage or break thresholds");
        if(joint.kind != JointKind::Hinge && (joint.releaseDamage > 0.0f || joint.breakDamage > 0.0f))
            return SetValidationError(error, "release_damage and break_damage require a hinge joint");
        if(!IsFinite(joint.hingeAnchor) || !IsFinite(joint.hingeAxis))
            return SetValidationError(error, "hinge anchor/axis must be finite");
        const double hingeAxisLengthSquared = double(joint.hingeAxis.x) * joint.hingeAxis.x +
            double(joint.hingeAxis.y) * joint.hingeAxis.y + double(joint.hingeAxis.z) * joint.hingeAxis.z;
        if(joint.kind == JointKind::Hinge && !(hingeAxisLengthSquared > 1e-12 && hingeAxisLengthSquared < 1e12))
            return SetValidationError(error, "hinge axis must be a non-zero finite vector");
        if(joint.kind == JointKind::Hinge && (joint.hingeMin < -180.0f || joint.hingeMax > 180.0f || joint.hingeMin >= joint.hingeMax))
            return SetValidationError(error, "hinge limits must satisfy -180 <= min < max <= 180");
        if(!IsFinite(joint.swing1) || !IsFinite(joint.swing2) || !IsFinite(joint.twistLow) || !IsFinite(joint.twistHigh))
            return SetValidationError(error, "joint '" + joint.name + "' contains non-finite limits");
        if(joint.swing1 <= 0.0f || joint.swing1 >= 180.0f || joint.swing2 <= 0.0f || joint.swing2 >= 180.0f)
            return SetValidationError(error, "joint '" + joint.name + "' swing limits must be > 0 and < 180 degrees");
        if(joint.twistLow < -180.0f || joint.twistHigh > 180.0f || joint.twistLow >= joint.twistHigh)
            return SetValidationError(error, "joint '" + joint.name + "' twist limits must satisfy -180 <= low < high <= 180");

        if(parentOf.find(childIt->second) != parentOf.end())
            return SetValidationError(error, "body '" + joint.child + "' has more than one parent joint");
        parentOf.emplace(childIt->second, parentIt->second);
    }

    size_t rootCount = 0;
    for(size_t i = 0; i < asset.bodies.size(); ++i)
        if(parentOf.find(static_cast<int>(i)) == parentOf.end()) ++rootCount;
    if(rootCount != 1)
        return SetValidationError(error, "articulated physics bodies must contain exactly one root body");

    for(size_t start = 0; start < asset.bodies.size(); ++start)
    {
        std::unordered_set<int> visited;
        int body = static_cast<int>(start);
        while(true)
        {
            const auto it = parentOf.find(body);
            if(it == parentOf.end()) break;
            if(!visited.insert(body).second)
                return SetValidationError(error, "physics asset joint graph contains a cycle");
            body = it->second;
        }
    }

    return true;
}

bool Parse(std::string_view text, AssetDef &out, std::string *error)
{
    if (!Parser(text).Run(out, error)) return false;
    // FNV-1a over bytes, including version and all authoring selections.
    // Changes to .phys must not reuse MDL-only collision cache entries.
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (unsigned char byte : text) { hash ^= byte; hash *= UINT64_C(1099511628211); }
    out.collisionFingerprint = hash ? hash : 1;
    return true;
}

std::string Serialize(const AssetDef &asset)
{
    std::ostringstream ss;
    // max_digits10 guarantees a float survives .phys Save -> Load exactly.
    // Six significant digits rounded low-amplitude hinge and collision data.
    ss << std::setprecision(std::numeric_limits<float>::max_digits10) << std::defaultfloat;
    ss << "physics_asset\n{\n";
    int writeVersion = asset.version;
    if(asset.collisions.empty() && asset.version == kLegacyVersion) writeVersion = kLegacyVersion;
    else if(writeVersion < kCollisionVersion) writeVersion = kCollisionVersion;
    const bool needsV3 = std::any_of(asset.collisions.begin(), asset.collisions.end(), [](const CollisionDef &c){ return !c.bone.empty(); });
    if(needsV3 && writeVersion < kBoneCollisionVersion) writeVersion = kBoneCollisionVersion;
    ss << "    version " << writeVersion << "\n\n";
    for(const CollisionDef &collision : asset.collisions)
    {
        ss << "    collision " << Quote(collision.name) << "\n    {\n";
        ss << "        type " << CollisionGeometryTypeName(collision.type) << "\n";
        if(!collision.bodyGroup.empty())
        {
            ss << "        bodygroup " << Quote(collision.bodyGroup) << "\n";
            ss << "        variant " << collision.variant << "\n";
        }
        if(!collision.bone.empty()) ss << "        bone " << Quote(collision.bone) << "\n";
        if(collision.disabled) ss << "        disabled 1\n";
        for(const Vec3 &vertex : collision.vertices)
        {
            ss << "        vertex "; WriteVec3(ss, vertex); ss << "\n";
        }
        for(size_t i = 0; i + 2 < collision.indices.size(); i += 3)
            ss << "        triangle " << collision.indices[i] << ' ' << collision.indices[i + 1] << ' ' << collision.indices[i + 2] << "\n";
        ss << "    }\n\n";
    }
    for(const BodyDef &body : asset.bodies)
    {
        ss << "    body " << Quote(body.name) << "\n    {\n";
        ss << "        bone " << Quote(body.bone) << "\n";
        ss << "        shape " << ShapeTypeName(body.shape.type) << "\n";
        if(body.shape.type == ShapeType::Box)
        {
            ss << "        half_extents "; WriteVec3(ss, body.shape.halfExtents); ss << "\n";
        }
        else if(body.shape.type == ShapeType::Sphere)
        {
            ss << "        radius " << body.shape.radius << "\n";
        }
        else
        {
            ss << "        radius " << body.shape.radius << "\n";
            ss << "        half_height " << body.shape.halfHeight << "\n";
        }
        ss << "        offset "; WriteVec3(ss, body.offset); ss << "\n";
        ss << "        angles "; WriteVec3(ss, body.angles); ss << "\n";
        ss << "        mass " << body.mass << "\n";
        ss << "        linear_damping " << body.linearDamping << "\n";
        ss << "        angular_damping " << body.angularDamping << "\n";
        ss << "        hitgroup " << body.hitGroup << "\n";
        if(!body.part.empty()) ss << "        part " << Quote(body.part) << "\n";
        if(body.propDrive != PropDriveMode::None)
        {
            const char *mode = body.propDrive == PropDriveMode::Animated ? "animated" : body.propDrive == PropDriveMode::Driven ? "driven" : "free";
            ss << "        prop_drive " << mode << "\n";
        }
        if(body.driveStrength != 0.0f) ss << "        drive_strength " << body.driveStrength << "\n";
        if(body.driveDamping != 0.0f) ss << "        drive_damping " << body.driveDamping << "\n";
        if(body.driveMaxForce != 0.0f) ss << "        drive_max_force " << body.driveMaxForce << "\n";
        ss << "    }\n\n";
    }
    for(const JointDef &joint : asset.joints)
    {
        ss << "    joint " << Quote(joint.name) << "\n    {\n";
        ss << "        parent " << Quote(joint.parent) << "\n";
        ss << "        child " << Quote(joint.child) << "\n";
        if(joint.kind == JointKind::Hinge)
        {
            ss << "        kind hinge\n";
            ss << "        hinge_min " << joint.hingeMin << "\n";
            ss << "        hinge_max " << joint.hingeMax << "\n";
            ss << "        hinge_anchor " << joint.hingeAnchor.x << " " << joint.hingeAnchor.y << " " << joint.hingeAnchor.z << "\n";
            ss << "        hinge_axis " << joint.hingeAxis.x << " " << joint.hingeAxis.y << " " << joint.hingeAxis.z << "\n";
        }
        if(joint.breakImpulse > 0.0f) ss << "        break_impulse " << joint.breakImpulse << "\n";
        if(joint.releaseDamage > 0.0f) ss << "        release_damage " << joint.releaseDamage << "\n";
        if(joint.breakDamage > 0.0f) ss << "        break_damage " << joint.breakDamage << "\n";
        ss << "        swing1 " << joint.swing1 << "\n";
        ss << "        swing2 " << joint.swing2 << "\n";
        ss << "        twist_low " << joint.twistLow << "\n";
        ss << "        twist_high " << joint.twistHigh << "\n";
        ss << "    }\n\n";
    }
    ss << "}\n";
    return ss.str();
}

bool LoadFile(const char *path, AssetDef &out, std::string *error)
{
    if(!path || !path[0])
    {
        if(error) *error = "empty physics asset path";
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    if(!stream)
    {
        if(error) *error = std::string("cannot open ") + path;
        return false;
    }
    std::ostringstream ss;
    ss << stream.rdbuf();
    if(!Parse(ss.str(), out, error)) return false;
    out.sourcePath = path;
    return true;
}

bool SaveFile(const char *path, const AssetDef &asset, std::string *error)
{
    if(!path || !path[0])
    {
        if(error) *error = "empty physics asset path";
        return false;
    }
    if(!Validate(asset, error)) return false;
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if(!stream)
    {
        if(error) *error = std::string("cannot write ") + path;
        return false;
    }
    const std::string text = Serialize(asset);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    if(!stream.good())
    {
        if(error) *error = std::string("write failed for ") + path;
        return false;
    }
    return true;
}

int FindBody(const AssetDef &asset, const char *name)
{
    if(!name) return -1;
    for(size_t i = 0; i < asset.bodies.size(); ++i)
    {
#ifdef _WIN32
        if(_stricmp(asset.bodies[i].name.c_str(), name) == 0) return static_cast<int>(i);
#else
        if(strcasecmp(asset.bodies[i].name.c_str(), name) == 0) return static_cast<int>(i);
#endif
    }
    return -1;
}

const char *ShapeTypeName(ShapeType type)
{
    switch(type)
    {
    case ShapeType::Box: return "box";
    case ShapeType::Sphere: return "sphere";
    case ShapeType::Capsule: return "capsule";
    }
    return "capsule";
}

bool ParseShapeType(const char *name, ShapeType &type)
{
    if(!name) return false;
    std::string value(name);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    if(value == "box") { type = ShapeType::Box; return true; }
    if(value == "sphere") { type = ShapeType::Sphere; return true; }
    if(value == "capsule") { type = ShapeType::Capsule; return true; }
    return false;
}
const char *CollisionGeometryTypeName(CollisionGeometryType type)
{
    switch(type)
    {
    case CollisionGeometryType::TriangleMesh: return "triangle_mesh";
    case CollisionGeometryType::Convex: return "convex";
    }
    return "triangle_mesh";
}

bool ParseCollisionGeometryType(const char *name, CollisionGeometryType &type)
{
    if(!name) return false;
    std::string value(name);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    if(value == "triangle_mesh" || value == "mesh") { type = CollisionGeometryType::TriangleMesh; return true; }
    if(value == "convex") { type = CollisionGeometryType::Convex; return true; }
    return false;
}

bool HasCollisionGeometry(const AssetDef &asset)
{
    return !asset.collisions.empty();
}

bool HasArticulatedBodies(const AssetDef &asset)
{
    return !asset.bodies.empty();
}

}
