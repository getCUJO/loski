local network = require "network"
local tests = require "test.network.utils"

for _, kind in ipairs{"datagram", "connection", "listen"} do
	local cases = {
		["unreachable"] = network.address("255.255.255.255",
		                                  tests.FreeAddress.port)
	}
	if not tests.IsWindows then
		cases["access denied"] = tests.DeniedAddress
	end
	if kind ~= "datagram" then
		cases["unavailable"] = tests.UsedAddress
	end
	for expected, addr in pairs(cases) do
		local socket = tests.testcreatesocket(kind)
		tests.testoptions(socket, kind)

		-- succ [, errmsg] = socket:bind(address)
		tests.testerrmsg(expected, socket:bind(addr))

		tests.testoptions(socket, kind)
		tests.testclose(socket)
	end
end

for _, kind in ipairs{"datagram", "connection", "listen"} do
	local socket = tests.testcreatesocket(kind)
	tests.testoptions(socket, kind)

	-- succ [, errmsg] = socket:bind(address)
	assert(socket:bind(tests.FreeAddress) == true)
	tests.testerror("invalid operation", socket.bind, socket, tests.LocalAddress)

	-- address [, errmsg] = socket:getaddress(address, [site])
	local addr = socket:getaddress(network.address())
	assert(addr == tests.FreeAddress)
	local addr = socket:getaddress(network.address(), "local")
	assert(addr == tests.FreeAddress)
	tests.testerrmsg("closed", socket:getaddress(addr, "peer"))

	tests.testoptions(socket, kind)
	tests.testclose(socket)
end

-- TODO: Find out the correct way to test is 'reuseaddr' works.
--       The following only works is UsedAddress.port is binded using different
--       hosts.
--for _, kind in ipairs{"datagram", "connection", "listen"} do
--	local socket = tests.testcreatesocket(kind)
--	assert(socket:setoption("reuseaddr", true) == true)
--	assert(socket:bind(tests.UsedAddress) == true)
--	tests.testclose(socket)
--end
