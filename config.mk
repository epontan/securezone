# securezone version
VERSION = 0.1

# paths
PREFIX = /usr/local

# includes and libs
INCS = -I/usr/include
LIBS = -lc -lcrypt `sdl-config --libs` -lSDL_image 

# flags
CFLAGS = -Wall -Os ${INCS} `sdl-config --cflags` \
		 -D_PATH_PREFIX=\"$(PREFIX)\" \
		 -DVERSION=\"${VERSION}\"
LDFLAGS = ${LIBS}

# compiler and linker
CC = cc
LD = ${CC}
