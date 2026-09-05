cmake_minimum_required(VERSION 3.16)

project(pasgen
    VERSION 1.0.0
    DESCRIPTION "Secure cross-platform password manager"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH
        "/opt/homebrew"
        "/opt/homebrew/opt/openssl@3"
        "/usr/local/opt/openssl@3"
        "/usr/local/opt/openssl"
    )
endif()
find_package(OpenSSL 1.1.1 REQUIRED)

find_package(SDL2 QUIET CONFIG)
if(NOT SDL2_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 QUIET sdl2)
    endif()
endif()

if(NOT SDL2_FOUND AND NOT TARGET SDL2::SDL2)
    message(STATUS "SDL2 not found locally — fetching from source")
    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-2.30.9
        GIT_SHALLOW    TRUE
    )
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON  CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(SDL2)
endif()

find_package(OpenGL REQUIRED)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    # std::string-backed InputText, so text fields are not capped by a fixed
    # char buffer (a too-small buffer silently truncated stored passwords/notes).
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imgui_SOURCE_DIR}/misc/cpp
)
if(TARGET SDL2::SDL2)
    target_link_libraries(imgui_lib PUBLIC SDL2::SDL2)
elseif(TARGET SDL2-static)
    target_link_libraries(imgui_lib PUBLIC SDL2-static)
elseif(DEFINED SDL2_INCLUDE_DIRS)
    target_include_directories(imgui_lib PUBLIC ${SDL2_INCLUDE_DIRS})
endif()
target_link_libraries(imgui_lib PUBLIC OpenGL::GL)
if(UNIX AND NOT APPLE)
    target_compile_definitions(imgui_lib PUBLIC GL_GLEXT_PROTOTYPES)
endif()
if(APPLE)
    target_compile_options(imgui_lib PRIVATE -Wno-deprecated-declarations)
endif()

find_package(nlohmann_json 3.9.0 QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Falls back to PBKDF2 if libargon2 isn't available on the system.
set(ARGON2_FOUND FALSE)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(ARGON2 QUIET libargon2)
    if(ARGON2_FOUND)
        message(STATUS "Found Argon2 via pkg-config")
    endif()
endif()
if(NOT ARGON2_FOUND)
    find_path(ARGON2_INCLUDE_DIR argon2.h
        HINTS /usr/include /usr/local/include /opt/homebrew/include)
    find_library(ARGON2_LIBRARY NAMES argon2
        HINTS /usr/lib /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu
              /usr/local/lib /opt/homebrew/lib)
    if(ARGON2_INCLUDE_DIR AND ARGON2_LIBRARY)
        set(ARGON2_FOUND TRUE)
        set(ARGON2_INCLUDE_DIRS ${ARGON2_INCLUDE_DIR})
        set(ARGON2_LIBRARIES    ${ARGON2_LIBRARY})
        message(STATUS "Found Argon2: ${ARGON2_LIBRARY}")
    else()
        message(STATUS "Argon2 not found — using PBKDF2 fallback (install libargon2-dev for stronger KDF)")
        add_definitions(-DUSE_PBKDF2_FALLBACK)
    endif()
endif()

FetchContent_Declare(
    pfd
    GIT_REPOSITORY https://github.com/samhocevar/portable-file-dialogs.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(pfd)
if(NOT pfd_POPULATED)
    FetchContent_Populate(pfd)
endif()

set(PASGEN_SOURCES
    src/main.cpp
    src/app/App.cpp
    src/core/Database.cpp
    src/core/Account.cpp
    src/core/Category.cpp
    src/crypto/CryptoEngine.cpp
    src/crypto/KeyDerivation.cpp
    src/crypto/SecureMemory.cpp
    src/util/TOTPGenerator.cpp
    src/util/PasswordGenerator.cpp
    src/util/Config.cpp
    src/util/Theme.cpp
    src/util/Font.cpp
    src/util/Widgets.cpp
    src/util/Uuid.cpp
)

add_executable(pasgen ${PASGEN_SOURCES})

if(WIN32)
    set_target_properties(pasgen PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
endif()

# Fonts selectable in Settings > Preferences > Font
set(PASGEN_BUNDLED_FONTS
    ${imgui_SOURCE_DIR}/misc/fonts/Roboto-Medium.ttf
    ${imgui_SOURCE_DIR}/misc/fonts/DroidSans.ttf
    ${imgui_SOURCE_DIR}/misc/fonts/Karla-Regular.ttf
    ${imgui_SOURCE_DIR}/misc/fonts/Cousine-Regular.ttf
    ${imgui_SOURCE_DIR}/misc/fonts/ProggyTiny.ttf
)

add_custom_command(TARGET pasgen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:pasgen>/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PASGEN_BUNDLED_FONTS} "$<TARGET_FILE_DIR:pasgen>/fonts"
)

target_include_directories(pasgen PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${ARGON2_INCLUDE_DIRS}
    ${pfd_SOURCE_DIR}
)

target_link_libraries(pasgen PRIVATE
    imgui_lib
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    ${ARGON2_LIBRARIES}
)

if(TARGET SDL2::SDL2)
    target_link_libraries(pasgen PRIVATE SDL2::SDL2)
    if(WIN32 AND TARGET SDL2::SDL2main)
        target_link_libraries(pasgen PRIVATE SDL2::SDL2main)
    endif()
elseif(TARGET SDL2-static)
    target_link_libraries(pasgen PRIVATE SDL2-static)
elseif(DEFINED SDL2_LIBRARIES)
    target_include_directories(pasgen PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(pasgen PRIVATE ${SDL2_LIBRARIES})
endif()

if(WIN32)
    target_link_libraries(pasgen PRIVATE opengl32 dwmapi)
elseif(APPLE)
    target_link_libraries(pasgen PRIVATE "-framework OpenGL" "-framework Cocoa")
    target_compile_options(pasgen PRIVATE -Wno-deprecated-declarations)
else()
    target_link_libraries(pasgen PRIVATE OpenGL::GL)
endif()

if(MSVC)
    target_compile_options(pasgen PRIVATE /W3)
else()
    target_compile_options(pasgen PRIVATE -Wall -Wextra)
endif()

include(GNUInstallDirs)
install(TARGETS pasgen RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES ${PASGEN_BUNDLED_FONTS} DESTINATION ${CMAKE_INSTALL_BINDIR}/fonts)
