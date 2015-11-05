local address = require "network.address"
local utils = require "test.utils"

do
	assert(address.type(nil) == nil)
	assert(address.type(false) == nil)
	assert(address.type(true) == nil)
	assert(address.type(3232235776) == nil)
	assert(address.type("192.168.0.1:8080") == nil)
	assert(address.type(table) == nil)
	assert(address.type(print) == nil)
	assert(address.type(coroutine.running()) == nil)
	assert(address.type(io.stdout) == nil)
end

do
	local function checkaddr(a)
		assert(address.type(a) == "ipv4")
		assert(tostring(a) == "192.168.0.1:8080")
		assert(a.type == "ipv4")
		assert(a.port == 8080)
		assert(a.literal == "192.168.0.1")
		assert(a.bytes == "\192\168\000\001")
		utils.testerror("bad argument #2 to '__index' (invalid option 'wrongfield')",
			function () return a.wrongfield end)
		utils.testerror("attempt to index upvalue 'a' (a userdata value)",
			function () a.port = 80 end)
	end

	checkaddr(address.create("192.168.0.1:8080"))
	checkaddr(address.create("192.168.0.1:8080", nil, "uri"))
	checkaddr(address.create("192.168.0.1", 8080, "literal"))
	checkaddr(address.create("\192\168\000\001", 8080, "bytes"))
end

do
	utils.testerror("bad argument #1 to '?' (string expected, got boolean)",
		address.create, true)
	utils.testerror("bad argument #1 to '?' (string expected, got boolean)",
		address.create, true, 8080, "bytes")
	utils.testerror("bad argument #1 to '?' (string expected, got boolean)",
		address.create, true, 8080, "literal")
	utils.testerror("bad argument #2 to '?' (number expected, got string)",
		address.create, "192.168.0.1", "port", "literal")
	utils.testerror("bad argument #2 to '?' (port must be nil)",
		address.create, "192.168.0.1", 8080)
	utils.testerror("bad argument #3 to '?' (invalid option 'number')",
		address.create, 3232235776, 8080, "number")

	utils.testerror("bad argument #1 to '?' (invalid address)",
		address.create, "192.168.0.1")
	utils.testerror("bad argument #1 to '?' (invalid address)",
		address.create, "localhost:8080")
	utils.testerror("bad argument #1 to '?' (invalid address)",
		address.create, "291.168.0.1:8080")
	utils.testerror("bad argument #1 to '?' (invalid port)",
		address.create, "192.168.0.1:65536")
	utils.testerror("bad argument #1 to '?' (invalid port)",
		address.create, "192.168.0.1:-8080")
	utils.testerror("bad argument #1 to '?' (invalid port)",
		address.create, "192.168.0.1:0x1f90")

	utils.testerror("bad argument #1 to '?' (invalid address)",
		address.create, "192.168.0.1", nil, "uri")
	utils.testerror("bad argument #1 to '?' (invalid address)",
		address.create, "localhost:8080", nil, "uri")
	utils.testerror("bad argument #1 to '?' (invalid address)",
		address.create, "291.168.0.1:8080", nil, "uri")
	utils.testerror("bad argument #1 to '?' (invalid port)",
		address.create, "192.168.0.1:65536", nil, "uri")
	utils.testerror("bad argument #1 to '?' (invalid port)",
		address.create, "192.168.0.1:-8080", nil, "uri")
	utils.testerror("bad argument #1 to '?' (invalid port)",
		address.create, "192.168.0.1:0x1f90", nil, "uri")

	utils.testerror("bad argument #1 to '?' (invalid address)",
		address.create, "192.168.0.1", nil, "uri")
	utils.testerror("bad argument #1 to '?' (invalid address)",
		address.create, "localhost", 8080, "literal")
	utils.testerror("bad argument #1 to '?' (invalid address)",
		address.create, "291.168.0.1", 8080, "literal")

	utils.testerror("bad argument #2 to '?' (invalid port)",
		address.create, "192.168.0.1", 65536, "literal")
	utils.testerror("bad argument #2 to '?' (invalid port)",
		address.create, "192.168.0.1", -1, "literal")
	utils.testerror("bad argument #2 to '?' (invalid port)",
		address.create, "\192\168\000\001", 65536, "bytes")
	utils.testerror("bad argument #2 to '?' (invalid port)",
		address.create, "\192\168\000\001", -1, "bytes")

	-- no IPv6 support
	utils.testerror("bad argument #1 to '?' (invalid address)",
		address.create, "[::1]:8080")
	utils.testerror("bad argument #1 to '?' (invalid address)",
		address.create, "::1", 8080, "literal")
	utils.testerror("bad argument #1 to '?' (invalid address)",
		address.create, string.rep("\0", 15).."\1", 8080, "bytes")
end
