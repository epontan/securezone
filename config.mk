# paths
PREFIX = /usr/local

# includes and libs
INCS = -I/usr/include
LIBS = -lX11 -lXext -lXinerama -lpam

# flags
CFLAGS = ${DEBUG} -Wall -Os ${INCS} \
		 -DPREFIX=\"$(PREFIX)\" \
		 -DVERSION=\"${VERSION}\"
LDFLAGS = ${DEBUG} ${LIBS}

# compiler and linker
CC = cc
LD = ${CC}
