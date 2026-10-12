// J.8 - central, server-authoritative ballistic manager. No per-bullet edicts.
// Free flight is precomputed; collision is evaluated when the shot reaches
// each segment, so moving characters, ragdolls, and props remain authoritative.
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "game.h"
#include "modular_ballistics.h"
#include "cartridge_damage.h"
#include "modular_damage_scope.h"
#include "ballistic_flight.h"
#include "ballistic_penetration.h"
#include "ballistic_path_cache.h"
#include "ballistic_timing.h"
#include "modular_ballistic_exit.h"
#include "ballistic_world_query.h"
#include "seeded_random_generator.h"
#include "material.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cctype>
#include <utility>
#include <string>
#include <vector>

namespace {
constexpr double kUnitsPerMeter = 39.37007874015748;
constexpr double kCurveStep = 0.01; // calculated once per uninterrupted flight
constexpr double kSweepStep = 0.02; // at most 20ms of trajectory per collision sweep
constexpr int kMaxActive = 256;
constexpr int kCurveSamples = 1002;
constexpr int kMaxPenetrations = 4;
constexpr int kShotDiskVersion = 3; // versioned compact in-flight + transit save record
constexpr double kMaxLifetime = 10.0;
constexpr float kMaxReverseDepthUnits = 96.0f;
constexpr float kSurfaceOffsetUnits = 0.5f;

Vector GamePoint(modular::BallisticVec3 p) {
    return Vector(float(p.x * kUnitsPerMeter),float(p.y * kUnitsPerMeter),float(p.z * kUnitsPerMeter));
}
modular::BallisticVec3 SI(Vector p) {
    return {p.x / kUnitsPerMeter,p.y / kUnitsPerMeter,p.z / kUnitsPerMeter};
}
modular::BallisticVec3 Mix(modular::BallisticVec3 a, modular::BallisticVec3 b, double t) {
    return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};
}
modular::BallisticState Blend(const modular::BallisticState& a,
                               const modular::BallisticState& b,double t) {
    modular::BallisticState s;
    s.position=Mix(a.position,b.position,t);
    s.velocity=Mix(a.velocity,b.velocity,t);
    s.seconds=a.seconds+(b.seconds-a.seconds)*t;
    s.meters=a.meters+(b.meters-a.meters)*t;
    return s;
}
bool Finite(modular::BallisticVec3 p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
}
bool ValidState(const modular::BallisticState& s,const modular::BallisticParameters& p,
                double maxMeters,double initialEnergy) {
    return Finite(s.position) && Finite(s.velocity) && std::isfinite(s.seconds) &&
        s.seconds >= 0 && s.seconds <= kMaxLifetime && std::isfinite(s.meters) &&
        s.meters >= 0 && s.meters <= maxMeters &&
        std::isfinite(p.massKg) && p.massKg > 0 &&
        std::isfinite(p.diameterM) && p.diameterM > 0 &&
        std::isfinite(p.dragCoefficient) && p.dragCoefficient >= 0 &&
        std::isfinite(p.gravity) && p.gravity >= 0 &&
        std::isfinite(p.airDensity) && p.airDensity >= 0 &&
        std::isfinite(maxMeters) && maxMeters > 0 &&
        std::isfinite(initialEnergy) && initialEnergy > 0 &&
        modular::PlausibleBallisticEnergy(s,p,initialEnergy);
}

// These are gameplay-oriented fallbacks: material data currently has no
// penetration resistance property. Unknown materials deliberately resist more.
struct PenetrationMaterial { double energyPerMeter; double minimumSpeed; };
PenetrationMaterial ResistanceForMaterialName(std::string name,
    PenetrationMaterial fallback = {90000.0,600.0}) {
    std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){ return char(std::tolower(c)); });
    if (name.find("glass") != std::string::npos) return {1500.0,60.0};
    if (name.find("wood") != std::string::npos) return {4000.0,90.0};
    if (name.find("flesh") != std::string::npos || name.find("body") != std::string::npos) return {1500.0,80.0};
    if (name.find("metal") != std::string::npos || name.find("steel") != std::string::npos) return {60000.0,400.0};
    if (name.find("stone") != std::string::npos || name.find("concrete") != std::string::npos) return {90000.0,600.0};
    return fallback;
}
PenetrationMaterial MaterialResistance(const TraceResult& tr,const Vector& from,const Vector& to) {
    std::string name;
    if (tr.materialHash) {
        if (const auto* material = COM_FindMaterial(tr.materialHash)) name = material->name;
    }
    if (name.empty() && UTIL_IsValidEntity(tr.pHit)) {
        if (const auto* hit = CBaseEntity::Instance(tr.pHit)) {
            if (UTIL_GetModelType(hit->pev->modelindex) == mod_brush) {
                if (auto* face = TRACE_SURFACE(tr.pHit,from,to)) {
                    if (auto* mat = COM_MatDefFromSurface(face,tr.vecEndPos)) name=mat->name;
                }
            }
        }
    }
    PenetrationMaterial fallback{90000.0,600.0};
    if (UTIL_IsValidEntity(tr.pHit)) {
        if (auto* target=CBaseEntity::Instance(tr.pHit)) {
            if (target->pev->flags & (FL_CLIENT | FL_MONSTER)) fallback={1500.0,80.0};
            else if (UTIL_GetModelType(target->pev->modelindex)==mod_studio) fallback={8000.0,120.0};
        }
    }
    return ResistanceForMaterialName(std::move(name),fallback);
}

struct Shot {
    EHANDLE owner;
    modular::BallisticState state;
    modular::BallisticParameters physics;
    std::vector<modular::BallisticState> curve;
    double progress = 0; // progress within current cached trajectory
    double lastSimulatedTime = 0; // per-shot clock: no pre-birth movement
    modular::BallisticTransit transit; // pending material travel across frames
    double originalEnergy = 0, maxMeters = 0;
    float damage = 0, tracerTime = 0;
    float damageCoefficient = 0;
    bool v4 = false; // restored from the fingerprint-matched weapon definition
    modular::DamageLogic damageLogic = modular::DamageLogic::ConfigurableBase;
    std::string weaponId, killSprite;
    Vector tracerPosition;
    int bulletType = 0, penetrations = 0;
    double projectileFactor = 1;
};

// Only scalar state is serialized; trajectory samples are rebuilt on restore.
// Owner EHANDLEs are saved separately with FIELD_EHANDLE so references rebase.
struct ShotDisk {
    float position[3], velocity[3], seconds, meters;
    float mass, diameter, drag, gravity, airDensity;
    float originalEnergy, maxMeters, damage, projectileFactor;
    int bulletType, penetrations;
    // J.10.1: keep in-material travel when saving between entry and exit.
    float transitEntryPosition[3], transitEntryVelocity[3];
    float transitExitPosition[3], transitExitVelocity[3];
    float transitEntrySeconds, transitEntryMeters;
    float transitDistance, transitDuration, transitElapsed;
    int transitActive;
};
static_assert(sizeof(ShotDisk)*kMaxActive < 60000,"snapshot must fit datamap field size");

// Calculate the whole uninterrupted free-flight curve once at firing or
// after an impact changes its velocity, using a portable shared implementation.
bool BuildCurve(Shot& shot) {
    if (!ValidState(shot.state,shot.physics,shot.maxMeters,shot.originalEnergy)) return false;
    shot.progress=0;
    return modular::PrecomputeBallisticCurve(shot.curve,shot.state,shot.physics,
                                             shot.maxMeters,kCurveStep,kCurveSamples);
}
modular::BallisticState At(const Shot& shot,double offset) {
    return modular::SampleBallisticCurve(shot.curve,offset,kCurveStep);
}
// Retain I.10.10's material/physics flags and ragdoll nearest-hit selection.
int BulletTrace(Vector from,Vector to,edict_t* ignore,TraceResult& trace) {
    const int previousTraceFlags = gpGlobals->trace_flags;
    SetBits(gpGlobals->trace_flags,FTRACE_MATERIAL_TRACE | FTRACE_BULLET_PHYSICS);
    UTIL_TraceLine(from,to,dont_ignore_monsters,ignore,&trace);
    gpGlobals->trace_flags = previousTraceFlags;
    return ResolveRagdollBulletCollision(&trace,from,to,ignore);
}
void Tracer(Shot& shot,const Vector& to) {
    if (CVAR_GET_FLOAT("sv_ballistics_tracers") > 0 &&
        gpGlobals->time - shot.tracerTime >= 0.05f) {
        MESSAGE_BEGIN(MSG_PVS,SVC_TEMPENTITY,shot.tracerPosition);
        WRITE_BYTE(TE_TRACER);
        WRITE_COORD(shot.tracerPosition.x); WRITE_COORD(shot.tracerPosition.y);
        WRITE_COORD(shot.tracerPosition.z);
        WRITE_COORD(to.x); WRITE_COORD(to.y); WRITE_COORD(to.z);
        MESSAGE_END();
        shot.tracerTime = gpGlobals->time;
        shot.tracerPosition = to;
    }
}
void Impact(Shot& shot,const TraceResult& tr,int ragdollBody,Vector from,Vector to,
            const modular::BallisticState& atImpact) {
    TraceResult effects{};
    ResolveBulletImpactEffects(&tr,from,to,&effects);
    if (CVAR_GET_FLOAT("sv_ballistics_debug") > 0)
        ALERT(at_console,"J.8 impact: %.3fs %.1fm %.1fJ\n",atImpact.seconds,atImpact.meters,
            modular::BallisticEnergy(atImpact,shot.physics.massKg));
    const Vector direction=(to-from).Normalize();
    CBaseEntity* owner=shot.owner;
    if (owner && owner->pev && UTIL_IsValidEntity(tr.pHit)) {
        if (CBaseEntity* target=CBaseEntity::Instance(tr.pHit)) {
            const double impactEnergy=modular::BallisticEnergy(atImpact,shot.physics.massKg);
            const float damage=float(shot.v4 ? modular::ImpactDamage(
                shot.damageLogic, impactEnergy, shot.originalEnergy,
                shot.physics.diameterM * 1000.0, shot.damageCoefficient, shot.damage) :
                modular::ImpactDamage(impactEnergy, shot.originalEnergy,
                shot.physics.diameterM * 1000.0, shot.damageCoefficient, shot.damage));
            if (!std::isfinite(damage) || damage < 0) return;
            ScopedModularDamage damageSource({owner->pev,
                modular::SharedRegistry().FindWeapon(shot.weaponId), shot.killSprite});
            ClearMultiDamage();
            gMultiDamage.type=DMG_BULLET;
            ApplyPhysicsBulletImpulse(&tr,ragdollBody,from,to,direction,damage);
            target->TraceAttack(owner->pev,damage,direction,const_cast<TraceResult*>(&tr),DMG_BULLET);
            ApplyMultiDamage(owner->pev,owner->pev);
            ClearMultiDamage(); // delayed projectile must not leak stale global damage
        }
    }
    TEXTURETYPE_PlaySound(&effects,from,to,shot.bulletType);
    DecalGunshot(&effects,shot.bulletType,from,to);
}

// Capture penetration geometry BEFORE TraceAttack: lethal hits may synchronously
// replace a living hitbox with a ragdoll (or detach a physical part).
struct PenetrationCandidate {
    Vector exitPoint;
    double thicknessMeters=0;
    double exitSpeed=0;
    double incomingSpeed=0;
    float postExitOffsetUnits=kSurfaceOffsetUnits;
    float entryLeadUnits=0;
};
bool PreparePenetration(Shot& shot,const TraceResult& entry,int bodyIndex,
    const Vector& incoming,const Vector& traceStart,
    const modular::BallisticState& contact,PenetrationCandidate& out) {
    if (CVAR_GET_FLOAT("sv_ballistics_penetration") <= 0 ||
        shot.penetrations >= kMaxPenetrations || !shot.owner ||
        !shot.owner->pev || !UTIL_IsValidEntity(entry.pHit)) return false;
    auto* target=CBaseEntity::Instance(entry.pHit);
    if (!target) return false;
    const Vector forward=incoming.Normalize();
    if (forward == g_vecZero || entry.fStartSolid || entry.fAllSolid) return false;
    const double angle=std::clamp(double(-DotProduct(forward,entry.vecPlaneNormal)),0.0,1.0);
    if (angle < 0.2) return false;
    Vector exitPoint;
    float thicknessGame=0;
    float minimumThickness=kSurfaceOffsetUnits;
    auto material=MaterialResistance(entry,entry.vecEndPos-incoming,entry.vecEndPos);
    if (UTIL_GetModelType(target->pev->modelindex)==mod_brush &&
        target->pev->solid != SOLID_CUSTOM && target->entindex()==0) {
        // J.10.6.1: the engine owns exact BSP intervals and native hull layouts.
        // Old engines remain usable, but cannot certify world penetration.
        if (!g_physfuncs.pfnGetNativeObject) return false;
        const auto* api=static_cast<const world_ballistic_query_api_t*>(
            g_physfuncs.pfnGetNativeObject(WORLD_BALLISTIC_QUERY_OBJECT));
        if (!modular::ValidWorldBallisticApi(api)) return false;
        world_ballistic_request_t request{};
        request.struct_size=sizeof(request);
        for (int i=0;i<3;++i) {
            request.start[i]=traceStart[i];
            request.direction[i]=forward[i];
        }
        request.contact_distance=(entry.vecEndPos-traceStart).Length();
        request.maximum_depth=kMaxReverseDepthUnits;
        request.preferred_offset=kSurfaceOffsetUnits;
        world_ballistic_result_t interval{};
        interval.struct_size=sizeof(interval);
        modular::WorldBallisticGeometry geometry{};
        if (!api->query(&request,&interval) ||
            !modular::ValidateWorldBallisticGeometry(request,interval,geometry)) return false;
        const Vector entryPoint(interval.entry_position);
        exitPoint=Vector(interval.exit_position);
        if (DotProduct(Vector(interval.entry_normal),entry.vecPlaneNormal)<0.9f) return false;
        // BSP materials are resolved at the exact exposed faces. World
        // materialHash aliases hitgroup and is not a material identity.
        const auto* entryMaterial=COM_MatDefFromSurface(interval.entry_surface,entryPoint);
        const auto* exitMaterial=COM_MatDefFromSurface(interval.exit_surface,exitPoint);
        if (!entryMaterial || !exitMaterial || entryMaterial==COM_DefaultMatdef() ||
            exitMaterial==COM_DefaultMatdef() ||
            !std::memchr(entryMaterial->name,0,sizeof(entryMaterial->name)) ||
            !std::memchr(exitMaterial->name,0,sizeof(exitMaterial->name)) ||
            !modular::SameWorldPenetrationMaterial(entryMaterial->name,exitMaterial->name))
            return false;
        material=ResistanceForMaterialName(entryMaterial->name);
        thicknessGame=geometry.thicknessUnits;
        minimumThickness=0.001f;
        out.postExitOffsetUnits=geometry.postExitOffsetUnits;
        out.entryLeadUnits=geometry.entryLeadUnits;
        const Vector continuation=exitPoint+forward*out.postExitOffsetUnits;
        // The complete interval is certified by the engine. This endpoint
        // guard additionally rejects offsets lost to float-coordinate rounding.
        if ((continuation-exitPoint).Length()<out.postExitOffsetUnits*0.5f ||
            UTIL_PointContents(continuation)==CONTENTS_SOLID) return false;
    } else {
        BallisticExitCandidate exit{};
        if (!FindModularBallisticExit(target,entry,bodyIndex,forward,
                                      kMaxReverseDepthUnits,exit)) return false;
        exitPoint=exit.position;
        thicknessGame=exit.thicknessUnits;
    }
    if (!std::isfinite(thicknessGame) || thicknessGame<=minimumThickness ||
        thicknessGame>kMaxReverseDepthUnits) return false;
    const double speed=std::sqrt(2.0*modular::BallisticEnergy(contact,shot.physics.massKg)/shot.physics.massKg);
    const auto result=modular::ComputePenetration({shot.physics.massKg,speed,
        thicknessGame/kUnitsPerMeter,material.energyPerMeter,
        angle,shot.projectileFactor,material.minimumSpeed});
    if (!result.penetrated) return false;
    out.exitPoint=exitPoint;
    out.thicknessMeters=thicknessGame/kUnitsPerMeter;
    out.exitSpeed=result.exitSpeedMps;
    out.incomingSpeed=speed;
    return true;
}
bool BeginPenetration(Shot& shot,const modular::BallisticState& contact,
    const Vector& incoming,const PenetrationCandidate& candidate) {
    const Vector forward=incoming.Normalize();
    if (forward == g_vecZero || !(candidate.exitSpeed>0) ||
        !(candidate.incomingSpeed>candidate.exitSpeed) ||
        !(candidate.thicknessMeters>0) ||
        !std::isfinite(candidate.postExitOffsetUnits) || candidate.postExitOffsetUnits<=0 ||
        candidate.postExitOffsetUnits>kSurfaceOffsetUnits ||
        !std::isfinite(candidate.entryLeadUnits) || candidate.entryLeadUnits<0 ||
        candidate.entryLeadUnits>0.25f) return false;
    const double ratio=candidate.exitSpeed/candidate.incomingSpeed;
    modular::BallisticTransit next{};
    next.entry=contact;
    next.exitPosition=SI(candidate.exitPoint+forward*candidate.postExitOffsetUnits);
    next.exitVelocity={contact.velocity.x*ratio,contact.velocity.y*ratio,contact.velocity.z*ratio};
    next.distanceMeters=(GamePoint(next.exitPosition)-GamePoint(contact.position)).Length()/kUnitsPerMeter;
    const double offsetMeters=candidate.postExitOffsetUnits/kUnitsPerMeter;
    const double entryLeadMeters=candidate.entryLeadUnits/kUnitsPerMeter;
    next.durationSeconds=entryLeadMeters/candidate.incomingSpeed+candidate.thicknessMeters/
        std::max(1.0,0.5*(candidate.incomingSpeed+candidate.exitSpeed))+
        offsetMeters/std::max(1.0,candidate.exitSpeed);
    next.elapsedSeconds=0;
    next.active=true;
    if (!modular::ValidTransit(next) || !std::isfinite(next.distanceMeters) ||
        next.distanceMeters<candidate.thicknessMeters*0.9 ||
        next.durationSeconds>kMaxLifetime) return false;
    shot.state=contact;
    shot.transit=next;
    ++shot.penetrations;
    // Keep the old curve dormant until the actual exit time is reached.
    // An intermediate save preserves this pending traversal rather than
    // teleporting the projectile beyond a barrier.
    return true;
}

// Invoked once per shot from one manager, never one Think per projectile.
// Reports consumed simulation time so a bounded sweep cannot silently eat
// an unprocessed part of the shot's time budget.
bool AdvanceShot(Shot& shot,double budget,double& consumed) {
    consumed=0;
    if (!shot.owner || !shot.owner->pev) return false;
    double dt=std::clamp(budget,0.0,0.10);
    for (int sweeps=0;sweeps<24 && dt>1e-9;++sweeps) {
        if (shot.state.seconds>=kMaxLifetime-1e-9 ||
            shot.state.meters>=shot.maxMeters-1e-9) return false;
        if (shot.transit.active) {
            if (!modular::ValidTransit(shot.transit)) return false;
            const double used=modular::AdvanceTransit(shot.state,shot.transit,dt,
                shot.maxMeters,kMaxLifetime);
            consumed+=used;
            dt-=used;
            if (shot.state.seconds>=kMaxLifetime-1e-9 ||
                shot.state.meters>=shot.maxMeters-1e-9) return false;
            if (used<=1e-10) return false;
            if (shot.transit.active) continue; // still inside material
            if (!BuildCurve(shot)) return false;
            Tracer(shot,GamePoint(shot.state.position));
            continue;
        }
        if (shot.curve.size()<2) return false;
        const double endOffset=shot.curve.back().seconds-shot.curve.front().seconds;
        const double available=std::max(0.0,endOffset-shot.progress);
        if (available<=1e-10) return false;
        const double step=std::min({dt,kSweepStep,available,
                                    kMaxLifetime-shot.state.seconds});
        if (step<=1e-10) return false;
        const auto fromState=At(shot,shot.progress);
        const auto toState=At(shot,shot.progress+step);
        // Defensive range limit even if a malformed cache were supplied.
        if (toState.meters>shot.maxMeters+1e-7) return false;
        const Vector from=GamePoint(fromState.position),to=GamePoint(toState.position);
        TraceResult tr{};
        const int body=BulletTrace(from,to,shot.owner->edict(),tr);
        if (tr.flFraction<1.f) {
            const double f=std::clamp(double(tr.flFraction),0.0,1.0);
            auto contact=Blend(fromState,toState,f);
            contact.position=SI(tr.vecEndPos);
            PenetrationCandidate penetration{};
            // Query original shape before hit damage can mutate it.
            const bool canPenetrate=PreparePenetration(shot,tr,body,to-from,from,contact,penetration);
            consumed+=step*f;
            dt-=step*f;
            shot.state=contact;
            Impact(shot,tr,body,from,to,contact);
            if (canPenetrate && BeginPenetration(shot,contact,to-from,penetration)) {
                // No early exit teleport: the next loop consumes traversal
                // time and later frames resume from the recorded progress.
                continue;
            }
            return false;
        }
        shot.progress+=step;
        shot.state=toState;
        consumed+=step;
        dt-=step;
        Tracer(shot,to);
        if (shot.state.seconds>=kMaxLifetime-1e-9 ||
            shot.state.meters>=shot.maxMeters-1e-9 ||
            modular::BallisticEnergy(shot.state,shot.physics.massKg)<0.1) return false;
    }
    return true;
}

} // namespace

class CModularBallisticManager : public CBaseEntity {
    DECLARE_CLASS(CModularBallisticManager,CBaseEntity);
public:
    DECLARE_DATADESC();
    ~CModularBallisticManager() override;
    void Spawn() override;
    int Save(CSave& save) override;
    int Restore(CRestore& restore) override;
    void Update();
    bool Add(CBaseEntity* shooter,const modular::Weapon& weapon,const modular::Cartridge& cartridge,
             Vector at,Vector dir,float speed, const modular::AssetSelection* assets = nullptr);
    int Count() const { return int(m_shots.size()); }
    void RollbackTo(int count) { if (count>=0 && count<=int(m_shots.size())) m_shots.resize(count); }
private:
    // FIELD_EHANDLE handles game save entity remapping; other POD fields use
    // a compact byte array. Full cached curves never enter the savegame.
    EHANDLE m_savedOwners[kMaxActive];
    char m_savedRecords[sizeof(ShotDisk)*kMaxActive]{};
    // Keep the version-2 byte stride; newly added save fields are separate.
    float m_savedDamageCoefficients[kMaxActive]{};
    string_t m_savedWeaponIds[kMaxActive]{}, m_savedKillSprites[kMaxActive]{};
    int m_savedCount=0,m_savedFingerprint=0,m_savedRecordVersion=0;
    float m_last=0;
    std::vector<Shot> m_shots;
};

namespace { CModularBallisticManager* g_manager=nullptr; }
CModularBallisticManager::~CModularBallisticManager() {
    if (g_manager==this) g_manager=nullptr;
}
void CModularBallisticManager::Spawn() {
    pev->solid=SOLID_NOT;
    pev->movetype=MOVETYPE_NONE;
    pev->effects|=EF_NODRAW;
    // TraceAttack/ApplyMultiDamage can synchronously cause another weapon to fire.
    // Reserve the full bounded shot capacity before any in-flight callbacks so
    // Add() cannot reallocate m_shots while AdvanceShot holds a Shot&.
    m_shots.reserve(kMaxActive);
    g_manager=this;
    m_last=gpGlobals->time;
    // The manager is advanced from EndFrame; it never schedules Think.
    pev->nextthink=0;
}
bool CModularBallisticManager::Add(CBaseEntity* owner,const modular::Weapon& weapon,
    const modular::Cartridge& cartridge,Vector at,Vector dir,float speed,
    const modular::AssetSelection* assets) {
    if (!owner || !owner->pev || int(m_shots.size())>=kMaxActive) return false;
    Shot shot;
    shot.owner=owner;
    shot.physics.massKg=cartridge.massGrams*0.001;
    shot.physics.diameterM=cartridge.diameterMm*0.001;
    shot.physics.dragCoefficient=cartridge.dragCoefficient;
    const double gravityScale=CVAR_GET_FLOAT("sv_ballistics_gravity_scale");
    const double density=CVAR_GET_FLOAT("sv_ballistics_air_density");
    shot.physics.gravity=9.80665*(std::isfinite(gravityScale)?std::clamp(gravityScale,0.0,10.0):1.0);
    shot.physics.airDensity=std::isfinite(density)?std::clamp(density,0.0,10.0):1.225;
    shot.state.position=SI(at);
    shot.state.velocity={dir.x*speed,dir.y*speed,dir.z*speed};
    shot.maxMeters=weapon.range/kUnitsPerMeter;
    shot.originalEnergy=modular::BallisticEnergy(shot.state,shot.physics.massKg);
    shot.damage=float(modular::MuzzleDamage(cartridge, speed));
    shot.v4=cartridge.schemaVersion >= 4;
    if (shot.v4) shot.damageLogic=cartridge.damageLogic;
    if (cartridge.schemaVersion >= 4 &&
        (cartridge.attackLogic != modular::AttackLogic::Ballistic ||
         cartridge.damageLogic == modular::DamageLogic::Legacy)) return false;
    shot.damageCoefficient = cartridge.schemaVersion >= 4 ?
        (cartridge.damageLogic == modular::DamageLogic::EnergyBased ? cartridge.damageCoefficient : 0.f) :
        (cartridge.schemaVersion >= 3 ? cartridge.damageCoefficient : 0.f);
    if (!std::isfinite(shot.damage) || shot.damage <= 0 || shot.damage > 1000 ||
        !std::isfinite(shot.damageCoefficient) || shot.damageCoefficient < 0 ||
        shot.damageCoefficient > 1e6f ||
        (cartridge.schemaVersion >= 4 && cartridge.damageLogic == modular::DamageLogic::EnergyBased &&
         shot.damageCoefficient <= 0)) return false;
    shot.weaponId=weapon.id;
    const modular::AssetSelection fallback{cartridge.id, weapon.primary.mode, 0, modular::AssetAction::Primary};
    shot.killSprite=modular::SelectAsset(weapon, assets ? *assets : fallback, "kill_sprite");
    shot.bulletType=weapon.bulletType;
    shot.projectileFactor=cartridge.projectileType=="ap"?0.7:
        (cartridge.projectileType=="jhp"?1.4:1.0);
    shot.tracerTime=gpGlobals->time;
    shot.tracerPosition=at;
    shot.lastSimulatedTime=gpGlobals->time;
    if (!BuildCurve(shot)) return false;
    if (m_shots.empty()) {
        // New flight begins now, without catching up any idle time.
        m_last=gpGlobals->time;
    }
    m_shots.push_back(std::move(shot));
    return true;
}
void CModularBallisticManager::Update() {
    const double now=gpGlobals->time;
    m_last=gpGlobals->time; // retained for old manager data-map compatibility
    for (size_t i=0;i<m_shots.size();) {
        Shot& shot=m_shots[i];
        // Only advance time since THIS shot was born/last advanced.
        // Preserve outstanding time if catch-up is capped at 100 ms/frame.
        const double dt=modular::ShotTimeBudget(now,shot.lastSimulatedTime);
        double used=0;
        if (!AdvanceShot(shot,dt,used)) {
            m_shots[i]=std::move(m_shots.back());
            m_shots.pop_back();
        } else {
            shot.lastSimulatedTime=std::min(now,shot.lastSimulatedTime+used);
            ++i;
        }
    }
    // Singleton remains for save/restore and new shots, with no Think timer.
}
int CModularBallisticManager::Save(CSave& save) {
    m_savedCount=std::min(int(m_shots.size()),kMaxActive);
    m_savedFingerprint=static_cast<int>(modular::SharedRegistry().Fingerprint());
    m_savedRecordVersion=kShotDiskVersion;
    std::memset(m_savedRecords,0,sizeof(m_savedRecords));
    std::fill_n(m_savedDamageCoefficients,kMaxActive,0.f);
    std::fill_n(m_savedWeaponIds,kMaxActive,string_t(0));
    std::fill_n(m_savedKillSprites,kMaxActive,string_t(0));
    for (int i=0;i<kMaxActive;++i) m_savedOwners[i]=nullptr;
    for (int i=0;i<m_savedCount;++i) {
        const Shot& s=m_shots[i];
        ShotDisk d{};
        d.position[0]=float(s.state.position.x); d.position[1]=float(s.state.position.y); d.position[2]=float(s.state.position.z);
        d.velocity[0]=float(s.state.velocity.x); d.velocity[1]=float(s.state.velocity.y); d.velocity[2]=float(s.state.velocity.z);
        d.seconds=float(s.state.seconds); d.meters=float(s.state.meters);
        d.mass=float(s.physics.massKg); d.diameter=float(s.physics.diameterM);
        d.drag=float(s.physics.dragCoefficient); d.gravity=float(s.physics.gravity);
        d.airDensity=float(s.physics.airDensity);
        d.originalEnergy=float(s.originalEnergy); d.maxMeters=float(s.maxMeters);
        d.damage=s.damage; d.projectileFactor=float(s.projectileFactor);
        d.bulletType=s.bulletType; d.penetrations=s.penetrations;
        if (s.transit.active) {
            const auto& t=s.transit;
            for (int j=0;j<3;++j) {
                const double entryPos[]={t.entry.position.x,t.entry.position.y,t.entry.position.z};
                const double entryVel[]={t.entry.velocity.x,t.entry.velocity.y,t.entry.velocity.z};
                const double exitPos[]={t.exitPosition.x,t.exitPosition.y,t.exitPosition.z};
                const double exitVel[]={t.exitVelocity.x,t.exitVelocity.y,t.exitVelocity.z};
                d.transitEntryPosition[j]=float(entryPos[j]);
                d.transitEntryVelocity[j]=float(entryVel[j]);
                d.transitExitPosition[j]=float(exitPos[j]);
                d.transitExitVelocity[j]=float(exitVel[j]);
            }
            d.transitEntrySeconds=float(t.entry.seconds);
            d.transitEntryMeters=float(t.entry.meters);
            d.transitDistance=float(t.distanceMeters);
            d.transitDuration=float(t.durationSeconds);
            d.transitElapsed=float(t.elapsedSeconds);
            d.transitActive=1;
        }
        std::memcpy(m_savedRecords+i*sizeof(ShotDisk),&d,sizeof(d));
        m_savedOwners[i]=s.owner;
        m_savedDamageCoefficients[i]=s.damageCoefficient;
        m_savedWeaponIds[i]=s.weaponId.empty() ? 0 : ALLOC_STRING(s.weaponId.c_str());
        m_savedKillSprites[i]=s.killSprite.empty() ? 0 : ALLOC_STRING(s.killSprite.c_str());
    }
    return BaseClass::Save(save);
}
int CModularBallisticManager::Restore(CRestore& restore) {
    const int status=BaseClass::Restore(restore);
    if (!status) return status;
    m_shots.clear();
    // A restored shot can also spawn another shot synchronously at impact.
    // Reserve before rebuilding the saved records (same rule as Spawn()).
    m_shots.reserve(kMaxActive);
    if (m_savedCount<0 || m_savedCount>kMaxActive ||
        (m_savedRecordVersion!=2 && m_savedRecordVersion!=kShotDiskVersion) ||
        static_cast<uint32_t>(m_savedFingerprint)!=modular::SharedRegistry().Fingerprint()) {
        ALERT(at_warning,"J.8 manager: incompatible saved shots discarded\n");
        UTIL_Remove(this); return status;
    }
    for (int i=0;i<m_savedCount;++i) {
        CBaseEntity* owner=m_savedOwners[i];
        if (!owner || !owner->pev) continue;
        ShotDisk d{};
        std::memcpy(&d,m_savedRecords+i*sizeof(ShotDisk),sizeof(d));
        Shot s;
        s.owner=owner;
        s.state.position={d.position[0],d.position[1],d.position[2]};
        s.state.velocity={d.velocity[0],d.velocity[1],d.velocity[2]};
        s.state.seconds=d.seconds; s.state.meters=d.meters;
        s.physics.massKg=d.mass; s.physics.diameterM=d.diameter;
        s.physics.dragCoefficient=d.drag; s.physics.gravity=d.gravity;
        s.physics.airDensity=d.airDensity;
        s.originalEnergy=d.originalEnergy; s.maxMeters=d.maxMeters;
        s.damage=d.damage; s.projectileFactor=d.projectileFactor;
        if (m_savedRecordVersion >= 3) {
            s.damageCoefficient=m_savedDamageCoefficients[i];
            s.weaponId=STRING(m_savedWeaponIds[i]);
            s.killSprite=STRING(m_savedKillSprites[i]);
            if (s.weaponId.empty()) {
                // A version-2 shot can be restored and subsequently saved as v3.
                if (s.damageCoefficient != 0 || !s.killSprite.empty()) continue;
            } else {
                const auto* weapon=modular::SharedRegistry().FindWeapon(s.weaponId);
                if (!weapon || (!s.killSprite.empty() &&
                    modular::ReferenceKillAsset(*weapon,s.killSprite).kind == 0)) continue;
                // Registry fingerprint + weapon version encode the damage model.
                // No change to the compact ShotDisk/save stride is needed.
                s.v4=weapon->schemaVersion >= 4;
                if (s.v4) s.damageLogic=s.damageCoefficient > 0 ?
                    modular::DamageLogic::EnergyBased : modular::DamageLogic::ConfigurableBase;
            }
        }
        if (!std::isfinite(s.damageCoefficient) || s.damageCoefficient < 0 || s.damageCoefficient > 1e6f) continue;
        if (s.damageCoefficient > 0) {
            const double expected=modular::EnergyDensityDamage(s.originalEnergy,
                s.physics.diameterM * 1000.0, s.damageCoefficient);
            if (!std::isfinite(expected) || std::abs(expected-s.damage) > .01 * std::max(1.0,expected)) continue;
        }
        s.bulletType=d.bulletType; s.penetrations=d.penetrations;
        if (d.transitActive) {
            auto& t=s.transit;
            t.entry.position={d.transitEntryPosition[0],d.transitEntryPosition[1],d.transitEntryPosition[2]};
            t.entry.velocity={d.transitEntryVelocity[0],d.transitEntryVelocity[1],d.transitEntryVelocity[2]};
            t.entry.seconds=d.transitEntrySeconds; t.entry.meters=d.transitEntryMeters;
            t.exitPosition={d.transitExitPosition[0],d.transitExitPosition[1],d.transitExitPosition[2]};
            t.exitVelocity={d.transitExitVelocity[0],d.transitExitVelocity[1],d.transitExitVelocity[2]};
            t.distanceMeters=d.transitDistance; t.durationSeconds=d.transitDuration;
            t.elapsedSeconds=d.transitElapsed; t.active=true;
        }
        s.lastSimulatedTime=gpGlobals->time;
        s.tracerPosition=GamePoint(s.state.position);
        s.tracerTime=gpGlobals->time;
        if (!std::isfinite(s.damage) || s.damage<=0 || s.damage>1000 ||
            !std::isfinite(s.projectileFactor) || s.projectileFactor<=0 ||
            s.bulletType<1 || s.bulletType>3 || s.penetrations<0 ||
            s.penetrations>kMaxPenetrations ||
            (d.transitActive!=0 && d.transitActive!=1) ||
            !ValidState(s.state,s.physics,s.maxMeters,s.originalEnergy) ||
            (s.transit.active && !modular::ValidTransit(s.transit)) ||
            (!s.transit.active && !BuildCurve(s))) continue;
        if (s.transit.active) {
            // A saved mid-material shot has no free-flight curve until exit.
            // Validate the state as belonging to this traversal, too.
            if (s.state.seconds+1e-3 < s.transit.entry.seconds ||
                s.state.meters+1e-3 < s.transit.entry.meters ||
                std::abs(s.state.seconds-(s.transit.entry.seconds+s.transit.elapsedSeconds))>0.005 ||
                s.transit.entry.meters+s.transit.distanceMeters>s.maxMeters+100.0)
                continue;
        }
        m_shots.push_back(std::move(s));
    }
    g_manager=this;
    m_last=gpGlobals->time;
    // The manager is advanced from EndFrame; it never schedules Think.
    pev->nextthink=0;
    return status;
}
LINK_ENTITY_TO_CLASS(modular_ballistic_manager,CModularBallisticManager);
BEGIN_DATADESC(CModularBallisticManager)
    DEFINE_ARRAY(m_savedOwners,FIELD_EHANDLE,kMaxActive),
    DEFINE_ARRAY(m_savedRecords,FIELD_CHARACTER,sizeof(ShotDisk)*kMaxActive),
    DEFINE_ARRAY(m_savedDamageCoefficients,FIELD_FLOAT,kMaxActive),
    DEFINE_ARRAY(m_savedWeaponIds,FIELD_STRING,kMaxActive),
    DEFINE_ARRAY(m_savedKillSprites,FIELD_STRING,kMaxActive),
    DEFINE_FIELD(m_savedCount,FIELD_INTEGER),
    DEFINE_FIELD(m_savedFingerprint,FIELD_INTEGER),
    DEFINE_FIELD(m_savedRecordVersion,FIELD_INTEGER),
    DEFINE_FIELD(m_last,FIELD_TIME),
END_DATADESC()

void UpdateModularBallisticManager() {
    if (g_manager && g_manager->Count()) g_manager->Update();
}

bool SpawnModularBallisticShot(CBaseEntity* owner,const modular::Weapon& weapon,
    const modular::Cartridge& cartridge,float muzzleVelocityMps,int count,
    const Vector& origin,const matrix3x3& orientation,float spread,std::uint32_t seed,
    const modular::AssetSelection* assets) {
    if (!owner || !owner->pev || count<=0 || count>kMaxActive ||
        !std::isfinite(muzzleVelocityMps) || muzzleVelocityMps<=0 ||
        !std::isfinite(spread) || spread<0 || spread>1) return false;
    const auto& registry=modular::SharedRegistry();
    if (!registry.Valid() || registry.FindWeapon(weapon.id)!=&weapon ||
        registry.FindCartridge(cartridge.id)!=&cartridge) return false;
    bool allowed=false;
    for (const auto& c:weapon.compatible)
        if (c.cartridgeId==cartridge.id &&
            std::abs(c.muzzleVelocityMps-muzzleVelocityMps)<=0.0001f) allowed=true;
    const bool v4 = weapon.schemaVersion >= 4 && cartridge.schemaVersion >= 4;
    if (v4 && (cartridge.attackLogic != modular::AttackLogic::Ballistic ||
        CVAR_GET_FLOAT("sv_ballistics_enabled") <= 0)) return false;
    const int primaryLimit=(weapon.primary.behavior=="ballistic" ||
        (v4 && weapon.primary.behavior=="fire_loaded_cartridge")) ?
        weapon.primary.projectiles*cartridge.projectileCount : 0;
    const int secondaryLimit=(weapon.secondary.behavior=="ballistic" ||
        (v4 && weapon.secondary.behavior=="fire_loaded_cartridge")) ?
        weapon.secondary.projectiles*cartridge.projectileCount : 0;
    if (!allowed || count>std::max(primaryLimit,secondaryLimit)) return false;
    if (g_manager && g_manager->Count()+count>kMaxActive) return false;
    if (!g_manager) {
        CBaseEntity* base=CBaseEntity::Create(const_cast<char*>("modular_ballistic_manager"),
            origin,Vector(0,0,0),nullptr);
        if (!base || !g_manager) return false;
    }
    CSeededRandomGenerator random;
    const modular::AssetSelection fallback{cartridge.id,weapon.primary.mode,seed,modular::AssetAction::Primary};
    const int originalCount=g_manager->Count();
    int added=0;
    for (int i=0;i<count;++i) {
        const std::uint32_t n=seed+i*4+1;
        const float x=random.GetFloat(n,-.5f,.5f)+random.GetFloat(n+1,-.5f,.5f);
        const float y=random.GetFloat(n+2,-.5f,.5f)+random.GetFloat(n+3,-.5f,.5f);
        const Vector direction=(orientation.GetForward()+orientation.GetRight()*(x*spread)+
                                orientation.GetUp()*(y*spread)).Normalize();
        if (!g_manager->Add(owner,weapon,cartridge,origin,direction,muzzleVelocityMps,assets ? assets : &fallback)) {
            // Roll back a whole shotgun shot if any projectile fails to initialize.
            g_manager->RollbackTo(originalCount);
            if (CVAR_GET_FLOAT("sv_ballistics_debug")>0)
                ALERT(at_warning,"J.8: rejected projectile %d/%d\n",i+1,count);
            return false;
        }
        ++added;
    }
    return added==count;
}
