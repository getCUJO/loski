local network = require "network"
local utils = require "test.network.utils"

do
	local file1 = os.tmpname()
	local file2 = os.tmpname()
	assert(os.remove(file1))
	assert(os.remove(file2))

	local addr1 = network.address("file", file1)
	local addr2 = network.address("file", file2)
	local dgrm1 = utils.testcreatesocket("datagram", "file")
	local dgrm2 = utils.testcreatesocket("datagram", "file")
	assert(dgrm1:bind(addr1))
	assert(dgrm2:bind(addr2))

	assert(dgrm1:send("Hello", 1, -1, addr2))
	local from = network.address("file")
	utils.testreceive(dgrm2, "Hello", "", from)
	assert(from == addr1)

	assert(dgrm2:connect(addr1))
	assert(dgrm2:send("Hi"))
	utils.testreceive(dgrm1, "Hi")

	assert(os.remove(file1))
	assert(os.remove(file2))
end

do
	local file = os.tmpname()
	assert(os.remove(file))

	local addr = network.address("file", file)
	local server = utils.testcreatesocket("listen", "file")
	assert(server:bind(addr))
	assert(server:listen())

	local conn = utils.testcreatesocket("stream", "file")
	assert(conn:connect(addr))

	local accepted = assert(server:accept())
	assert(accepted:getdomain() == server:getdomain())

	assert(conn:send("Hello"))
	utils.testreceive(accepted, "Hello")

	assert(accepted:send("Hi"))
	utils.testreceive(conn, "Hi")

	assert(os.remove(file))
end
