
include_directories(inc)

set(lib_rp src/log.c
		   src/event_loop.c)

add_library(rp ${lib_rp})

add_subdirectory(test/)
