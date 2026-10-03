include_guard(GLOBAL)

option(STAPIK_WARNINGS_AS_ERRORS "Treat compiler warnings in Stapik targets as errors (meant for CI)" OFF)

# Enables the warning set shared by all Stapik targets. Warnings are private to the target.
function(stapik_enable_warnings targetName)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        return()
    endif()

    target_compile_options(${targetName} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    if(STAPIK_WARNINGS_AS_ERRORS)
        target_compile_options(${targetName} PRIVATE -Werror)
    endif()
endfunction()
