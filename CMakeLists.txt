project(luasocket)
cmake_minimum_required(VERSION 2.8...3.20)
include(CheckIncludeFile)
check_include_file("lua.h" HAVE_LUA)
set(socket_VERSION "3.0-rc1")
set(MIME_VERSION "1.0.3")

include_directories(BEFORE "${PROJECT_SOURCE_DIR}/src/")

# add header files directory
# include_directories()

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 /Ot /MD /W3 /nologo")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wshadow -Wextra -Wimplicit -O2 -ggdb3 -fpic")
endif()


if(WIN32)
  if(${HAVE_LUA})
    set(
      SOURCE
      "src/auxiliar.c"
      "src/buffer.c"
      "src/except.c"
      "src/inet.c"
      "src/io.c"
      "src/options.c"
      "src/select.c"
      "src/tcp.c"
      "src/udp.c"
      "src/wsocket.c"
    )
  else()
    message(FATAL_ERROR "can not found lua.h or lualib.h \n Maybe you can set \"include_directories(xxx/include)\"to solve this problem")  
  endif()
  add_library(socket SHARED "${SOURCE}")
  add_library(mime SHARED "src/mime.c" "src/compat.c")
  set_target_properties(socket PROPERTIES OUTPUT_NAME "socket")
  set_target_properties(mime PROPERTIES OUTPUT_NAME "mime")
elseif(APPLE)
  #  MACOS build
elseif(UNIX)
  #  Linux or Unix build
endif()

set(
  TO_SOCKET_LDIR
  "src/http.lua"
  "src/url.lua"
  "src/tp.lua"
  "src/ftp.lua"
  "src/headers.lua"
  "src/smtp.lua"
)

set(
  TO_TOP_LDIR
  "src/ltn12.lua"
  "src/socket.lua"
  "src/mime.lua"
)

install(TARGETS socket LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/luasocket/socket")
install(FILES ${TO_SOCKET_LDIR} DESTINATION "${CMAKE_INSTALL_PREFIX}/luasocket/socket")
install(FILES ${TO_TOP_LDIR} DESTINATION "${CMAKE_INSTALL_PREFIX}/luasocket")
