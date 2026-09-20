-- socket.unix, on every platform that builds it.
--
-- socket.unix is required on its own rather than after socket, because on Windows
-- opening the module is what starts Winsock, and requiring socket first would hide
-- a failure to do so.
local unix = require "socket.unix"

-- The sockets are bound in the current directory. sun_path holds 104 bytes on some
-- platforms, so a temporary directory is not always short enough to fit, and the
-- temporary directory a Windows program is given is not one this test can guess
-- from Lua.
local STREAM_PATH = "test-unixstream.sock"
local RECEIVER_PATH = "test-unixdgram-receiver.sock"
local SENDER_PATH = "test-unixdgram-sender.sock"
local TIMEOUT = 10

local windows = package.config:sub(1, 1) == "\\"

-- A socket file outlives the socket bound to it, and bind fails if one is already
-- there, so anything a previous run left behind goes first.
os.remove(STREAM_PATH)
os.remove(RECEIVER_PATH)
os.remove(SENDER_PATH)

-- stream
local server = assert(unix.stream())
server:settimeout(TIMEOUT)
assert(server:bind(STREAM_PATH))
assert(server:listen(1))

local client = assert(unix.stream())
client:settimeout(TIMEOUT)
assert(client:connect(STREAM_PATH))

local peer = assert(server:accept())
peer:settimeout(TIMEOUT)

assert(client:send("hello\n"))
local line = assert(peer:receive())
assert(line == "hello", "server read '" .. tostring(line) .. "'")

assert(peer:send("world\n"))
line = assert(client:receive())
assert(line == "world", "client read '" .. tostring(line) .. "'")

peer:close()
client:close()
server:close()
os.remove(STREAM_PATH)

-- dgram
if windows then
    -- Windows implements the stream sockets only. The module builds there and
    -- reports the address family it cannot open in place of returning a socket.
    local sock, err = unix.dgram()
    assert(sock == nil,
        "socket.unix.dgram opened a socket on Windows: if the platform has grown "
        .. "datagram support, run the rest of this test there too")
    assert(type(err) == "string", "socket.unix.dgram failed without an error message")
else
    local receiver = assert(unix.dgram())
    receiver:settimeout(TIMEOUT)
    assert(receiver:bind(RECEIVER_PATH))

    -- The sender is bound as well as the receiver, because not every platform
    -- lets an unbound datagram socket send.
    local sender = assert(unix.dgram())
    sender:settimeout(TIMEOUT)
    assert(sender:bind(SENDER_PATH))
    assert(sender:sendto("ping", RECEIVER_PATH))

    local data = assert(receiver:receive())
    assert(data == "ping", "receiver read '" .. tostring(data) .. "'")

    sender:close()
    receiver:close()
    os.remove(RECEIVER_PATH)
    os.remove(SENDER_PATH)
end

print("done!")
