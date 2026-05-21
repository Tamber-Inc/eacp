#pragma once

#include "../../Core/Utils/StateValue.h"

#include <Miro/Miro.h>

#include <ea_data_structures/Pointers/OwningPointer.h>
#include <ea_data_structures/Structures/Vector.h>

#include <functional>
#include <string>
#include <utility>

namespace eacp::Graphics
{

// Subscribes the bridge to a single state's onChange broadcaster and
// re-emits the current value as an event whenever it fires. The
// returned Listener keeps the subscription alive — drop it to
// disconnect.
template <typename T>
eacp::Listener bindToBridge(eacp::StateValue<T>& state,
                            Miro::Bridge& bridge,
                            std::string eventName)
{
    return eacp::Listener {
        state.onChange(),
        [&state, &bridge, name = std::move(eventName)]
        { bridge.emit(name, state.get()); }};
}

// ---------- Auto-bind registry ----------
//
// EACP_STATE(...) registers a binder into a process-wide list during
// static init. Each transport (currently WebViewBridge) calls
// attachStaticStateBinders() in its constructor to subscribe every
// registered state at once. The transport owns the resulting Listener
// pack — when it dies, the listeners unsubscribe.

using StateBinder =
    std::function<EA::OwningPointer<EA::Listener>(Miro::Bridge&)>;

namespace Detail
{

// Inline so static initializers in any TU using EACP_STATE can call it
// without needing to link the eacp-webview library — important for the
// Miro codegen executable, which links the user's command sources but
// not the runtime transport library.
inline EA::Vector<StateBinder>& stateBinderRegistry()
{
    static auto registry = EA::Vector<StateBinder> {};
    return registry;
}

template <typename T>
inline void registerStateBinder(eacp::StateValue<T>& (*accessor)(),
                                std::string eventName)
{
    stateBinderRegistry().add(
        [accessor, eventName = std::move(eventName)](Miro::Bridge& bridge)
            -> EA::OwningPointer<EA::Listener>
        {
            auto& state = accessor();
            return EA::makeOwned<EA::Listener>(
                state.onChange(),
                [&state, &bridge, eventName]
                { bridge.emit(eventName, state.get()); },
                EA::Listener::Modes::TriggerOnEvent);
        });
}

} // namespace Detail

EA::Vector<EA::OwningPointer<EA::Listener>>
    attachStaticStateBinders(Miro::Bridge& bridge);

} // namespace eacp::Graphics

#define EACP_STATE_CAT2(a, b) a##b
#define EACP_STATE_CAT(a, b) EACP_STATE_CAT2(a, b)

// EACP_STATE — single-declaration state hub. Generates the StateValue
// accessor, registers the typed bridge event, and registers an auto-
// bind so any transport built later picks up changes automatically.
// Replaces the old split across Types.h (declaration), Commands.cpp
// (definition) and Main.cpp (bindToBridge).
//
// Usage (must be in a TU, not a header):
//   EACP_STATE(TodoState, todoState, todos, makeInitialState())
//
// Generates:
//   eacp::StateValue<TodoState>& todoState();   // function definition
//   // ...and registers event "todos" with payload TodoState
//   // ...and registers a binder so WebViewBridge construction
//   //    auto-subscribes and starts broadcasting.
//
// The user still owns the read-side command if the frontend needs to
// fetch initial state:
//   TodoState getTodos() { return todoState().get(); }
//   MIRO_EXPORT_COMMAND(getTodos)
//
// If the initial expression contains top-level commas (e.g. a brace
// init list with multiple fields), wrap it in an extra set of parens
// so the preprocessor sees one argument.
//
// Event registration and binder registration both happen inside one
// static-init lambda so the order is locked together — important when
// other TUs also register events, since a split would expose us to
// unspecified inter-TU init ordering.
#define EACP_STATE(T, accessor, eventName, ...)                                     \
    eacp::StateValue<T>& accessor()                                                 \
    {                                                                               \
        static auto state = eacp::StateValue<T> {__VA_ARGS__};                      \
        return state;                                                               \
    }                                                                               \
    namespace                                                                       \
    {                                                                               \
    [[maybe_unused]] const auto EACP_STATE_CAT(eacpState_, __LINE__) = []           \
    {                                                                               \
        ::Miro::CommandExport::Detail::registerEvent<T>(#eventName);                \
        ::eacp::Graphics::Detail::registerStateBinder<T>(&accessor, #eventName);    \
        return 0;                                                                   \
    }();                                                                            \
    }

// EACP_KEYED_STATE — same as EACP_STATE but additionally declares the
// payload as a keyed collection (a vector field on the payload, each
// element identified by an id field). The codegen `hooks` format uses
// this metadata to emit `useXxx` / `useXxxIds` / `useXxxItem` hooks
// backed by `makeKeyedStore`, so the user gets per-id selector
// re-renders for free.
//
//   EACP_KEYED_STATE(TodoState, todoState, todos,
//                    items,            // collection field on TodoState
//                    id,               // key field on TodoItem
//                    makeInitialState())
#define EACP_KEYED_STATE(T, accessor, eventName, collectionField, keyField, ...)    \
    eacp::StateValue<T>& accessor()                                                 \
    {                                                                               \
        static auto state = eacp::StateValue<T> {__VA_ARGS__};                      \
        return state;                                                               \
    }                                                                               \
    namespace                                                                       \
    {                                                                               \
    [[maybe_unused]] const auto EACP_STATE_CAT(eacpKeyedState_, __LINE__) = []      \
    {                                                                               \
        ::Miro::CommandExport::Detail::registerKeyedEvent<T>(                       \
            #eventName, #collectionField, #keyField);                               \
        ::eacp::Graphics::Detail::registerStateBinder<T>(&accessor, #eventName);    \
        return 0;                                                                   \
    }();                                                                            \
    }
