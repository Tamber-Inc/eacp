#include "Types.h"

#include <eacp/WebView/WebView.h>

#include <algorithm>
#include <cmath>

namespace
{
double cyclePhase = 0.0;
} // namespace

EACP_STATE(Parameters, parametersState, parameters, Parameters {})

Parameters getParameters()
{
    return parametersState().get();
}

void setParameters(const Parameters& req)
{
    parametersState().modify(
        [&](Parameters& p)
        {
            p.level = std::clamp(req.level, 0.0, 1.0);
            p.autoCycle = req.autoCycle;
        });

    cyclePhase = std::asin(parametersState().get().level * 2.0 - 1.0);
}

void advanceTick()
{
    parametersState().modify(
        [](Parameters& p)
        {
            p.counter++;

            if (p.autoCycle)
            {
                cyclePhase += 0.05;
                p.level = 0.5 + 0.5 * std::sin(cyclePhase);
            }
        });
}

MIRO_EXPORT_COMMANDS(getParameters, setParameters)
