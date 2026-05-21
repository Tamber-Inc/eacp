#pragma once

#include <Miro/Miro.h>

struct Parameters
{
    double level = 0.5;
    bool autoCycle = false;
    long long counter = 0;

    MIRO_REFLECT(level, autoCycle, counter)
};

Parameters getParameters();
void setParameters(const Parameters& req);
void advanceTick();
