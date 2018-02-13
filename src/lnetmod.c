#include "luaosi/lnetlib.h"


#include "luaosi/lauxlib.h"
#include "luaosi/levtlib.h"

#include <assert.h>
#include <string.h>
#include <ctype.h>

#ifndef LOSI_DISABLE_LUAMEMORY
#include <lmemlib.h>
#else
#define luamem_checkstring luaL_checklstring
#endif

#ifdef LOSI_DISABLE_NETDRV
#define DRVUPV	0
#define todrv(L)	NULL
#else
#define DRVUPV	1
#define todrv(L)	((losi_NetDriver *)lua_touserdata(L, \
                                     lua_upvalueindex(DRVUPV)))
#endif


/*****************************************************************************
 * Addresses *****************************************************************
 *****************************************************************************/


static int mem2port (const char *s, const char *e, losi_AddressPort *pn)
{
	lua_Unsigned n = 0;
	if (s == e) return 0;  /* no digit? */
	do {
		char d = *s - '0';
		if (d < 0 || d > 9) return 0;  /* invalid digit */
		n = n * 10 + d;
		if (n > LOSI_ADDRMAXPORT) return 0;  /* value too large */
	} while (++s < e);
	*pn = n;
	return 1;
}

static losi_AddressPort int2port (lua_State *L, lua_Integer n, int idx)
{
	luaL_argcheck(L, 0 <= n && n <= LOSI_ADDRMAXPORT, idx, "invalid port");
	return (losi_AddressPort)n;
}

static int pushnewaddr (lua_State *L,
                        losi_AddressType type,
                        losi_AddressPort port,
                        const char *data, int bin)
{
	losi_NetDriver *drv = todrv(L);
	losi_Address *na = losi_newaddress(L);
	losi_ErrorCode err = losiN_initaddr(drv, na, type);
	if (!err && data) {
		if (bin) err = losiN_setaddrbytes(drv, na, data);
		else err = losiN_setaddrliteral(drv, na, data);
		if (!err && port) err = losiN_setaddrport(drv, na, port);
	}
	return losiL_doresults(L, 1, err);
}

/* address = network.address([data [, port [, format]]]) */
static int net_address (lua_State *L)
{
	losi_AddressType type = LOSI_ADDRTYPE_IPV4;
	losi_AddressPort port = 0;
	const char *data = NULL;
	int bin = 0, n = lua_gettop(L);
	if (n > 0) {
		size_t sz;
		data = luamem_checkstring(L, 1, &sz);
		if (n == 1) {  /* URI format */
			char literal[LOSI_ADDRMAXLITERAL];
			const char *c, *e = data + sz;
			if (data[0] == '[') {
				c = memchr(++data, ']', sz-1);
				luaL_argcheck(L, c && c+1 < e && c[1] == ':', 1, "invalid URI format");
				sz = c - data;
				type = LOSI_ADDRTYPE_IPV6;
				++c;
			} else {
				c = memchr(data, ':', sz);
				luaL_argcheck(L, c, 1, "invalid URI format");
				sz = c - data;
				type = LOSI_ADDRTYPE_IPV4;
			}
			luaL_argcheck(L, sz < LOSI_ADDRMAXLITERAL, 1, "invalid URI format");
			luaL_argcheck(L, mem2port(c+1, e, &port), 1, "invalid port");
			memcpy(literal, data, sz);
			literal[sz] = '\0';
			return pushnewaddr(L, type, port, literal, 0);
		} else {
			const char *mode = luaL_optstring(L, 3, "t");
			port = int2port(L, luaL_checkinteger(L, 2), 2);
			if (mode[0] == 'b' && mode[1] == '\0') {  /* binary format */
				switch (sz) {
					case LOSI_ADDRSIZE_IPV4: type = LOSI_ADDRTYPE_IPV4; break;
					case LOSI_ADDRSIZE_IPV6: type = LOSI_ADDRTYPE_IPV6; break;
					default: return luaL_argerror(L, 1, "invalid binary address");
				}
				bin = 1;
			} else if (mode[0] == 't' && mode[1] == '\0') {  /* literal format */
				type = strchr(data, ':') ? LOSI_ADDRTYPE_IPV6 : LOSI_ADDRTYPE_IPV4;
			} else {
				return luaL_argerror(L, 3, "invalid mode");
			}
		}
	}
	return pushnewaddr(L, type, port, data, bin);
}


#define toaddr(L,i)	((losi_Address *)luaL_checkudata(L, i, LOSI_NETADDRCLS))

/* uri = tostring(address) */
static int addr_tostring (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	losi_Address *na = toaddr(L, 1);
	losi_AddressType type = losiN_getaddrtype(drv, na);
	losi_AddressPort port = losiN_getaddrport(drv, na);
	char b[LOSI_ADDRMAXLITERAL];
	const char *s = losiN_getaddrliteral(drv, na, b);
	if (s) lua_pushfstring(L, type==LOSI_ADDRTYPE_IPV6?"[%s]:%d":"%s:%d",s,port);
	else lua_pushfstring(L, "invalid address type (%d)", type);
	return 1;
}


/* addr1 == addr2 */
static int addr_eq (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	losi_Address *a1 = losi_toaddress(L, 1);
	losi_Address *a2 = losi_toaddress(L, 2);
	if ( a1 && a2 &&
	     (losiN_getaddrtype(drv, a1) == losiN_getaddrtype(drv, a2)) &&
	     (losiN_getaddrport(drv, a1) == losiN_getaddrport(drv, a2)) ) {
		size_t sz;
		const char *b = losiN_getaddrbytes(drv, a1, &sz);
		lua_pushboolean(L, memcmp(b, losiN_getaddrbytes(drv, a2, NULL), sz)==0);
	}
	else lua_pushboolean(L, 0);
	return 1;
}


/*
 * type = address.type
 * literal = address.literal
 * binary = address.binary
 * port = address.port
 */
static int addr_index (lua_State *L)
{
	static const char *const fields[] = {"port","binary","literal","type",NULL};
	losi_NetDriver *drv = todrv(L);
	losi_Address *na = toaddr(L, 1);
	int field = luaL_checkoption(L, 2, NULL, fields);
	switch (field) {
		case 0: {  /* port */
			lua_pushinteger(L, losiN_getaddrport(drv, na));
		} break;
		case 1: {  /* binary */
			size_t sz;
			const char *s = losiN_getaddrbytes(drv, na, &sz);
			if (s) lua_pushlstring(L, s, sz);
			else lua_pushnil(L);
		} break;
		case 2: {  /* literal */
			char b[LOSI_ADDRMAXLITERAL];
			const char *s = losiN_getaddrliteral(drv, na, b);
			lua_pushstring(L, s);
		} break;
		case 3: {  /* type */
			losi_AddressType type = losiN_getaddrtype(drv, na);
			switch (type) {
				case LOSI_ADDRTYPE_IPV4: lua_pushliteral(L, "ipv4"); break;
				case LOSI_ADDRTYPE_IPV6: lua_pushliteral(L, "ipv6"); break;
				default: return luaL_argerror(L, 1, "corrupted data");
			}
		} break;
	}
	return 1;
}


static const char *const AddrTypeName[] = {
	"ipv4",
	"ipv6",
	NULL
};
static const losi_AddressType AddrTypeId[] = {
	LOSI_ADDRTYPE_IPV4,
	LOSI_ADDRTYPE_IPV6
};

/*
 * address.type = type
 * address.literal = literal
 * address.binary = binary
 * address.port = port
 */
static int addr_newindex (lua_State *L)
{
	static const char *const fields[] = {"port","binary","literal","type",NULL};
	losi_NetDriver *drv = todrv(L);
	losi_Address *na = toaddr(L, 1);
	int field = luaL_checkoption(L, 2, NULL, fields);
	losi_ErrorCode err = 0;
	switch (field) {
		case 0: {  /* port */
			err = losiN_setaddrport(drv, na, int2port(L, luaL_checkinteger(L,3), 3));
		} break;
		case 1: {  /* binary */
			size_t sz, esz = 0;
			const char *data = luamem_checkstring(L, 3, &sz);
			losi_AddressType type = losiN_getaddrtype(drv, na);
			switch (type) {
				case LOSI_ADDRTYPE_IPV4: esz = LOSI_ADDRSIZE_IPV4; break;
				case LOSI_ADDRTYPE_IPV6: esz = LOSI_ADDRSIZE_IPV6; break;
				default: return luaL_argerror(L, 1, "corrupted data");
			}
			luaL_argcheck(L, sz == esz, 3, "wrong byte size");
			err = losiN_setaddrbytes(drv, na, data);
		} break;
		case 2: {  /* literal */
			size_t sz;
			const char *data = luaL_checklstring(L, 3, &sz);
			err = losiN_setaddrliteral(drv, na, data);
		} break;
		case 3: {  /* type */
			int i = luaL_checkoption(L, 3, NULL, AddrTypeName);
			losi_AddressType type = losiN_getaddrtype(drv, na);
			losi_AddressType new = AddrTypeId[i];
			if (new != type) err = losiN_initaddr(drv, na, new);
		} break;
	}
	if (!err) return 0;
	losiL_pusherrmsg(L, err);
	return lua_error(L);
}


/*****************************************************************************
 * Sockets *******************************************************************
 *****************************************************************************/


static const char *const SockTypeName[] = {
	"listen",
	"connection",
	"datagram",
	NULL
};
static const losi_SocketType SockTypeId[] = {
	LOSI_SOCKTYPE_LSTN,
	LOSI_SOCKTYPE_CONN,
	LOSI_SOCKTYPE_DGRM
};

/* socket = network.socket(type [, domain]) */
static int net_socket (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	int optsck = luaL_checkoption(L, 1, NULL, SockTypeName);
	int optadr = luaL_checkoption(L, 2, "ipv4", AddrTypeName);
	losi_SocketType type = SockTypeId[optsck];
	losi_AddressType domain = AddrTypeId[optadr];
	losi_Socket *socket = losi_newsocket(L, type);
	losi_ErrorCode err = losiN_initsock(drv, socket, type, domain);
	if (!err) losi_enablesocket(L, -1);
	return losiL_doresults(L, 1, err);
}


#define tolsock(L,c)	((LuaSocket *)luaL_checkinstance(L, 1, \
                                                       losi_SocketClasses[c]))

/* string = tostring(socket) */
static int sck_tostring (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	LuaSocket *ls = tolsock(L, LOSI_SOCKTYPE_SOCK);
	if (ls->refs == 0)
		lua_pushliteral(L, "socket (closed)");
	else
		lua_pushfstring(L, "socket (%d)", losiN_getsockid(drv, &ls->socket));
	return 1;
}


static int endsock (losi_NetDriver *drv, lua_State *L, LuaSocket *ls)
{
	losi_ErrorCode err = losiN_closesock(drv, &ls->socket);
	if (!err) ls->refs = 0;  /* mark socket as closed */
	return losiL_doresults(L, 0, err);
}

/* getmetatable(socket).__gc(socket) */
static int sck_gc (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	LuaSocket *ls = tolsock(L, LOSI_SOCKTYPE_SOCK);
	if (ls->refs != 0) endsock(drv, L, ls);
	return 0;
}


static losi_Socket *tosock (lua_State *L, int cls)
{
	LuaSocket *ls = tolsock(L, cls);
	if (ls->refs == 0) luaL_error(L, "attempt to use a closed socket");
	return &ls->socket;
}

/* succ [, errmsg] = socket:close() */
static int sck_close (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	LuaSocket *ls = tolsock(L, LOSI_SOCKTYPE_SOCK);
	tosock(L, LOSI_SOCKTYPE_SOCK);  /* make sure argument is an open socket */
	if (ls->refs > 1) return losiL_doresults(L, 0, LOSI_ERRINUSE);
	return endsock(drv, L, ls);
}


/* succ [, errmsg] = socket:bind(address) */
static int sck_bind (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	losi_Socket *socket = tosock(L, LOSI_SOCKTYPE_SOCK);
	const losi_Address *addr = toaddr(L, 2);
	losi_ErrorCode err = losiN_bindsock(drv, socket, addr);
	return losiL_doresults(L, 0, err);
}


/* address [, errmsg] = socket:getaddress([site [, address]]) */
static int sck_getaddress (lua_State *L)
{
	static const char *const sites[] = {"this", "peer", NULL};
	losi_NetDriver *drv = todrv(L);
	losi_Socket *socket = tosock(L, LOSI_SOCKTYPE_SOCK);
	int peer = luaL_checkoption(L, 2, "this", sites);
	losi_Address *addr = losi_toaddress(L, 3);
	losi_ErrorCode err;
	if (!addr) {
		lua_settop(L, 2);
		addr = losi_newaddress(L);
	}
	else lua_settop(L, 3);
	err = losiN_getsockaddr(drv, socket, addr, peer);
	return losiL_doresults(L, 1, err);
}


struct SocketOptionInfo {
	const char *name;
	losi_SocketOption option;
};

static struct SocketOptionInfo lst_opts[] = {
	{"blocking", LOSI_SOCKOPT_BLOCKING},
	{"reuseaddr", LOSI_SOCKOPT_REUSEADDR},
	{"dontroute", LOSI_SOCKOPT_DONTROUTE},
	{NULL, 0}
};

static struct SocketOptionInfo cnt_opts[] = {
	{"blocking", LOSI_SOCKOPT_BLOCKING},
	{"reuseaddr", LOSI_SOCKOPT_REUSEADDR},
	{"dontroute", LOSI_SOCKOPT_DONTROUTE},
	{"linger", LOSI_SOCKOPT_LINGER},
	{"keepalive", LOSI_SOCKOPT_KEEPALIVE},
	{"nodelay", LOSI_SOCKOPT_TCPNDELAY},
	{NULL, 0}
};

static struct SocketOptionInfo dgm_opts[] = {
	{"blocking", LOSI_SOCKOPT_BLOCKING},
	{"reuseaddr", LOSI_SOCKOPT_REUSEADDR},
	{"dontroute", LOSI_SOCKOPT_DONTROUTE},
	{"broadcast", LOSI_SOCKOPT_BROADCAST},
	{NULL, 0}
};

static int checksockopt (lua_State *L,
                         int narg,
                         struct SocketOptionInfo opts[])
{
	const char *name = luaL_checkstring(L, narg);
	int i;
	for (i=0; opts[i].name; i++)
		if (strcmp(opts[i].name, name) == 0)
			return opts[i].option;
	return luaL_argerror(L, narg,
	                     lua_pushfstring(L, "invalid option " LUA_QS, name));
}


/* value [, errmsg] = socket:getoption(name) */
static int lst_getoption (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	losi_Socket *socket = tosock(L, LOSI_SOCKTYPE_LSTN);
	losi_SocketOption opt = checksockopt(L, 2, lst_opts);
	int val;
	losi_ErrorCode err = losiN_getsockopt(drv, socket, opt, &val);
	if (err == 0) lua_pushboolean(L, val);
	return losiL_doresults(L, 1, err);
}

static int cnt_getoption (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	losi_Socket *socket = tosock(L, LOSI_SOCKTYPE_CONN);
	losi_SocketOption opt = checksockopt(L, 2, cnt_opts);
	int val;
	losi_ErrorCode err = losiN_getsockopt(drv, socket, opt, &val);
	if (err == 0) {
		if (opt == LOSI_SOCKOPT_LINGER) lua_pushinteger(L, val);
		else                            lua_pushboolean(L, val);
	}
	return losiL_doresults(L, 1, err);
}

static int dgm_getoption (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	losi_Socket *socket = tosock(L, LOSI_SOCKTYPE_DGRM);
	losi_SocketOption opt = checksockopt(L, 2, dgm_opts);
	int val;
	losi_ErrorCode err = losiN_getsockopt(drv, socket, opt, &val);
	if (err == 0) lua_pushboolean(L, val);
	return losiL_doresults(L, 1, err);
}


/* succ [, errmsg] = socket:setoption(name, value) */
static int lst_setoption (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	losi_Socket *socket = tosock(L, LOSI_SOCKTYPE_LSTN);
	losi_SocketOption opt = checksockopt(L, 2, lst_opts);
	int val = lua_toboolean(L, 3);
	losi_ErrorCode err = losiN_setsockopt(drv, socket, opt, val);
	return losiL_doresults(L, 0, err);
}

static int cnt_setoption (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	losi_Socket *socket = tosock(L, LOSI_SOCKTYPE_CONN);
	losi_SocketOption opt = checksockopt(L, 2, cnt_opts);
	int val = (opt == LOSI_SOCKOPT_LINGER) ? luaL_checkinteger(L, 3)
	                                       : lua_toboolean(L, 3);
	losi_ErrorCode err = losiN_setsockopt(drv, socket, opt, val);
	return losiL_doresults(L, 0, err);
}

static int dgm_setoption (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	losi_Socket *socket = tosock(L, LOSI_SOCKTYPE_DGRM);
	losi_SocketOption opt = checksockopt(L, 2, dgm_opts);
	int val = lua_toboolean(L, 3);
	losi_ErrorCode err = losiN_setsockopt(drv, socket, opt, val);
	return losiL_doresults(L, 0, err);
}


/* succ [, errmsg] = socket:connect(address) */
static int str_connect (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	losi_Socket *socket = tosock(L, LOSI_SOCKTYPE_STRM);
	const losi_Address *addr = toaddr(L, 2);
	losi_ErrorCode err = losiN_connectsock(drv, socket, addr);
	return losiL_doresults(L, 0, err);
}


static size_t posrelat (ptrdiff_t pos, size_t len)
{
	if (pos >= 0) return (size_t)pos;
	else if (0u - (size_t)pos > len) return 0;
	else return len - ((size_t)-pos) + 1;
}

static int senddata(lua_State *L, int cls, const losi_Address *addr)
{
	losi_NetDriver *drv = todrv(L);
	losi_Socket *socket = tosock(L, cls);
	size_t sz, sent;
	const char *data = luamem_checkstring(L, 2, &sz);
	size_t start = posrelat(luaL_optinteger(L, 3, 1), sz);
	size_t end = posrelat(luaL_optinteger(L, 4, -1), sz);
	losi_ErrorCode err;
	if (start < 1) start = 1;
	if (end > sz) end = sz;
	sz = end - start + 1;
	data += start - 1;
	err = losiN_sendtosock(drv, socket, data, sz, &sent, addr);
	lua_pushinteger(L, sent);
	return losiL_doresults(L, 1, err);
}

/* sent [, errmsg] = socket:send(data [, i [, j]]) */
static int cnt_send (lua_State *L)
{
	return senddata(L, LOSI_SOCKTYPE_CONN, NULL);
}

/* sent [, errmsg] = socket:send(data [, i [, j [, address]]]) */
static int dgm_send (lua_State *L)
{
	return senddata(L, LOSI_SOCKTYPE_DGRM, losi_toaddress(L, 5));
}


static int recvdata(lua_State *L, int cls, losi_Address *addr)
{
	losi_NetDriver *drv = todrv(L);
	losi_Socket *socket = tosock(L, cls);
	const char *mode = luaL_optstring(L, 3, "");
	char *buf;
	size_t len, sz;
	losi_ErrorCode err;
	losi_SocketRecvFlag flags = 0;
	for (; *mode; ++mode) switch (*mode) {
		case 'p': flags |= LOSI_SOCKRCV_PEEKONLY; break;
		case 'a': flags |= LOSI_SOCKRCV_WAITALL; break;
		default: return luaL_error(L, "unknown mode char (got '%c')", *mode);
	}
#ifndef LOSI_DISABLE_LUAMEMORY
	buf = luamem_tomemory(L, 2, &sz);
	if (buf != NULL) {
		err = losiN_recvfromsock(drv, socket, flags, buf, sz, &len, addr);
		if (!err) lua_pushinteger(L, len);
	} else
#endif
	{
		luaL_Buffer lbuf;
		sz = (size_t)luaL_checkinteger(L, 2);
		luaL_buffinit(L, &lbuf);
		buf = luaL_prepbuffsize(&lbuf, sz);  /* prepare buffer to read all */
		err = losiN_recvfromsock(drv, socket, flags, buf, sz, &len, addr);
		if (!err) {
			luaL_addsize(&lbuf, len);
			luaL_pushresult(&lbuf);  /* close buffer */
		}
	}
	return losiL_doresults(L, 1, err);
}

/* data [, errmsg] = socket:receive(size [, mode]) */
static int cnt_receive (lua_State *L)
{
	return recvdata(L, LOSI_SOCKTYPE_CONN, NULL);
}

/* data [, errmsg] = socket:receive(size [, mode [, address]]) */
static int dgm_receive (lua_State *L)
{
	return recvdata(L, LOSI_SOCKTYPE_DGRM, losi_toaddress(L, 4));
}


/* succ [, errmsg] = socket:shutdown([mode]) */
static int cnt_shutdown (lua_State *L)
{
	static const char *const waynames[] = {"send", "receive", "both", NULL};
	losi_NetDriver *drv = todrv(L);
	losi_Socket *socket = tosock(L, LOSI_SOCKTYPE_CONN);
	int way = luaL_checkoption(L, 2, "both", waynames);
	losi_ErrorCode err = losiN_shutdownsock(drv, socket, way);
	return losiL_doresults(L, 0, err);
}


/* socket [, errmsg] = socket:accept([address]) */
static int lst_accept (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	losi_Socket *socket = tosock(L, LOSI_SOCKTYPE_LSTN);
	losi_Address *addr = losi_toaddress(L, 2);
	losi_Socket *conn = losi_newsocket(L, LOSI_SOCKTYPE_CONN);
	losi_ErrorCode err = losiN_acceptsock(drv, socket, conn, addr);
	if (!err) losi_enablesocket(L, -1);
	return losiL_doresults(L, 1, err);
}


/* succ [, errmsg] = socket:listen([backlog]) */
static int lst_listen (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	losi_Socket *socket = tosock(L, LOSI_SOCKTYPE_LSTN);
	int backlog = luaL_optinteger(L, 2, 32);
	losi_ErrorCode err = losiN_listensock(drv, socket, backlog);
	return losiL_doresults(L, 0, err);
}


/*****************************************************************************
 * Names *********************************************************************
 *****************************************************************************/


#define FOUNDUPV	DRVUPV+1
#define tofound(L) \
	((losi_AddressFound *)lua_touserdata(L, lua_upvalueindex(FOUNDUPV)))

/* [address, socktype, more =] next([address]) */
static int net_nextfound (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	losi_AddressFound *found = tofound(L);
	if (found) {
		int more, i;
		losi_SocketType type;
		losi_Address *addr = losi_toaddress(L, 1);
		if (!addr) {
			lua_settop(L, 0);
			addr = losi_newaddress(L);
		}
		else lua_settop(L, 1);
		more = losiN_getaddrfound(drv, found, addr, &type);
		for (i = 0; SockTypeName[i] && SockTypeId[i] != type; ++i);
		lua_pushstring(L, SockTypeName[i]);
		lua_pushboolean(L, more);
		if (!more) {  /* avoid further attempts to get more results */
			lua_pushnil(L);
			lua_replace(L, lua_upvalueindex(FOUNDUPV));
		}
		return 3;
	}
	return 0;
}

static int net_freefound (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	losi_AddressFound *found = (losi_AddressFound *)lua_touserdata(L, 1);
	losiN_freeaddrfound(drv, found);
	return 0;
}

/* next [, errmsg] = network.resolve (name [, service [, mode]]) */
static int net_resolve (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	const char *nodename = luaL_optstring(L, 1, NULL);
	const char *servname = luaL_optstring(L, 2, NULL);
	const char *mode = luaL_optstring(L, 3, "");
	losi_AddressFindFlag flags = 0;
	losi_AddressFound *found;
	losi_ErrorCode err;
	if (nodename) {
		if (nodename[0] == '*' && nodename[1] == '\0') {
			luaL_argcheck(L, servname, 2, "service must be provided for '*'");
			flags |= LOSI_ADDRFIND_LSTN;
			nodename = NULL;
		}
	}
	else luaL_argcheck(L, servname, 1, "name or service must be provided");
	for (; *mode; ++mode) switch (*mode) {
		case '4': flags |= LOSI_ADDRFIND_IPV4; break;
		case '6': flags |= LOSI_ADDRFIND_IPV6; break;
		case 'm': flags |= LOSI_ADDRFIND_MAP4; break;
		case 'd': flags |= LOSI_ADDRFIND_DGRM; break;
		case 's': flags |= LOSI_ADDRFIND_STRM; break;
		default: return luaL_error(L, "unknown mode char (got '%c')", *mode);
	}
#ifndef LOSI_DISABLE_NETDRV
	lua_pushvalue(L, lua_upvalueindex(DRVUPV)); /* push library state */
#endif
	found = (losi_AddressFound *)luaL_newsentinel(L, sizeof(losi_AddressFound),
	                                                 net_freefound);
	err = losiN_resolveaddr(drv, found, flags, nodename, servname);
	if (err) luaL_cancelsentinel(L);
	else lua_pushcclosure(L, net_nextfound, FOUNDUPV);
	return losiL_doresults(L, 1, err);
}

static char *incbuf (lua_State *L, size_t *sz)
{
	lua_pop(L, 1);  /* remove old buffer */
	*sz += LOSI_NETNAMEBUFSZ;
	return (char *)lua_newuserdata(L, *sz);
}

/* name [, service] = network.getname (data [, mode]) */
static int net_getname (lua_State *L)
{
	losi_NetDriver *drv = todrv(L);
	char buffer[LOSI_NETNAMEBUFSZ];
	size_t sz = LOSI_NETNAMEBUFSZ;
	char *buf = buffer;
	losi_ErrorCode err;
	int ltype = lua_type(L, 1);
	lua_settop(L, 2);  /* discard any extra parameters */
	lua_pushnil(L);  /* simulate the initial buffer on the stack */
	if (ltype == LUA_TSTRING) {
		do {
			err = losiN_getcanonical(drv, lua_tostring(L, 1), buf, sz);
			if (!err) {
				lua_pushstring(L, buf);
				return 1;
			} else if ((err == LOSI_ERRTOOMUCH) && (buf = incbuf(L, &sz))) {
				err = 0;
			}
		} while (!err);
	} else {
		losi_Address *na;
		losi_AddressNameFlag flags = 0;
		const char *mode = luaL_optstring(L, 2, "");
		for (; *mode; ++mode) switch (*mode) {
			case 'l': flags |= LOSI_ADDRNAME_LOCAL; break;
			case 'd': flags |= LOSI_ADDRNAME_DGRM; break;
			default: return luaL_error(L, "unknown mode char (got '%c')", *mode);
		}
		if (ltype == LUA_TNUMBER) {
			losi_AddressPort port = int2port(L, luaL_checkinteger(L, 1), 1);
			na = losi_newaddress(L);
			if ( !(err = losiN_initaddr(drv, na, LOSI_ADDRTYPE_IPV4)) &&
			     !(err = losiN_setaddrport(drv, na, port)) ) {
				do {
					err = losiN_getaddrnames(drv, na, flags, NULL, 0, buf, sz);
					if (!err) {
						lua_pushstring(L, buf);
						return 1;
					} else if ((err == LOSI_ERRTOOMUCH) && (buf = incbuf(L, &sz))) {
						err = 0;
					}
				} while (!err);
			}
		} else {
			na = toaddr(L, 1);
			do {
				size_t ssz = sz/4;
				size_t nsz = sz-ssz;
				char *sbuf = buf+nsz;
				err = losiN_getaddrnames(drv, na, flags, buf, nsz, sbuf, ssz);
				if (!err) {
					lua_pushstring(L, buf);
					lua_pushstring(L, sbuf);
					return 2;
				} else if ((err == LOSI_ERRTOOMUCH) && (buf = incbuf(L, &sz))) {
					err = 0;
				}
			} while (!err);
		}
	}
	return losiL_doresults(L, 0, err);
}


/*****************************************************************************
 * Library *******************************************************************
 *****************************************************************************/


static int net_type (lua_State *L) {
	if (lua_isuserdata(L, 1) && lua_getmetatable(L, 1)) {
		luaL_getmetatable(L, LOSI_NETADDRCLS);
		if (lua_rawequal(L, 2, 3)) {
			lua_pushliteral(L, "address");
			return 1;
		}
		lua_pop(L, 1);  /* remove address metatable */
		if (luaL_issubclass(L, losi_SocketClasses[LOSI_SOCKTYPE_SOCK])) {
			lua_pushliteral(L, "socket");
			return 1;
		}
	}
	return 0;
}


static const luaL_Reg addr[] = {
	{"__tostring", addr_tostring},
	{"__eq", addr_eq},
	{"__index", addr_index},
	{"__newindex", addr_newindex},
	{NULL, NULL}
};

static const luaL_Reg sck[] = {
	{"__tostring", sck_tostring},
	{"__gc", sck_gc},
	{"close", sck_close},
	{"bind", sck_bind},
	{"getaddress", sck_getaddress},
	{NULL, NULL}
};

static const luaL_Reg lst[] = {
	{"getoption", lst_getoption},
	{"setoption", lst_setoption},
	{"accept", lst_accept},
	{"listen", lst_listen},
	{NULL, NULL}
};

static const luaL_Reg str[] = {
	{"connect", str_connect},
	{NULL, NULL}
};

static const luaL_Reg cnt[] = {
	{"getoption", cnt_getoption},
	{"setoption", cnt_setoption},
	{"send", cnt_send},
	{"receive", cnt_receive},
	{"shutdown", cnt_shutdown},
	{NULL, NULL}
};

static const luaL_Reg dgm[] = {
	{"getoption", dgm_getoption},
	{"setoption", dgm_setoption},
	{"send", dgm_send},
	{"receive", dgm_receive},
	{NULL, NULL}
};

static const luaL_Reg lib[] = {
	{"address", net_address},
	{"getname", net_getname},
	{"resolve", net_resolve},
	{"socket", net_socket},
	{"type", net_type},
	{NULL, NULL}
};

#ifndef LOSI_DISABLE_NETDRV
static int lfreedrv (lua_State *L)
{
	losi_NetDriver *drv = (losi_NetDriver *)lua_touserdata(L, 1);
	losiN_freedrv(drv);
	return 0;
}
#endif

LUAMOD_API int luaopen_network (lua_State *L)
{
#ifndef LOSI_DISABLE_NETDRV
	/* create sentinel */
	losi_NetDriver *drv;
	losi_ErrorCode err;
	lua_settop(L, 0);  /* dicard any arguments */
	drv = (losi_NetDriver *)luaL_newsentinel(L, sizeof(losi_NetDriver),
	                                            lfreedrv);
	/* initialize library */
	err = losiN_initdrv(drv);
	if (err) {
		luaL_cancelsentinel(L);
		losiL_pusherrmsg(L, err);
		return lua_error(L);
	}
#define pushsentinel(L)	lua_pushvalue(L, 1)
#else
#define pushsentinel(L)	((void)L)
#endif
	/* create address class */
	pushsentinel(L);
	luaL_newclass(L, LOSI_NETADDRCLS, NULL, addr, DRVUPV);
	lua_pop(L, 1);  /* remove new class */
	/* create abstract base socket class */
	pushsentinel(L);
	luaL_newclass(L, losi_SocketClasses[LOSI_SOCKTYPE_SOCK], NULL, sck, DRVUPV);
	lua_pop(L, 1);  /* remove new class */
	/* create TCP listening socket class */
	pushsentinel(L);
	luaL_newclass(L, losi_SocketClasses[LOSI_SOCKTYPE_LSTN],
	                 losi_SocketClasses[LOSI_SOCKTYPE_SOCK], lst, DRVUPV);
	lua_pushliteral(L, "listen");
	lua_setfield(L, -2, "type");
	lua_pop(L, 1);  /* remove new class */
	/* create streaming socket class */
	pushsentinel(L);
	luaL_newclass(L, losi_SocketClasses[LOSI_SOCKTYPE_STRM],
	                 losi_SocketClasses[LOSI_SOCKTYPE_SOCK], str, DRVUPV);
	lua_pop(L, 1);  /* remove new class */
	/* create TCP connection socket class */
	pushsentinel(L);
	luaL_newclass(L, losi_SocketClasses[LOSI_SOCKTYPE_CONN],
	                 losi_SocketClasses[LOSI_SOCKTYPE_STRM], cnt, DRVUPV);
	lua_pushliteral(L, "connection");
	lua_setfield(L, -2, "type");
	lua_pop(L, 1);  /* remove new class */
	/* create UDP socket class */
	pushsentinel(L);
	luaL_newclass(L, losi_SocketClasses[LOSI_SOCKTYPE_DGRM],
	                 losi_SocketClasses[LOSI_SOCKTYPE_STRM], dgm, DRVUPV);
	lua_pushliteral(L, "datagram");
	lua_setfield(L, -2, "type");
	lua_pop(L, 1);  /* remove new class */
	/* create library table */
	luaL_newlibtable(L, lib);
	pushsentinel(L);
	luaL_setfuncs(L, lib, DRVUPV);

#ifdef LOSI_ENABLE_SOCKETEVENTS
	losi_defgetevtsrc(L, losi_SocketClasses[LOSI_SOCKTYPE_LSTN],
	                     losiN_getsockevtsrc);
	losi_deffreeevtsrc(L, losi_SocketClasses[LOSI_SOCKTYPE_LSTN],
	                      losiN_freesockevtsrc);
	losi_defgetevtsrc(L, losi_SocketClasses[LOSI_SOCKTYPE_CONN],
	                     losiN_getsockevtsrc);
	losi_deffreeevtsrc(L, losi_SocketClasses[LOSI_SOCKTYPE_CONN],
	                      losiN_freesockevtsrc);
	losi_defgetevtsrc(L, losi_SocketClasses[LOSI_SOCKTYPE_DGRM],
	                     losiN_getsockevtsrc);
	losi_deffreeevtsrc(L, losi_SocketClasses[LOSI_SOCKTYPE_DGRM],
	                      losiN_freesockevtsrc);
#endif
	return 1;
}
