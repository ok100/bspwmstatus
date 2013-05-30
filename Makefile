all: dwmstatus

dwmstatus: dwmstatus.c
	gcc -Wall -Wextra -Os -liw -lmpdclient -lX11 -o dwmstatus dwmstatus.c
	strip dwmstatus

clean:
	rm dwmstatus
