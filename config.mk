# paths
PREFIX = /usr/local

# debug
#DEBUG = -ggdb -std=c99 -pedantic

# includes and libs
INCS = -I/usr/include
LIBS = -lc -lcrypt `sdl-config --libs` -lSDL_image 

# flags
CFLAGS = ${DEBUG} -Wall -Os ${INCS} `sdl-config --cflags` \
		 -DPREFIX=\"$(PREFIX)\" \
		 -DVERSION=\"${VERSION}\"
LDFLAGS = ${DEBUG} ${LIBS}

# compiler and linker
CC = cc
LD = ${CC}
