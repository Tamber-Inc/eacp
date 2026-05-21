#pragma once

#include <Miro/Miro.h>

struct PingResponse
{
    bool pong = false;
    long long serverTimeMs = 0;

    MIRO_REFLECT(pong, serverTimeMs)
};

PingResponse ping();
