LINUX_STEVENS_BASE = /users/cse533/Stevens/unpv13e/
SOLARIS_STEVENS_BASE = ../Stevens/unpv13e_solaris2.10/

CC = gcc

LIBS = -lpthread -lm ${LINUX_STEVENS_BASE}/libunp.a\

FLAGS = -g -O2

CFLAGS = ${FLAGS} -I${LINUX_STEVENS_BASE}/lib

all: header.o readline.o prhwaddrs.o get_hw_addrs.o tour arp myclient

install: all
	./scripts/stop_app.sh tour
	./scripts/stop_app.sh arp
	~/cse533/deploy_app tour arp myclient

run: all
	./scripts/run_app.sh arp
	./scripts/run_app.sh tour
stop:
	./scripts/stop_app.sh tour
	./scripts/stop_app.sh arp

debug-install:
	~/cse533/deploy_app *.c *.h

## Packed

myclient: myclient.o
	${CC} ${FLAGS} -o myclient myclient.o readline.o header.o get_hw_addrs.o ${LIBS}
myclient.o: myclient.c
	${CC} ${CFLAGS} -c myclient.c header.h

get_hw_addrs.o: get_hw_addrs.c
	${CC} ${CFLAGS} -c get_hw_addrs.c

prhwaddrs.o: prhwaddrs.c
	${CC} ${CFLAGS} -c prhwaddrs.c
## End packed.

header.o: header.h header.c
	${CC} ${CFLAGS} -c header.c header.h

tour: tour.o readline.o header.o get_hw_addrs.o prhwaddrs.o
	${CC} ${FLAGS} -o tour tour.o readline.o prhwaddrs.o get_hw_addrs.o header.o ${LIBS}
tour.o: tour.c header.h
	${CC} ${CFLAGS} -c tour.c header.h


arp: arp.o header.o prhwaddrs.o get_hw_addrs.o
	${CC} ${FLAGS} -o arp arp.o prhwaddrs.o get_hw_addrs.o header.o ${LIBS}
arp.o: arp.c header.h
	${CC} ${CFLAGS} -c arp.c header.h

# pick up the thread-safe version of readline.c from directory "threads"

readline.o: ${LINUX_STEVENS_BASE}/threads/readline.c
	${CC} ${CFLAGS} -c ${LINUX_STEVENS_BASE}/threads/readline.c

clean:
	rm tour tour.o arp arp.o readline.o header.o \
	    prhwaddrs.o get_hw_addrs.o header.h.gch myclient.o myclient

