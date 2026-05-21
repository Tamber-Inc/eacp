#include "Schema.h"

#include <chrono>

PingResponse ping()
{
    using namespace std::chrono;
    auto now = system_clock::now().time_since_epoch();
    return {.pong = true,
            .serverTimeMs = duration_cast<milliseconds>(now).count()};
}

MIRO_EXPORT_COMMAND(ping)
