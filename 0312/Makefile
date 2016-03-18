CC = gcc
LIB1 = -lpthread
LIB2 = -lsocket -lnsl -lpthread
LIB3 = 
LIB4 = -lsocket -lnsl

all: myproxy

myproxy: myproxy.c
	@if [ "`uname -s`" = "Linux" ] ; then \
		${CC} -o myproxy myproxy.c ${LIB1}; \
	else \
		${CC} -o myproxy myproxy.c ${LIB2}; \
	fi \

clean:
	rm myproxy