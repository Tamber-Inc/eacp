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

    if (TARGET ${schema}_Codegen)
        add_dependencies(${target} ${schema}_Codegen)
    endif ()
endfunction()
