#
#   This file is part of Magnum.
#
#   Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019
#             Vladimír Vondruš <mosra@centrum.cz>
#   Copyright © 2018 ShaddyAQN <ShaddyAQN@gmail.com>
#   Copyright © 2018 Tomáš Skřivan <skrivantomas@seznam.cz>
#   Copyright © 2018 Jonathan Hale <squareys@googlemail.com>
#
#   Permission is hereby granted, free of charge, to any person obtaining a
#   copy of this software and associated documentation files (the "Software"),
#   to deal in the Software without restriction, including without limitation
#   the rights to use, copy, modify, merge, publish, distribute, sublicense,
#   and/or sell copies of the Software, and to permit persons to whom the
#   Software is furnished to do so, subject to the following conditions:
#
#   The above copyright notice and this permission notice shall be included
#   in all copies or substantial portions of the Software.
#
#   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#   THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#   DEALINGS IN THE SOFTWARE.
#

find_package(Magnum REQUIRED GL)
find_package(imgui CONFIG REQUIRED)

if(BUILD_STATIC)
    set(MAGNUM_IMGUIINTEGRATION_BUILD_STATIC 1)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configure.h.cmake
               ${CMAKE_CURRENT_BINARY_DIR}/configure.h)

corrade_add_resource(MagnumImGuiIntegrationShaders_RCS resources.conf)
set_target_properties(MagnumImGuiIntegrationShaders_RCS-dependencies PROPERTIES
    FOLDER "Magnum/ImGuiIntegration")

set(MagnumImGuiIntegration_SRCS
    Context.cpp

    ${MagnumImGuiIntegrationShaders_RCS})

set(MagnumImGuiIntegration_HEADERS
    Context.h
    Context.hpp
    Integration.h
    Widgets.h

    visibility.h)

# ImGuiIntegration library
add_library(MagnumImGuiIntegration ${SHARED_OR_STATIC}
    ${MagnumImGuiIntegration_SRCS}
    ${MagnumImGuiIntegration_HEADERS})
target_include_directories(MagnumImGuiIntegration PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/src)
set_target_properties(MagnumImGuiIntegration PROPERTIES
    DEBUG_POSTFIX "-d"
    FOLDER "Magnum/ImGuiIntegration")
if(NOT BUILD_STATIC)
    set_target_properties(MagnumImGuiIntegration PROPERTIES VERSION ${MAGNUM_LIBRARY_VERSION} SOVERSION ${MAGNUM_LIBRARY_SOVERSION})
elseif(BUILD_STATIC_PIC)
    set_target_properties(MagnumImGuiIntegration PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()
target_link_libraries(MagnumImGuiIntegration
    PUBLIC Magnum::GL imgui::imgui)

if(BUILD_TESTS)
    add_subdirectory(Test)
endif()

install(TARGETS MagnumImGuiIntegration
    RUNTIME DESTINATION ${MAGNUM_BINARY_INSTALL_DIR}
    LIBRARY DESTINATION ${MAGNUM_LIBRARY_INSTALL_DIR}
    ARCHIVE DESTINATION ${MAGNUM_LIBRARY_INSTALL_DIR})
install(FILES ${MagnumImGuiIntegration_HEADERS} DESTINATION ${MAGNUM_INCLUDE_INSTALL_DIR}/ImGuiIntegration)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/configure.h DESTINATION ${MAGNUM_INCLUDE_INSTALL_DIR}/ImGuiIntegration)

# Magnum Dear ImGui integration target alias for superprojects
add_library(MagnumIntegration::ImGui ALIAS MagnumImGuiIntegration)
