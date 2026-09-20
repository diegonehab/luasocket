#ifndef UNIX_H
#define UNIX_H
/*=========================================================================*\
* Unix domain object
* LuaSocket toolkit
*
* This module is just an example of how to extend LuaSocket with a new 
* domain.
\*=========================================================================*/
#include "luasocket.h"

#include "buffer.h"
#include "timeout.h"
#include "socket.h"

/* Windows has carried AF_UNIX since Windows 10 1803 and Windows Server 2019,
 * where the SDK gained <afunix.h>
 * Header presence is the check fir AF_UNIX support, and failing that the NTDDI_VERSION */
#ifndef HAVE_WINDOWS_AFUNIX
#ifdef _WIN32
#ifdef __has_include
#if __has_include(<afunix.h>)
#define HAVE_WINDOWS_AFUNIX 1
#endif
#else
#include <sdkddkver.h>
#if defined(NTDDI_WIN10_RS4) && defined(NTDDI_VERSION) \
        && NTDDI_VERSION >= NTDDI_WIN10_RS4
#define HAVE_WINDOWS_AFUNIX 1
#endif
#endif
#endif
#endif

#ifndef HAVE_WINDOWS_AFUNIX
#define HAVE_WINDOWS_AFUNIX 0
#endif

#if defined(_WIN32) && !HAVE_WINDOWS_AFUNIX
#error "socket.unix on Windows needs AF_UNIX, which arrived in Windows 10 1803. \
Build against that SDK or later, or leave the module out of the build."
#endif

typedef struct t_unix_ {
    t_socket sock;
    t_io io;
    t_buffer buf;
    t_timeout tm;
} t_unix;
typedef t_unix *p_unix;

LUASOCKET_API int luaopen_socket_unix(lua_State *L);

#endif /* UNIX_H */
