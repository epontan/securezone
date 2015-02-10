# securezone

include config.mk
sinclude config.local.mk

# securezone version
VERSION = 1.0

BIN = securezone
SRC = securezone.c
OBJ = ${SRC:.c=.o}

all: options ${BIN}

options:
	@echo ${BIN} build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"
	@echo "LD       = ${LD}"

${BIN}: ${OBJ}
	@echo LD $@
	@${LD} -o $@ ${OBJ} ${LDFLAGS}
	@if [ -z "${DEBUG}" ]; then echo "Stripping $@"; strip $@; fi

${BINDIR}:
	@mkdir -p ${BINDIR}

${OBJ}: config.mk

.c.o:
	@echo CC $@
	@${CC} -c ${CFLAGS} $<

debug: DEBUG = -ggdb
debug: all

test: CFLAGS += -DTEST
test: debug

clean:
	@echo cleaning
	@rm -f ${BIN} ${OBJ} ${BIN}-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p ${BIN}-${VERSION}
	@cp -R images/ COPYING Makefile README config.mk ${SRC} ${BIN}-${VERSION}
	@tar -cf ${BIN}-${VERSION}.tar ${BIN}-${VERSION}
	@gzip ${BIN}-${VERSION}.tar
	@rm -rf ${BIN}-${VERSION}
	@md5sum ${BIN}-${VERSION}.tar.gz

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@install -Dm755 ${BIN} ${DESTDIR}${PREFIX}/bin/${BIN}

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/{BIN}

.PHONY: all test debug options test clean dist install uninstall
