CC = gcc
CFLAGS = -Wall -Werror -pedantic -std=gnu18
LOGIN = ramesh
SUBMITPATH = ~cs537-1/handin/login/P3


all: wsh

wsh: wsh.c wsh.h
	$(CC) $(CFLAGS) $< -o $@

run: wsh
	export PATH="/bin:$$PATH"
	./wsh

pack: wsh.c wsh.h Makefile README.md
	tar -czvf login.tar.gz $^

submit: pack
	cp -r login.tar.gz $(SUBMITPATH)

clean: 
	rm -rf *.o wsh
	
.PHONY: all clean