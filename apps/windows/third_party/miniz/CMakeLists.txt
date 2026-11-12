cmake_minimum_required(VERSION 3.24)

# miniz 3.0.2 (MIT / public domain). Vendored so the app can read and write
# .mxuslides ZIP bundles without a system compression dependency.
add_library(miniz STATIC
    miniz.c
    miniz_tdef.c
    miniz_tinfl.c
    miniz_zip.c
)

target_include_directories(miniz PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(miniz PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
if(MSVC)
    target_compile_options(miniz PRIVATE /W0)
else()
    target_compile_options(miniz PRIVATE -w)
endif()
