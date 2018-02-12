PROJNAME = losi
LIBNAME = time

NO_DEPEND= YES
USE_LUA52= YES

DEF_FILE= ../etc/tecmake/losi.def
DEFINES= LUA_BUILD_AS_DLL LUA_LIB
INCLUDES= . win32 posix
SRC= \
	win32/timelib.c \
	ltimelib.c \
	losiaux.c
