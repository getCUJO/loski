local event = require "event"
local network = require "network"
local time = require "time"
local memory = require "memory"
local testutils = require "test.utils"

do
	local watcher = assert(event.watcher())
	local adr = assert(network.resolve("localhost", 0)())

	local lst = assert(testutils.testcreatesocket("listen"))
	assert(lst:setoption("blocking", false))
	assert(lst:bind(adr))
	assert(lst:getaddress("this", adr))
	assert(lst:listen())
	assert(watcher:set(lst, "r"))
	testutils.testerrmsg("unfulfilled", watcher:wait(0))
	local start = time.now()
	testutils.testerrmsg("unfulfilled", watcher:wait(.5))
	assert(time.now() - start > .5)

	local cnt = assert(testutils.testcreatesocket("stream"))
	assert(cnt:setoption("blocking", false))
	testutils.testerrmsg("unfulfilled", cnt.connect(cnt, adr))
	assert(watcher:set(cnt, "w"))
	local events = assert(watcher:wait(0))
	assert(events[cnt] == "w")
	assert(events[lst] == "r")
	assert(cnt:connect(adr))
	assert(watcher:set(cnt, "r"))
	local srv = assert(lst:accept())
	assert(srv:getdomain() == lst:getdomain())
	assert(srv:setoption("blocking", false))
	assert(watcher:set(srv, "r"))
	testutils.testerrmsg("unfulfilled", watcher:wait(0))

	assert(cnt:send("Hello!"))
	assert(srv:send("Hi!"))
	local events = assert(watcher:wait(0))
	assert(events[lst] == nil)
	assert(events[cnt] == "r")
	assert(events[srv] == "r")
	testutils.testreceive(cnt, "Hi!")
	testutils.testreceive(srv, "Hello!")
	testutils.testerrmsg("unfulfilled", watcher:wait(0))

	assert(cnt:send("Hi there!"))
	assert(srv:send("Hello back!"))
	local events = assert(watcher:wait(0))
	assert(events[lst] == nil)
	assert(events[cnt] == "r")
	assert(events[srv] == "r")
	testutils.testreceive(cnt, "Hello back!")
	testutils.testreceive(srv, "Hi there!")
	testutils.testerrmsg("unfulfilled", watcher:wait(0))

	assert(watcher:set(cnt, "rw"))
	assert(watcher:set(srv, "rw"))
	assert(cnt:send("OK, bye."))
	assert(srv:send("Good bye."))

	local events = assert(watcher:wait(0))
	assert(events[lst] == nil)
	assert(events[cnt] == "rw")
	assert(events[srv] == "rw")
	testutils.testreceive(cnt, "Good bye.")
	testutils.testreceive(srv, "OK, bye.")
	local events = assert(watcher:wait(0))
	assert(events[lst] == nil)
	assert(events[cnt] == "w")
	assert(events[srv] == "w")

	testutils.testerrmsg("in use", lst:close())
	testutils.testerrmsg("in use", cnt:close())
	testutils.testerrmsg("in use", srv:close())

	assert(watcher:close())
	assert(lst:close())
	assert(cnt:close())
	assert(srv:close())
end

do
	local process = require "process"

	local port = 54322
	local luabin = "lua"
	do
		local i = -1
		while arg[i] ~= nil do
			luabin = arg[i]
			i = i-1
		end
	end

	local server = assert(process.create(luabin, "-e", [[
	local event = require "event"
	local network = require "network"
	local memory = require "memory"

	local addr = assert(network.resolve("localhost", ]]..port..[[)())
	local port = assert(network.socket("listen"))
	assert(port:bind(addr))
	assert(port:listen())

	local watcher = assert(event.watcher())
	assert(watcher:set(port, "r"))
	local count = 0
	local buffer = memory.create(64)
	repeat
		local events = assert(watcher:wait())
		for conn in pairs(events) do
			if conn == port then
				conn = assert(port:accept())
				assert(watcher:set(conn, "r"))
				count = count+1
			else
				local size = assert(conn:receive(buffer))
				local result = assert(load(buffer:unpack("c"..size, 1)))()
				if result ~= nil then
					assert(assert(conn:send(result)) == #result)
				else
					assert(watcher:set(conn, nil))
					assert(conn:close())
					count = count-1
				end
			end
		end
	until count == 0
	assert(watcher:set(port, nil))
	assert(watcher:close())
	assert(port:close())
	]]))

	time.sleep(1)

	local conns = {}
	local addr = assert(network.resolve("localhost", port)())
	for i = 1, 3 do
		conns[i] = assert(network.socket("stream"))
		assert(conns[i]:connect(addr))
	end

	local code = "return _VERSION"
	for i = 3, 1, -1 do
		time.sleep(i)
		local conn = conns[i]
		assert(assert(conn:send(code)) == #code)
		testutils.testreceive(conn, _VERSION)
	end

	for i = 1, 3 do
		time.sleep(i)
		local conn = conns[i]
		assert(conn:send("return"))
		assert(conn:close())
	end
end
