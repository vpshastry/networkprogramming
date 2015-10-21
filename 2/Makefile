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

CC = gcc

LIBS = -lresolv -lsocket -lnsl -lpthread\
	/home/courses/cse533/Stevens/unpv13e_solaris2.10/libunp.a\

FLAGS = -g -O2

CFLAGS = ${FLAGS} -I/home/courses/cse533/Stevens/unpv13e_solaris2.10/lib

all: get_ifi_info_plus.o prifinfo_plus.o client server
	${CC} -o prifinfo_plus prifinfo_plus.o get_ifi_info_plus.o ${LIBS}

get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c

prifinfo_plus.o: prifinfo_plus.c
	${CC} ${CFLAGS} -c prifinfo_plus.c


header.o: header.h header.c
	${CC} ${CFLAGS} -c header.c header.h

server: server.o readline.o header.o 
	${CC} ${FLAGS} -o server server.o readline.o header.o get_ifi_info_plus.o ${LIBS}
server.o: server.c header.o
	${CC} ${CFLAGS} -c server.c header.h


client: client.o header.o
	${CC} ${FLAGS} -o client client.o header.o ${LIBS}
client.o: client.c header.o
	${CC} ${CFLAGS} -c client.c header.h


# pick up the thread-safe version of readline.c from directory "threads"

readline.o: /home/courses/cse533/Stevens/unpv13e_solaris2.10/threads/readline.c
	${CC} ${CFLAGS} -c /home/courses/cse533/Stevens/unpv13e_solaris2.10/threads/readline.c


clean:
	rm server server.o client client.o readline.o header.o get_ifi_info_plus.o header.h.gch prifinfo_plus prifinfo_plus.o get_ifi_info_plus.o

