cmake_minimum_required(VERSION 3.14)
project(live-paper VERSION 0.0.0)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24.0)
    cmake_policy(SET CMP0135 NEW)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FetchContent)

# FFmpeg
if(MSVC)
    FetchContent_Declare(
        FFmpeg
        URL https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-full-shared.7z
    )
    FetchContent_GetProperties(FFmpeg)
    if(NOT ffmpeg_POPULATED)
        FetchContent_MakeAvailable(FFmpeg)
        add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
        set_target_properties(FFmpeg::FFmpeg PROPERTIES
            INTERFACE_LINK_DIRECTORIES "${ffmpeg_SOURCE_DIR}/lib"
            INTERFACE_INCLUDE_DIRECTORIES "${ffmpeg_SOURCE_DIR}/include/"
            INTERFACE_LINK_LIBRARIES "avcodec;avformat;avutil;swscale"
        )

        set(FFMPEG_DLL_NAMES "avcodec" "avformat" "avutil" "swresample" "swscale")
        set(FFMPEG_DLLS)
        foreach(DLL_NAME ${FFMPEG_DLL_NAMES})
            file(GLOB DLL_FILES "${ffmpeg_SOURCE_DIR}/bin/${DLL_NAME}*.dll")
            list(LENGTH DLL_FILES DLL_FILES_LENGTH)
            if(NOT DLL_FILES_LENGTH EQUAL 1)
                message(FATAL_ERROR "Expected exactly one DLL for ${DLL_NAME}, but found ${DLL_FILES_LENGTH}")
            endif()
            list(APPEND FFMPEG_DLLS ${DLL_FILES})
        endforeach()
    endif()
elseif(MINGW)
    # MinGW build: the FFmpeg import libraries are provided by the
    # prebuilt MinGW FFmpeg package. Point LIVEPAPER_FFMPEG_DIR at the
    # directory that contains lib/, include/ and bin/.
    if(NOT LIVEPAPER_FFMPEG_DIR)
        message(FATAL_ERROR
            "Set LIVEPAPER_FFMPEG_DIR to the directory of a prebuilt MinGW FFmpeg "
            "shared build (containing lib/, include/ and bin/).")
    endif()
    add_library(FFmpeg::FFmpeg INTERFACE IMPORTED)
    set_target_properties(FFmpeg::FFmpeg PROPERTIES
        INTERFACE_LINK_DIRECTORIES "${LIVEPAPER_FFMPEG_DIR}/lib"
        INTERFACE_INCLUDE_DIRECTORIES "${LIVEPAPER_FFMPEG_DIR}/include"
        INTERFACE_LINK_LIBRARIES "avcodec;avformat;avutil;swscale"
    )
    set(FFMPEG_DLL_NAMES "avcodec" "avformat" "avutil" "swresample" "swscale")
    set(FFMPEG_DLLS)
    foreach(DLL_NAME ${FFMPEG_DLL_NAMES})
        file(GLOB DLL_FILES "${LIVEPAPER_FFMPEG_DIR}/bin/${DLL_NAME}*.dll")
        list(LENGTH DLL_FILES DLL_FILES_LENGTH)
        if(NOT DLL_FILES_LENGTH EQUAL 1)
            message(FATAL_ERROR "Expected exactly one DLL for ${DLL_NAME}, but found ${DLL_FILES_LENGTH}")
        endif()
        list(APPEND FFMPEG_DLLS ${DLL_FILES})
    endforeach()
else()
    find_package(FFmpeg 6.0 REQUIRED COMPONENTS AVCODEC AVFORMAT AVUTIL SWSCALE)
endif()


# SDL2
if(MSVC)
    FetchContent_Declare(
        SDL2
        URL https://www.libsdl.org/release/SDL2-devel-2.30.9-VC.zip
    )
    FetchContent_GetProperties(SDL2)
    if(NOT sdl2_POPULATED)
        FetchContent_MakeAvailable(SDL2)
        add_library(SDL2::SDL2 SHARED IMPORTED)
        set_target_properties(SDL2::SDL2 PROPERTIES
            IMPORTED_LOCATION "${sdl2_SOURCE_DIR}/lib/x64/SDL2.dll"
            IMPORTED_IMPLIB "${sdl2_SOURCE_DIR}/lib/x64/SDL2.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${sdl2_SOURCE_DIR}/include/"
        )
        add_library(SDL2::SDL2main STATIC IMPORTED)
        set_target_properties(SDL2::SDL2main PROPERTIES
            IMPORTED_LOCATION "${sdl2_SOURCE_DIR}/lib/x64/SDL2main.lib"
        )
    endif()
elseif(MINGW)
    # MinGW build: use the import libraries of the prebuilt MinGW SDL2
    # package. Point LIVEPAPER_SDL2_DIR at its x86_64-w64-mingw32 directory.
    if(NOT LIVEPAPER_SDL2_DIR)
        message(FATAL_ERROR
            "Set LIVEPAPER_SDL2_DIR to the x86_64-w64-mingw32 directory of a "
            "prebuilt MinGW SDL2 development package.")
    endif()
    # The import libraries are looked up by their exact names; find_library()
    # can't be used here, because it is restricted to the toolchain root path.
    if(EXISTS "${LIVEPAPER_SDL2_DIR}/lib/libSDL2.dll.a")
        set(SDL2_LIBRARY "${LIVEPAPER_SDL2_DIR}/lib/libSDL2.dll.a")
    elseif(EXISTS "${LIVEPAPER_SDL2_DIR}/lib/libSDL2.a")
        set(SDL2_LIBRARY "${LIVEPAPER_SDL2_DIR}/lib/libSDL2.a")
    else()
        message(FATAL_ERROR "Can't find the SDL2 import library in '${LIVEPAPER_SDL2_DIR}/lib'")
    endif()
    if(EXISTS "${LIVEPAPER_SDL2_DIR}/lib/libSDL2main.a")
        set(SDL2MAIN_LIBRARY "${LIVEPAPER_SDL2_DIR}/lib/libSDL2main.a")
    else()
        message(FATAL_ERROR "Can't find the SDL2main library in '${LIVEPAPER_SDL2_DIR}/lib'")
    endif()
    add_library(SDL2::SDL2 UNKNOWN IMPORTED)
    set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_LOCATION "${SDL2_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIVEPAPER_SDL2_DIR}/include/SDL2"
    )
    add_library(SDL2::SDL2main STATIC IMPORTED)
    set_target_properties(SDL2::SDL2main PROPERTIES
        IMPORTED_LOCATION "${SDL2MAIN_LIBRARY}"
    )
else()
    find_package(SDL2 REQUIRED)
    if (NOT TARGET SDL2::SDL2)
        add_library(SDL2::SDL2 UNKNOWN IMPORTED)
        set_target_properties(SDL2::SDL2 PROPERTIES
            IMPORTED_LOCATION "${SDL2_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIRS}"
        )
    endif()
endif()


# tray (on windows only)
if(WIN32)
    FetchContent_Declare(
        tray
        URL https://raw.githubusercontent.com/zserge/tray/master/tray.h
        DOWNLOAD_NO_EXTRACT TRUE
    )
    FetchContent_GetProperties(tray)
    if(NOT tray_POPULATED)
        FetchContent_MakeAvailable(tray)
        add_library(tray::tray INTERFACE IMPORTED)
        set_target_properties(tray::tray PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${tray_SOURCE_DIR}"
        )
    endif()
endif()


# argtable
FetchContent_Declare(
    argtable
    URL https://github.com/argtable/argtable3/releases/download/v3.2.2.f25c624/argtable-v3.2.2.f25c624-amalgamation.tar.gz
)
FetchContent_GetProperties(argtable)
if(NOT argtable_POPULATED)
    FetchContent_MakeAvailable(argtable)
    add_library(argtable::argtable INTERFACE IMPORTED)
    set_target_properties(argtable::argtable PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${argtable_SOURCE_DIR}/"
        INTERFACE_SOURCES "${argtable_SOURCE_DIR}/argtable3.c"
    )
endif()

# executable
add_executable(live-paper
    $<$<PLATFORM_ID:Windows>:src/platform_windows.c;assets/win.rc>
    $<$<PLATFORM_ID:Linux>:src/platform_linux.c>
    $<$<PLATFORM_ID:Darwin>:src/platform_macos.c>
    src/main.c src/fail.c src/video.c src/log.c src/sysinfo.c
    src/dock.c src/dock_ui.c src/dock_font.c src/dock_metrics.c
    src/dirlist.c src/launcher.c
)
set_target_properties(live-paper PROPERTIES 
    WIN32_EXECUTABLE $<PLATFORM_ID:Windows>
    C_STANDARD 99
)
target_link_libraries(live-paper PRIVATE
    $<$<BOOL:${MINGW}>:mingw32>
    "$<$<PLATFORM_ID:Windows>:SDL2::SDL2main;tray::tray;shell32;shlwapi;psapi;ole32;uuid;iphlpapi;pdh>"
    "$<$<PLATFORM_ID:Linux>:m;X11>"
    SDL2::SDL2 FFmpeg::FFmpeg argtable::argtable
)
# SetProcessDPIAware() and friends are only declared for Windows Vista and
# later. The definition has to be set on the command line, because windows.h
# is pulled in by SDL.h before any of our own headers.
target_compile_definitions(live-paper PRIVATE
    "$<$<PLATFORM_ID:Windows>:_WIN32_WINNT=0x0601;WINVER=0x0601>"
)
# MSVC warns about the standard functions of C99 (fopen, strdup, ...) and
# offers its own "secure" variants instead. The program is written in plain
# C99 and has to build with MinGW and with GCC as well, so the warnings are
# switched off instead of using the variants of MSVC.
target_compile_definitions(live-paper PRIVATE
    "$<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>"
    "$<$<C_COMPILER_ID:MSVC>:_CRT_NONSTDC_NO_DEPRECATE>"
)
add_custom_command(TARGET live-paper POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico"
        $<TARGET_FILE_DIR:live-paper>
)
if(WIN32)
    # Copy the runtime DLLs of the dependencies next to the executable, so that
    # the program can be started without any additional installation.
    set(SDL2_DLL "")
    if(MSVC)
        set(SDL2_DLL $<TARGET_FILE:SDL2::SDL2>)
    elseif(MINGW)
        file(GLOB SDL2_DLL_FILES "${LIVEPAPER_SDL2_DIR}/bin/SDL2.dll")
        list(LENGTH SDL2_DLL_FILES SDL2_DLL_FILES_LENGTH)
        if(NOT SDL2_DLL_FILES_LENGTH EQUAL 1)
            message(FATAL_ERROR "Expected exactly one SDL2.dll, but found ${SDL2_DLL_FILES_LENGTH}")
        endif()
        set(SDL2_DLL ${SDL2_DLL_FILES})
    endif()
    add_custom_command(TARGET live-paper POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${FFMPEG_DLLS}
            ${SDL2_DLL}
            $<TARGET_FILE_DIR:live-paper>
    )
endif()


# console variant
# On Windows the main executable is a GUI application (it shows a tray icon
# and has no console window). This variant is built as a console application,
# so that the diagnostics are printed to the terminal it is started from.
if(WIN32)
    add_executable(live-paper-console
        src/platform_windows.c assets/win.rc
        src/main.c src/fail.c src/video.c src/log.c src/sysinfo.c
        src/dock.c src/dock_ui.c src/dock_font.c src/dock_metrics.c
        src/dirlist.c src/launcher.c
    )
    set_target_properties(live-paper-console PROPERTIES
        WIN32_EXECUTABLE FALSE
        C_STANDARD 99
    )
    # SDL_MAIN_HANDLED tells SDL not to rename main() and to provide its own
    # WinMain(); the console variant uses the standard C entry point instead.
    target_compile_definitions(live-paper-console PRIVATE
        SDL_MAIN_HANDLED
        "_WIN32_WINNT=0x0601;WINVER=0x0601"
        # The same as for the program itself: the standard functions of C99 are
        # used, so the warnings of MSVC are switched off.
        "$<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>"
        "$<$<C_COMPILER_ID:MSVC>:_CRT_NONSTDC_NO_DEPRECATE>"
    )
    target_link_libraries(live-paper-console PRIVATE
        "$<$<PLATFORM_ID:Windows>:tray::tray;shell32;shlwapi;psapi;ole32;uuid;iphlpapi;pdh>"
        SDL2::SDL2 FFmpeg::FFmpeg argtable::argtable
    )
    add_custom_command(TARGET live-paper-console POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico"
            $<TARGET_FILE_DIR:live-paper-console>
    )
    if(WIN32)
        add_custom_command(TARGET live-paper-console POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${FFMPEG_DLLS}
                ${SDL2_DLL}
                $<TARGET_FILE_DIR:live-paper-console>
        )
    endif()
endif()


# binaries
if(WIN32)
    set(INSTALL_DIR ".")
    install(TARGETS live-paper RUNTIME DESTINATION ${INSTALL_DIR})
    # The console variant is useful for the diagnostics, so it is part of the
    # package as well.
    install(TARGETS live-paper-console RUNTIME DESTINATION ${INSTALL_DIR})
    install(FILES ${SDL2_DLL} ${FFMPEG_DLLS} "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico" 
        DESTINATION ${INSTALL_DIR}
    )
elseif(NOT WIN32)
    # On Linux the libraries of SDL and of FFmpeg are the ones of the system,
    # so the package contains only the program itself.
    install(TARGETS live-paper RUNTIME DESTINATION bin)
endif()

# The package for the release. The installer of Windows is built with the WIX
# toolset, which only exists there; the other platforms get a tarball.
if(MSVC OR NOT WIN32)
    set(CPACK_PACKAGE_DIRECTORY dist)
    set(CPACK_PACKAGE_NAME LivePaper)
    set(CPACK_PACKAGE_VENDOR Zuranthus)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
    set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/ZMajeh/LivePaper-Win11-Support")
    set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico")

    if(MSVC)
        set(CPACK_GENERATOR WIX ZIP)
        # Both programs get a shortcut in the start menu.
        set(CPACK_PACKAGE_EXECUTABLES live-paper LivePaper live-paper-console "LivePaper (console)")
        set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico")
        set(CPACK_WIX_UPGRADE_GUID A73EF541-19D7-4E50-8CC6-B4E1BAFB760D)
    elseif(NOT WIN32)
        set(CPACK_GENERATOR TGZ)
        # The name of the file, so that the system can be told apart from the
        # package for Windows.
        set(CPACK_SYSTEM_NAME linux-x86_64)
    endif()

    include(CPack)
endif()
