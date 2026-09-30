#ifndef TIMEOUT_H
#define TIMEOUT_H
/*=========================================================================*\
* Timeout management functions
* LuaSocket toolkit
\*=========================================================================*/
#include "luasocket.h"
#include "common.h"

/* timeout control structure */
typedef struct t_timeout_ {
    double block;          /* maximum time for blocking calls */
    double total;          /* total number of miliseconds for operation */
    double start;          /* time of start of operation */
} t_timeout;
typedef t_timeout *p_timeout;

#ifndef _WIN32
#pragma GCC visibility push(hidden)
#endif

LUASOCKET_API void timeout_init(p_timeout tm, double block, double total);
LUASOCKET_API double timeout_get(p_timeout tm);
LUASOCKET_API double timeout_getstart(p_timeout tm);
LUASOCKET_API double timeout_getretry(p_timeout tm);
LUASOCKET_API p_timeout timeout_markstart(p_timeout tm);

LUASOCKET_API double timeout_gettime(void);

LUASOCKET_API int timeout_open(lua_State *L);

LUASOCKET_API int timeout_meth_settimeout(lua_State *L, p_timeout tm);
LUASOCKET_API int timeout_meth_gettimeout(lua_State *L, p_timeout tm);

#ifndef _WIN32
#pragma GCC visibility pop
#endif

#define timeout_iszero(tm)   ((tm)->block == 0.0)

#endif /* TIMEOUT_H */
