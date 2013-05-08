all: dwmstatus

dwmstatus: dwmstatus.c
	gcc -Wall -Wextra -Os -lasound -liw -lmpdclient -lX11 -o dwmstatus dwmstatus.c

clean:
	rm dwmstatus
