cmake_minimum_required(VERSION 3.24)

project(MxUSlides VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(MXU_CORE_ONLY "Build only the portable core and tests (no Windows UI)" OFF)

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Widgets Test)
qt_standard_project_setup(REQUIRES 6.12)

if(MSVC)
    # The sources, UI copy, and test fixtures contain UTF-8 strings. Keep the
    # flag off the Windows resource compiler.
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/utf-8>")
endif()

add_subdirectory(third_party/miniz)

# Rendering, storage, and editing logic. Kept free of window chrome so the
# tests and future output backends can use it without starting the main window.
add_library(mxu_core STATIC
    src/core/document.cpp
    src/core/library.cpp
    src/core/presentationfile.cpp
    src/core/editor.cpp
    src/render/sliderenderer.cpp
)
target_include_directories(mxu_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(mxu_core PUBLIC Qt6::Core Qt6::Gui miniz)

include(CTest)

if(NOT MXU_CORE_ONLY)
    qt_add_executable(MxUSlides WIN32
        src/main.cpp
        src/ui/mainwindow.cpp
        src/ui/mainwindow.h
        src/ui/theme.cpp
        src/ui/theme.h
        src/ui/icons.cpp
        src/ui/icons.h
        src/ui/widgets.cpp
        src/ui/widgets.h
        src/ui/librarypanel.cpp
        src/ui/librarypanel.h
        src/ui/slidecanvas.cpp
        src/ui/slidecanvas.h
        src/ui/slidestrip.cpp
        src/ui/slidestrip.h
        src/ui/inspectorpanel.cpp
        src/ui/inspectorpanel.h
        src/ui/presenterpage.cpp
        src/ui/presenterpage.h
        src/ui/programwindow.cpp
        src/ui/programwindow.h
        src/ui/stagewindow.cpp
        src/ui/stagewindow.h
        src/ui/settingsdialog.cpp
        src/ui/settingsdialog.h
    )
    target_link_libraries(MxUSlides PRIVATE mxu_core Qt6::Widgets)
    set_target_properties(MxUSlides PROPERTIES OUTPUT_NAME "MxU Slides")

    if(WIN32)
        target_sources(MxUSlides PRIVATE resources/app.rc)
        target_include_directories(MxUSlides PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/resources)
        if(MSVC)
            # app.rc embeds resources/app.manifest; avoid a second linker manifest.
            target_link_options(MxUSlides PRIVATE /MANIFEST:NO)
        endif()
    endif()

    qt_add_resources(MxUSlides "app_icons"
        PREFIX "/"
        BASE ${CMAKE_CURRENT_SOURCE_DIR}/resources
        FILES resources/icons/app-256.png
    )

    if(WIN32)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS ${QT6_INSTALL_PREFIX}/bin)
        if(WINDEPLOYQT_EXECUTABLE)
            add_custom_target(deploy
                COMMAND ${WINDEPLOYQT_EXECUTABLE}
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    --compiler-runtime
                    "$<TARGET_FILE:MxUSlides>"
                DEPENDS MxUSlides
                COMMENT "Collecting Qt runtime next to MxU Slides.exe"
                VERBATIM
            )
        endif()
    endif()

    install(TARGETS MxUSlides RUNTIME DESTINATION .)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
