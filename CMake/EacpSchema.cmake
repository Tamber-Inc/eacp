# eacp_target_uses_schema — wire a target to consume a Miro schema.
#
# Default mode (no HANDLERS) links the schema INTERFACE library, which
# exposes the generated header search paths. Right for clients that just
# consume the typed wrapper headers (cpp-client.h etc.).
#
# HANDLERS additionally splices the schema's registration sources into
# the consumer (via target_sources). The MIRO_EXPORT_COMMAND static
# initializers in those sources fire at the consumer's startup — needed
# for any target that dispatches commands via
# Miro::Bridge::useStaticRegistry().
#
# The build-order dependency on the codegen run is wired via DEFER, so
# the helper can be called against a schema declared in a sibling
# directory (the codegen target is materialised at the schema
# directory's end-of-processing).
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

    target_link_libraries(${target} PRIVATE ${schema})

    if (EUS_HANDLERS)
        get_target_property(srcs ${schema} MIRO_SCHEMA_SOURCES)
        if (srcs)
            target_sources(${target} PRIVATE ${srcs})
        endif ()
    endif ()

    # _${schema}_codegen is created at end-of-directory by
    # _miro_export_finalize, so the add_dependencies call has to wait
    # until then. For consumers in a different directory than the
    # schema's, that directory has already finished processing by the
    # time we get here and the deferred call finds the target ready.
    # EVAL CODE bakes the values in now; a plain DEFER CALL would
    # defer variable expansion to fire time, when this function scope
    # is gone.
    cmake_language(EVAL CODE
            "cmake_language(DEFER CALL _eacp_target_uses_schema_wire "
            "${target} ${schema})")
endfunction()

function(_eacp_target_uses_schema_wire target schema)
    if (TARGET _${schema}_codegen)
        add_dependencies(${target} _${schema}_codegen)
    endif ()
endfunction()
