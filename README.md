bspwmstatus
===========

![screenshot](https://raw.github.com/ok100/bspwmstatus/master/screen.png)

Installation
------------

To build bspwmstatus, you will need the following libraries:

* libmpdclient

Edit the `config.h` file to match your setup and run `make`.

Usage
-----
Add the following lines into your bspwm config file (`~/.config/bspwm/bspwmrc``):

	PANEL_HEIGHT=16
	PANEL_FIFO="/tmp/panel-fifo"
	PANEL_FONT="DejaVu Sans Mono:size=9"

	bspc config top_padding $PANEL_HEIGHT

	killall bspwmstatus dzen2 bspc xtitle

	[ -e "$PANEL_FIFO" ] && rm "$PANEL_FIFO"
	mkfifo "$PANEL_FIFO"

	bspc control --subscribe > "$PANEL_FIFO" &
	xtitle -sf 'T%s' > "$PANEL_FIFO" &  # Optional: for window title
	bspwmstatus | dzen2 -fn "$PANEL_FONT" -h $PANEL_HEIGHT -ta l &

**Note:** Volume status is read from file `$HOME/.volume`. To have this file updated
automatically, you can bind included script `volume` to your volume control keys.
