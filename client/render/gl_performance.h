#pragma once
#include "performance_gpu.h"

void R_ProfileInitialize();
void R_ProfileShutdown();
void R_ProfileBeginFrame();
void R_ProfileEndFrame();
class RenderProfileFrame
{
public:
    RenderProfileFrame() { R_ProfileBeginFrame(); }
    ~RenderProfileFrame() { R_ProfileEndFrame(); }
    RenderProfileFrame(const RenderProfileFrame &) = delete;
    RenderProfileFrame &operator=(const RenderProfileFrame &) = delete;
};
