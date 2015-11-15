# This is a sample Makefile which compiles source files named:
# - server.c
# - client.c
# and creating executables: "server", "client".
#
# It uses various standard libraries, and the copy of Stevens'
# library "libunp.a" in ~cse533/Stevens/unpv13e_solaris2.10 .
#
# It also picks up the thread-safe version of "readline.c"
# from Stevens' directory "threads" and uses it when building
# the executable "server".
#
# It is set up, for illustrative purposes, to enable you to use
# the Stevens code in the ~cse533/Stevens/unpv13e_solaris2.10/lib
# subdirectory (where, for example, the file "unp.h" is located)
# without your needing to maintain your own, local copies of that
# code, and without your needing to include such code in the
# submissions of your assignments.
#
# Modify it as needed, and include it with your submission.

LINUX_STEVENS_BASE = ~cse533/Stevens/unpv13e/
SOLARIS_STEVENS_BASE = ../Stevens/unpv13e_solaris2.10/

CC = gcc

LIBS = -lresolv -lsocket -lnsl -lpthread -lm\
	${LINUX_STEVENS_BASE}/libunp.a\

FLAGS = -g -O2

CFLAGS = ${FLAGS} -I${LINUX_STEVENS_BASE}/lib

all: prhwaddrs prhwaddrs.o get_hw_addrs.o rtt.o dg_send_recv.o window.o server client

prhwaddrs: prhwaddrs.o get_hw_addrs.o
	${CC} -o prhwaddrs prhwaddrs.o get_hw_addrs.o ${LIBS}

get_hw_addrs.o: get_hw_addrs.c
	${CC} ${FLAGS} -c get_hw_addrs.c

prhwaddrs.o: prhwaddrs.c
	${CC} ${FLAGS} -c prhwaddrs.c

header.o: header.h header.c
	${CC} ${CFLAGS} -c header.c

server: server.o readline.o header.o get_hw_addrs.o prhwaddrs.o
	${CC} ${FLAGS} -o server server.o readline.o prhwaddrs.o get_hw_addrs.o header.o ${LIBS}
server.o: server.c header.h
	${CC} ${CFLAGS} -c server.c header.h


client: client.o header.o prhwaddrs.o get_hw_addrs.o
	${CC} ${FLAGS} -o client client.o prhwaddrs.o get_hw_addrs.o header.o ${LIBS}
client.o: client.c header.h
	${CC} ${CFLAGS} -c client.c header.h

# pick up the thread-safe version of readline.c from directory "threads"

readline.o: ${LINUX_STEVENS_BASE}/threads/readline.c
	${CC} ${CFLAGS} -c ${LINUX_STEVENS_BASE}/threads/readline.c

clean:
	rm server server.o client client.o readline.o header.o prhwaddrs \
	    prhwaddrs.o get_hw_addrs.o header.h.gch

