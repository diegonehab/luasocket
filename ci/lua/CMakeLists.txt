# GitHub Actions ships CMake 3.18.0:
#  * https://github.com/actions/virtual-environments/blob/main/images/win/Windows2016-Readme.md#tools
#  * https://github.com/actions/virtual-environments/blob/main/images/win/Windows2016-Readme.md#tools
cmake_minimum_required(VERSION 3.14.0)

set(LUA_VERSION "5.4.0" CACHE STRING "Lua version to be built")

project(lua
  VERSION ${LUA_VERSION}
  HOMEPAGE_URL "https://www.lua.org/"
  LANGUAGES C)

include(GNUInstallDirs)

set(LUA_ARCHIVE_BASE "lua-${LUA_VERSION}")
set(LUA_ARCHIVE_NAME "${LUA_ARCHIVE_BASE}.tar.gz")
if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${LUA_ARCHIVE_NAME}")
  message("Downloading Lua ${LUA_VERSION}...")
  set(LUA_ARCHIVE_URL "https://www.lua.org/ftp/${LUA_ARCHIVE_NAME}")
  file(DOWNLOAD
    ${LUA_ARCHIVE_URL}
    "${CMAKE_CURRENT_BINARY_DIR}/${LUA_ARCHIVE_NAME}"
    SHOW_PROGRESS
    STATUS LUA_DOWNLOAD_STATUS)
  list(GET LUA_DOWNLOAD_STATUS 0 LUA_DOWNLOAD_STATUS_CODE)
  if(NOT ${LUA_DOWNLOAD_STATUS_CODE} EQUAL 0)
    message(FATAL_ERROR
      "Failed to download Lua source: ${LUA_ARCHIVE_URL}: ${LUA_DOWNLOAD_STATUS}")
  endif()
  execute_process(COMMAND ${CMAKE_COMMAND} -E "tar" "xf" ${LUA_ARCHIVE_NAME}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endif()

string(REGEX REPLACE
  "^([0-9]+)\\.([0-9]+)\\..*$"
  "\\1\\2"
  LUA_DLL_VERSION
  ${LUA_VERSION})

if(${LUA_VERSION} VERSION_LESS "5.5.0")
  add_compile_definitions("LUA_COMPAT_5_3")
elseif(${LUA_VERSION} VERSION_LESS "5.4.0")
  add_compile_definitions("LUA_COMPAT_5_2")
endif()
if(WIN32)
  add_compile_definitions("LUA_BUILD_AS_DLL")
endif()

set(LUA_SOURCE_BASE ${CMAKE_CURRENT_BINARY_DIR}/${LUA_ARCHIVE_BASE}/src)
set(LIBLUA_CORE_SOURCES
  ${LUA_SOURCE_BASE}/lapi.c
  ${LUA_SOURCE_BASE}/lcode.c
  ${LUA_SOURCE_BASE}/lctype.c
  ${LUA_SOURCE_BASE}/ldebug.c
  ${LUA_SOURCE_BASE}/ldo.c
  ${LUA_SOURCE_BASE}/ldump.c
  ${LUA_SOURCE_BASE}/lfunc.c
  ${LUA_SOURCE_BASE}/lgc.c
  ${LUA_SOURCE_BASE}/llex.c
  ${LUA_SOURCE_BASE}/lmem.c
  ${LUA_SOURCE_BASE}/lobject.c
  ${LUA_SOURCE_BASE}/lopcodes.c
  ${LUA_SOURCE_BASE}/lparser.c
  ${LUA_SOURCE_BASE}/lstate.c
  ${LUA_SOURCE_BASE}/lstring.c
  ${LUA_SOURCE_BASE}/ltable.c
  ${LUA_SOURCE_BASE}/ltm.c
  ${LUA_SOURCE_BASE}/lundump.c
  ${LUA_SOURCE_BASE}/lvm.c
  ${LUA_SOURCE_BASE}/lzio.c)
set(LIBLUA_LIB_SOURCES
  ${LUA_SOURCE_BASE}/lauxlib.c
  ${LUA_SOURCE_BASE}/lbaselib.c
  ${LUA_SOURCE_BASE}/lcorolib.c
  ${LUA_SOURCE_BASE}/ldblib.c
  ${LUA_SOURCE_BASE}/liolib.c
  ${LUA_SOURCE_BASE}/lmathlib.c
  ${LUA_SOURCE_BASE}/loadlib.c
  ${LUA_SOURCE_BASE}/loslib.c
  ${LUA_SOURCE_BASE}/lstrlib.c
  ${LUA_SOURCE_BASE}/ltablib.c
  ${LUA_SOURCE_BASE}/lutf8lib.c
  ${LUA_SOURCE_BASE}/linit.c)
set(LIBLUA_SOURCES ${LIBLUA_CORE_SOURCES} ${LIBLUA_LIB_SOURCES})
add_library(liblua_shared SHARED ${LIBLUA_SOURCES})
if(NOT WIN32)
  add_library(liblua_static STATIC ${LIBLUA_SOURCES})
endif()
if(NOT WIN32)
  target_link_libraries(liblua_shared m)
  target_link_libraries(liblua_static m)
endif()
if(WIN32)
  set_target_properties(liblua_shared PROPERTIES
    OUTPUT_NAME "lua${LUA_DLL_VERSION}")
else()
  set_target_properties(liblua_shared
    PROPERTIES OUTPUT_NAME "lua")
  set_target_properties(liblua_static
    PROPERTIES OUTPUT_NAME "lua")
endif()
install(TARGETS liblua_shared
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(NOT WIN32)
  install(TARGETS liblua_static
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(MSVC)
  install(FILES $<TARGET_PDB_FILE:liblua_shared>
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    OPTIONAL)
endif()

set(LUA_SOURCES ${LUA_SOURCE_BASE}/lua.c)
add_executable(lua ${LUA_SOURCES})
if(WIN32)
  target_link_libraries(lua liblua_shared)
else()
  target_link_libraries(lua liblua_static)
endif()
install(TARGETS lua DESTINATION ${CMAKE_INSTALL_BINDIR})
if(MSVC)
  install(FILES $<TARGET_PDB_FILE:lua>
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    OPTIONAL)
endif()

set(LUAC_SOURCES ${LUA_SOURCE_BASE}/luac.c)
if(WIN32)
  add_executable(luac ${LUAC_SOURCES} ${LIBLUA_SOURCES})
else()
  target_link_libraries(luac liblua_static)
endif()
install(TARGETS luac DESTINATION ${CMAKE_INSTALL_BINDIR})
if(MSVC)
  install(FILES $<TARGET_PDB_FILE:luac>
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    OPTIONAL)
endif()

install(FILES
  ${LUA_SOURCE_BASE}/lua.h
  ${LUA_SOURCE_BASE}/luaconf.h
  ${LUA_SOURCE_BASE}/lualib.h
  ${LUA_SOURCE_BASE}/lauxlib.h
  ${LUA_SOURCE_BASE}/lua.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
