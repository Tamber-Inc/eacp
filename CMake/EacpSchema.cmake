# eacp_target_uses_schema — wire a target to consume a Miro schema.
#
# HANDLERS routes the consumer to the full schema INTERFACE library
# (registrations OBJECT lib + generated includes), so the
# MIRO_EXPORT_COMMAND static initializers land in the consumer's binary
# — needed for any target that dispatches commands via
# Miro::Bridge::useStaticRegistry(). Plain (no HANDLERS) routes to the
# sibling _includes INTERFACE library, which exposes only the generated
# header search paths and is the right choice for clients that just
# consume the typed wrapper headers.
#
# add_dependencies() does not propagate through INTERFACE link, so the
# build-order dep on the codegen run is wired directly on the consumer.
#
# Usage:
#   eacp_target_uses_schema(MyServer MySchema HANDLERS)
#   eacp_target_uses_schema(MyClient MySchema)
function(eacp_target_uses_schema target schema)
    cmake_parse_arguments(EUS "HANDLERS" "" "" ${ARGN})

    if (NOT TARGET ${target})
        message(FATAL_ERROR
            "eacp_target_uses_schema: target '${target}' does not exist")
    endif ()
    if (NOT TARGET ${schema})
        message(FATAL_ERROR
            "eacp_target_uses_schema: schema '${schema}' does not exist; "
            "call miro_export(${schema} ...) first")
    endif ()

    if (EUS_HANDLERS)
        target_link_libraries(${target} PRIVATE ${schema})
    else ()
        target_link_libraries(${target} PRIVATE ${schema}_includes)
    endif ()

    if (TARGET ${schema}_gen)
        add_dependencies(${target} ${schema}_gen)
    endif ()
endfunction()
