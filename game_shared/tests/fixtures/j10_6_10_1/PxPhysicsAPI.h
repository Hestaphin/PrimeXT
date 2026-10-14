#pragma once
// Explicit setter facade: not a PhysX SDK, actor, solver, or simulation.
namespace physx
{
struct PxD6Axis { enum Enum { eX,eY,eZ,eTWIST,eSWING1,eSWING2 }; };
struct PxD6Motion { enum Enum { eLOCKED,eLIMITED,eFREE }; };
struct PxD6Joint
{
    PxD6Motion::Enum motion[6]{};
    unsigned writes[6]{};
    void setMotion(PxD6Axis::Enum axis,PxD6Motion::Enum value) { motion[axis]=value;++writes[axis]; }
};
}
