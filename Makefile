# securezone

include config.mk
sinclude config.local.mk

# securezone version
VERSION = 0.2

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

install: all
	@echo installing executable file to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f ${BIN} ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/${BIN}
	@chmod u+s ${DESTDIR}${PREFIX}/bin/${BIN}
	@echo copying image files to ${DESTDIR}${PREFIX}/share/${BIN}
	@mkdir -p ${DESTDIR}${PREFIX}/share/${BIN}
	@cp -f images/*.png ${DESTDIR}${PREFIX}/share/${BIN}

uninstall:
	@echo removing executable file from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/{BIN}
	@echo removing image files from ${DESTDIR}${PREFIX}/share/${BIN}
	@rm -rf ${DESTDIR}${PREFIX}/share/${BIN}

.PHONY: all options clean dist install uninstall
