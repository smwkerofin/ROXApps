# Generated automatically from Makefile.in by configure.
SHELL = /bin/sh
srcdir = .
PLATFORM_DIR = /home/stephen/Apps/Tail/SunOS-sun4u
.SUFFIXES:
.SUFFIXES: .c .o
ROX_CLIB = rox_run ROX-CLib

CC = gcc
CFLAGS = -g -O2 `gtk-config --cflags` -I. `${ROX_CLIB} --cflags`
LDFLAGS =  `gtk-config --libs` `${ROX_CLIB} --libs`
EXEC = ${PLATFORM_DIR}/${PROG}

############ Things to change for different programs

PROG = Tail

SRCS = tail.c 

OBJECTS = tail.o

############ Things to keep the same

${EXEC}: ${PLATFORM_DIR} ${OBJECTS}
	${CC} -o ${EXEC} ${OBJECTS} ${LDFLAGS}

${PLATFORM_DIR}:
	mkdir ${PLATFORM_DIR}

clean:
	rm -f *.o Makefile.bak

depend:
	makedepend -- $(CFLAGS) -- $(SRCS)

Makefile: Makefile.in config.status
	./config.status

config.h: config.h.in
	./config.status

config.status: configure
	./configure --bindir=${PLATFORM_DIR}

configure: configure.in
	autoconf
