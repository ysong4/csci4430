CC = gcc
LIB1 = -lpthread -lcrypt
LIB2 = -lsocket -lnsl -lpthread -lcrypt

all: myproxy

myproxy: myproxy.c
	@if [ "`uname -s`" = "Linux" ] ; then \
		${CC} -o myproxy myproxy.c ${LIB1}; \
	else \
		${CC} -o myproxy myproxy.c ${LIB2}; \
	fi \

clean:
	rm myproxy