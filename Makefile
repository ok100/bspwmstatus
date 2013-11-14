all: bspwmstatus

bspwmstatus: bspwmstatus.c
	gcc -Wall -Wextra -Os -lmpdclient -o bspwmstatus bspwmstatus.c
	strip bspwmstatus

clean:
	rm bspwmstatus
