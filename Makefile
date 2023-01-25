main.o:
	gcc -Wall -Wextra main.c -o mountain

install: main.o
	cp mountain /bin/mountain

