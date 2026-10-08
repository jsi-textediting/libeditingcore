local function check(cond, msg) if not cond then error(msg, 2) end end

-- system
check(system.get_time() > 0, "get_time")
check(system.get_process_id() > 0, "get_process_id")
local dir = "/tmp/ec_smoke_" .. system.get_process_id()
check(system.mkdir(dir), "mkdir")
check(system.get_file_info(dir).type == "dir", "get_file_info dir")
local f = assert(io.open(dir .. "/a.txt", "w")); f:write("hello"); f:close()
check(system.get_file_info(dir .. "/a.txt").size == 5, "size")
local list = system.list_dir(dir)
check(#list == 1 and list[1] == "a.txt", "list_dir")
check(system.absolute_path(dir .. "/../" .. dir:match("[^/]+$")) == dir, "absolute_path")
check(system.path_compare("a", "file", "b", "file"), "path_compare")
check(system.fuzzy_match("hello world", "hw") ~= nil, "fuzzy_match")
check(system.setenv("EC_SMOKE", "1"), "setenv")
check(os.getenv("EC_SMOKE") == "1", "getenv")
check(system.rmdir(dir .. "/a.txt"), "remove file")
check(system.rmdir(dir), "rmdir")
check(system.get_file_info(dir) == nil, "removed")

-- regex
local re = regex.compile("(\\d+)-(\\d+)")
local s, e = regex.cmatch(re, "ab 12-34 cd")
check(s == 4 and e == 9, "regex.cmatch " .. tostring(s) .. " " .. tostring(e))
check(regex.gsub(regex.compile("\\d"), "a1b2", "#") == "a#b#", "regex.gsub")

-- process
local p = process.start({"sh", "-c", "echo hi"})
check(p:wait(process.WAIT_INFINITE) == 0, "process exit")
check((p:read_stdout() or ""):match("^hi"), "process stdout")

-- dirmonitor
local mon = dirmonitor.new()
local tmp = "/tmp/ec_watch_" .. system.get_process_id()
system.mkdir(tmp)
mon:watch(tmp)
local f = assert(io.open(tmp .. "/x", "w")); f:write("x"); f:close()
local deadline = system.get_time() + 3
local got = false
while system.get_time() < deadline and not got do
  mon:check(function() got = true end)
  system.sleep(0.05)
end
check(got, "dirmonitor change")
check(events_seen() > 0, "event hook")
os.remove(tmp .. "/x"); system.rmdir(tmp)
print("ec_smoke: ok")
