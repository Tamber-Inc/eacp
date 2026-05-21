#include "StateBridge.h"

namespace eacp::Graphics
{

EA::Vector<EA::OwningPointer<EA::Listener>>
    attachStaticStateBinders(Miro::Bridge& bridge)
{
    auto listeners = EA::Vector<EA::OwningPointer<EA::Listener>> {};

    for (auto& binder: Detail::stateBinderRegistry())
        listeners.add(binder(bridge));

    return listeners;
}

} // namespace eacp::Graphics
