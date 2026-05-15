#pragma once

#include <Miro/Miro.h>

struct Tick
{
    double angle = 0.0;

    MIRO_REFLECT(angle)
};

Tick getCurrentTick();
