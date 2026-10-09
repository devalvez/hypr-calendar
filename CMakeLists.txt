cmake_minimum_required(VERSION 3.27)

project(hypr-calendar
    DESCRIPTION "Calendário mensal no desktop para o Hyprland"
    VERSION 1.0
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(cal_deps REQUIRED IMPORTED_TARGET pangocairo)

# Núcleo de renderização: não depende do Hyprland.
add_library(hypr-calendar-core STATIC src/calendar_core.cpp)
target_include_directories(hypr-calendar-core PUBLIC src)
target_link_libraries(hypr-calendar-core PUBLIC PkgConfig::cal_deps)

# Ferramenta de preview: gera PNG sem precisar do Hyprland.
add_executable(calendar-preview src/preview.cpp)
target_link_libraries(calendar-preview PRIVATE hypr-calendar-core)

# Plugin: só é construído se os headers do Hyprland estiverem instalados.
pkg_check_modules(hypr_deps IMPORTED_TARGET
    hyprland
    libdrm
    libinput
    libudev
    pixman-1
    wayland-server
    xkbcommon
)

if(hypr_deps_FOUND)
    add_library(hypr-calendar SHARED src/main.cpp)
    set_target_properties(hypr-calendar PROPERTIES PREFIX "")
    target_link_libraries(hypr-calendar PRIVATE hypr-calendar-core rt PkgConfig::hypr_deps)
    install(TARGETS hypr-calendar LIBRARY DESTINATION lib)
else()
    message(STATUS "Headers do Hyprland não encontrados: construindo só o calendar-preview.")
endif()

install(TARGETS calendar-preview RUNTIME DESTINATION bin)
