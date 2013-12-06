bspwmstatus
===========

![screenshot](https://raw.github.com/ok100/bspwmstatus/master/screen.png)

Installation
------------

To build bspwmstatus, you will need the following libraries:

* libmpdclient

Edit the source file to match your setup and run *make*.

Usage
-----
Add the following lines into your bspwm config file (*~/.config/bspwm/bspwmrc*):

    [ -e "$PANEL_FIFO" ] && rm "$PANEL_FIFO"
    mkfifo "$PANEL_FIFO"
    
    bspc config top_padding $PANEL_HEIGHT
    
    bspc control --subscribe > "$PANEL_FIFO" &
    bspwmstatus | dzen2 -fn "DejaVu Sans Mono:size=9" -h $PANEL_HEIGHT -ta l &

and make sure you have defined the *PANEL_FIFO* and *PANEL_HEIGHT* environment variables.

*Note:* Volume status is read from file `$HOME/.volume`. To have this file updated
automatically, you can bind the script `volume` to your volume control keys.
