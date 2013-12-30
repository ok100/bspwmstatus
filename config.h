#define PANEL_FIFO      "/tmp/panel-fifo"
#define PANEL_WIDTH     195
#define UPDATE_INTERVAL 2

#define COLOR1          "^fg(#707880)"
#define COLOR2          "^fg(#ABADAC)"
#define COLOR_SEL       "^fg(#C5C8C6)"
#define COLOR_URG       "^fg(#CC6666)"
#define COLOR_BG        "^bg(#1D1F21)"
#define COLOR_TITLE     "^fg(#C5C8C6)"
#define OCCUPIED        "▘"
#define CLOCK_FORMAT    "^ca(1, gsimplecal)%s%%a %s%%d %s%%b %s%%H:%%M ^ca()"
#define STATUS_FORMAT   "%s  %s  %s  %s  %s  %s  %s", mpd, cpu, mem, bat, net, vol, time

#define WIRED_DEVICE    "enp3s0"
#define WIRELESS_DEVICE "wlp2s0"
#define BATTERY_FULL    "/sys/class/power_supply/BAT0/energy_full"
#define BATTERY_NOW     "/sys/class/power_supply/BAT0/energy_now"
#define ON_AC           "/sys/class/power_supply/ADP1/online"
#define VOLUME          "/home/ok/.volume"
