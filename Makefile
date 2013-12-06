all: bspwmstatus

bspwmstatus: bspwmstatus.c
	gcc -Wall -Wextra -Os -lmpdclient -lpthread -o bspwmstatus bspwmstatus.c

clean:
	rm bspwmstatus
