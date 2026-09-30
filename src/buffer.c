/*=========================================================================*\
* Input/Output interface for Lua programs
* LuaSocket toolkit
\*=========================================================================*/
#include "luasocket.h"
#include "buffer.h"

/*=========================================================================*\
* Internal function prototypes
\*=========================================================================*/
static int recvraw(p_buffer buf, size_t wanted, luaL_Buffer *b);
static int recvline(p_buffer buf, luaL_Buffer *b, size_t budget, int chop);
static int recvall(p_buffer buf, luaL_Buffer *b, size_t budget);
static int buffer_get(p_buffer buf, const char **data, size_t *count, size_t wanted);
static void buffer_skip(p_buffer buf, size_t count);
static int sendraw(p_buffer buf, const char *data, size_t count, size_t *sent);

/* Internal completion code for buffer_meth_receive. err is not confined to
 * the IO_* enum: socket_recv/socket_send (usocket.c/wsocket.c) propagate raw
 * platform errors (POSIX errno, Windows WSA codes) straight through, and
 * those are always positive, so a positive sentinel here could collide with
 * a genuine transport error (e.g. errno 1 == EPERM) and get misreported as
 * "oversized". Chosen negative and outside {IO_DONE, IO_TIMEOUT, IO_CLOSED,
 * IO_UNKNOWN} (0, -1, -2, -3) so it can never collide with anything err
 * legitimately takes.
 * MUST be handled before buf->io->error() is called -- it is not a transport
 * error. */
#define BUF_OVERSIZED (-1000)

/* min and max macros */
#ifndef MIN
#define MIN(x, y) ((x) < (y) ? x : y)
#endif
#ifndef MAX
#define MAX(x, y) ((x) > (y) ? x : y)
#endif

/*=========================================================================*\
* Exported functions
\*=========================================================================*/
/*-------------------------------------------------------------------------*\
* Initializes module
\*-------------------------------------------------------------------------*/
int buffer_open(lua_State *L) {
    (void) L;
    return 0;
}

/*-------------------------------------------------------------------------*\
* Initializes C structure
\*-------------------------------------------------------------------------*/
void buffer_init(p_buffer buf, p_io io, p_timeout tm) {
    buf->first = buf->last = 0;
    buf->io = io;
    buf->tm = tm;
    buf->received = buf->sent = 0;
    buf->birthday = timeout_gettime();
}

/*-------------------------------------------------------------------------*\
* object:getstats() interface
\*-------------------------------------------------------------------------*/
int buffer_meth_getstats(lua_State *L, p_buffer buf) {
    lua_pushnumber(L, (lua_Number) buf->received);
    lua_pushnumber(L, (lua_Number) buf->sent);
    lua_pushnumber(L, timeout_gettime() - buf->birthday);
    return 3;
}

/*-------------------------------------------------------------------------*\
* object:setstats() interface
\*-------------------------------------------------------------------------*/
int buffer_meth_setstats(lua_State *L, p_buffer buf) {
    buf->received = (long) luaL_optnumber(L, 2, (lua_Number) buf->received);
    buf->sent = (long) luaL_optnumber(L, 3, (lua_Number) buf->sent);
    if (lua_isnumber(L, 4)) buf->birthday = timeout_gettime() - lua_tonumber(L, 4);
    lua_pushnumber(L, 1);
    return 1;
}

/*-------------------------------------------------------------------------*\
* object:send() interface
\*-------------------------------------------------------------------------*/
int buffer_meth_send(lua_State *L, p_buffer buf) {
    int top = lua_gettop(L);
    int err = IO_DONE;
    size_t size = 0, sent = 0;
    const char *data = luaL_checklstring(L, 2, &size);
    long start = (long) luaL_optnumber(L, 3, 1);
    long end = (long) luaL_optnumber(L, 4, -1);
    timeout_markstart(buf->tm);
    if (start < 0) start = (long) (size+start+1);
    if (end < 0) end = (long) (size+end+1);
    if (start < 1) start = (long) 1;
    if (end > (long) size) end = (long) size;
    if (start <= end) err = sendraw(buf, data+start-1, end-start+1, &sent);
    /* check if there was an error */
    if (err != IO_DONE) {
        lua_pushnil(L);
        lua_pushstring(L, buf->io->error(buf->io->ctx, err));
        lua_pushnumber(L, (lua_Number) (sent+start-1));
    } else {
        lua_pushnumber(L, (lua_Number) (sent+start-1));
        lua_pushnil(L);
        lua_pushnil(L);
    }
#ifdef LUASOCKET_DEBUG
    /* push time elapsed during operation as the last return value */
    lua_pushnumber(L, timeout_gettime() - timeout_getstart(buf->tm));
#endif
    return lua_gettop(L) - top;
}

/*-------------------------------------------------------------------------*\
* object:receive() interface
\*-------------------------------------------------------------------------*/
int buffer_meth_receive(lua_State *L, p_buffer buf) {
    int err = IO_DONE, top;
    luaL_Buffer b;
    size_t size, wanted = 0, maxsize = 0;
    size_t budget = 0;  /* 0 == unlimited */
    int numeric = lua_isnumber(L, 2);
    const char *part = luaL_optlstring(L, 3, "", &size);
    char recvpat;

    /* ---- validation: must precede timeout_markstart() and any I/O ---- */
    if (numeric) {
        double n = lua_tonumber(L, 2);
        luaL_argcheck(L, n >= 0 && n < (lua_Number) ((size_t) -1), 2,
            "invalid receive pattern");
        wanted = (size_t) n;
    } else {
        const char *p = luaL_optstring(L, 2, "l");
        if (p[0] == '*') p++;
        recvpat = p[0];
        luaL_argcheck(L, recvpat == 'l' || recvpat == 'a' || recvpat == 'L',
                      2, "invalid receive pattern");
    }
    if (!lua_isnoneornil(L, 4)) {
        double m = luaL_checknumber(L, 4);
        luaL_argcheck(L, m >= 1 && m < (lua_Number) ((size_t) -1), 4,
            "maxsize must be a positive number");
        maxsize = (size_t) m;
        luaL_argcheck(L, size < maxsize, 4,
            "prefix length >= maxsize (drain with prefix=\"\" or raise maxsize)");
        if (numeric)
            luaL_argcheck(L, wanted <= maxsize, 4,
                "maxsize smaller than requested byte count");
        budget = maxsize - size;
    }

    timeout_markstart(buf->tm);
    /* make sure we don't confuse buffer stuff with arguments */
    lua_settop(L, 3);
    top = lua_gettop(L);
    /* initialize buffer with optional extra prefix
     * (useful for concatenating previous partial results) */
    luaL_buffinit(L, &b);
    luaL_addlstring(&b, part, size);
    /* receive new patterns */
    if (!numeric) {
        if (recvpat == 'l') err = recvline(buf, &b, budget, 1);
        else if (recvpat == 'L') err = recvline(buf, &b, budget, 0);
        else err = recvall(buf, &b, budget);
    /* get a fixed number of bytes (minus what was already partially
     * received) */
    } else {
        if (size == 0 || wanted > size)
            err = recvraw(buf, wanted-size, &b);
    }
    /* check if there was an error */
    /* luaL_pushresult(&b) must come first (its accumulator lives on the
     * stack), but the partial it produces belongs in slot 3, not 1 -- so
     * both error branches push buffer/error/buffer-copy/nil, then
     * lua_replace the nil into slot 1. */
    if (err == BUF_OVERSIZED) {
        luaL_pushresult(&b);
        lua_pushliteral(L, "oversized");
        lua_pushvalue(L, -2);
        lua_pushnil(L);
        lua_replace(L, -4);
    } else if (err != IO_DONE) {
        luaL_pushresult(&b);
        lua_pushstring(L, buf->io->error(buf->io->ctx, err));
        lua_pushvalue(L, -2);
        lua_pushnil(L);
        lua_replace(L, -4);
    } else {
        luaL_pushresult(&b);
        lua_pushnil(L);
        lua_pushnil(L);
    }
#ifdef LUASOCKET_DEBUG
    /* push time elapsed during operation as the last return value */
    lua_pushnumber(L, timeout_gettime() - timeout_getstart(buf->tm));
#endif
    return lua_gettop(L) - top;
}

/*-------------------------------------------------------------------------*\
* Determines if there is any data in the read buffer
\*-------------------------------------------------------------------------*/
int buffer_isempty(p_buffer buf) {
    return buf->first >= buf->last;
}

/*=========================================================================*\
* Internal functions
\*=========================================================================*/
/*-------------------------------------------------------------------------*\
* Sends a block of data (unbuffered)
\*-------------------------------------------------------------------------*/
#define STEPSIZE 8192
static int sendraw(p_buffer buf, const char *data, size_t count, size_t *sent) {
    p_io io = buf->io;
    p_timeout tm = buf->tm;
    size_t total = 0;
    int err = IO_DONE;
    while (total < count && err == IO_DONE) {
        size_t done = 0;
        size_t step = (count-total <= STEPSIZE)? count-total: STEPSIZE;
        err = io->send(io->ctx, data+total, step, &done, tm);
        total += done;
    }
    *sent = total;
    buf->sent += total;
    return err;
}

/*-------------------------------------------------------------------------*\
* Reads a fixed number of bytes (buffered)
\*-------------------------------------------------------------------------*/
static int recvraw(p_buffer buf, size_t wanted, luaL_Buffer *b) {
    int err = IO_DONE;
    size_t total = 0;
    do {
        size_t count; const char *data;
        err = buffer_get(buf, &data, &count, wanted - total);
        count = MIN(count, wanted - total);
        luaL_addlstring(b, data, count);
        buffer_skip(buf, count);
        total += count;
    } while (total < wanted && err == IO_DONE);
    return err;
}

/*-------------------------------------------------------------------------*\
* Reads everything until the connection is closed (buffered)
* budget == 0 means unlimited; otherwise the number of payload bytes still
* allowed. Completion (connection closed) beats the cap: filling the cap
* exactly and then seeing EOF means the whole stream was received.
\*-------------------------------------------------------------------------*/
static int recvall(p_buffer buf, luaL_Buffer *b, size_t budget) {
    int err = IO_DONE;
    size_t total = 0;
    while (err == IO_DONE) {
        const char *data; size_t count;
        err = buffer_get(buf, &data, &count, BUF_SIZE);
        if (budget && count > budget - total) {   /* strictly more than fits */
            count = budget - total;
            luaL_addlstring(b, data, count);
            buffer_skip(buf, count);
            return BUF_OVERSIZED;
        }
        total += count;
        luaL_addlstring(b, data, count);
        buffer_skip(buf, count);
    }
    if (err == IO_CLOSED) {                        /* completion beats the cap */
        if (total > 0) return IO_DONE;
        else return IO_CLOSED;
    }
    if (budget && total == budget) return BUF_OVERSIZED;
    return err;
}

/*-------------------------------------------------------------------------*\
* Reads a line terminated by a CR LF pair or just by a LF. The CR and LF
* are not returned by the function and are discarded from the buffer
* budget == 0 means unlimited; otherwise the number of payload bytes still
* allowed. The cap test sits before consuming a byte, so a line of exactly
* budget payload bytes succeeds while budget+1 reports oversized. A timeout
* or close with the payload exactly at the cap and no terminator yet also
* resolves to oversized, never to timeout/closed.
* Internal CRs are retained.
\*-------------------------------------------------------------------------*/
static int recvline(p_buffer buf, luaL_Buffer *b, size_t budget, int chop) {
    int err = IO_DONE;
    size_t total = 0;
    char hasprevch = 0;
    char prevch;
    while (err == IO_DONE) {
        size_t count, pos; const char *data;
        err = buffer_get(buf, &data, &count, BUF_SIZE);
        pos = 0;
        while (pos < count && data[pos] != '\n') {
            /* we delay all the characters an iteration so that \r can be skipped */
            if (hasprevch) {
                if (budget && total == budget) {
                    /* leave the offending byte in the buffer for the next call */
                    buffer_skip(buf, pos - 1);
                    return BUF_OVERSIZED;
                }
                luaL_addchar(b, prevch);
                total++;
            }
            prevch = data[pos];
            hasprevch = 1;
            pos++;
        }
        if (hasprevch && (prevch != '\r' || !chop)) {
            hasprevch = 0;
            if (budget && total == budget) {
                /* leave the offending byte in the buffer for the next call */
                buffer_skip(buf, pos - 1);
                return BUF_OVERSIZED;
            }
            luaL_addchar(b, prevch);
            total++;
        }
        if (pos < count) { /* found '\n' */
            if (!chop) {
                if (budget && total == budget) {
                    /* leave the offending byte in the buffer for the next call */
                    buffer_skip(buf, pos);
                    return BUF_OVERSIZED;
                }
                luaL_addchar(b, '\n');
            }
            buffer_skip(buf, pos+1); /* skip '\n' too */
            break; /* we are done */
        } else /* reached the end of the buffer */
            buffer_skip(buf, pos);
    }
    if (err == IO_DONE) return IO_DONE;                   /* '\n' found: success, regardless of total */
    if (budget && total == budget) return BUF_OVERSIZED;  /* stalled/closed exactly at the cap: I1 */
    return err;                                           /* real timeout/closed, below the cap */
}

/*-------------------------------------------------------------------------*\
* Skips a given number of bytes from read buffer. No data is read from the
* transport layer
\*-------------------------------------------------------------------------*/
static void buffer_skip(p_buffer buf, size_t count) {
    buf->received += count;
    buf->first += count;
    if (buffer_isempty(buf))
        buf->first = buf->last = 0;
}

/*-------------------------------------------------------------------------*\
* Return any data available in buffer, or get more data from transport layer
* if buffer is empty. 'wanted' is how many more bytes the caller is still
* after; when it is zero, the transport layer is still consulted (so an
* already-closed connection is still reported), but no more than zero bytes
* are requested from it, so a healthy connection with no data pending can
* never block.
\*-------------------------------------------------------------------------*/
static int buffer_get(p_buffer buf, const char **data, size_t *count, size_t wanted) {
    int err = IO_DONE;
    p_io io = buf->io;
    p_timeout tm = buf->tm;
    if (buffer_isempty(buf)) {
        size_t got;
        err = io->recv(io->ctx, buf->data, wanted == 0 ? 0 : BUF_SIZE, &got, tm);
        buf->first = 0;
        buf->last = got;
    }
    *count = buf->last - buf->first;
    *data = buf->data + buf->first;
    return err;
}
