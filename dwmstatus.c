#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <iwlib.h>
#include <mpd/client.h>
#include <X11/Xlib.h>

#define UPDATE_INTERVAL 2
#define CLOCK_FORMAT "\x01%a\x02%d\x01%b\x02%H:%M"
#define WIRED_DEVICE "enp3s0"
#define WIRELESS_DEVICE "wlp2s0"
#define BATTERY_FULL "/sys/class/power_supply/BAT0/energy_full"
#define BATTERY_NOW "/sys/class/power_supply/BAT0/energy_now"
#define ON_AC "/sys/class/power_supply/ADP1/online"
#define TEMP "/sys/class/thermal/thermal_zone0/temp"

char *get_time(char *buf, int bufsize, char *format)
{
	time_t tm;

	time(&tm);
	strftime(buf, bufsize, format, localtime(&tm));
	return buf;
}

char *get_mem(char *buf)
{
	FILE *f;
	float total, free, buffers, cached, mem;

	if((f = fopen("/proc/meminfo", "r")) != NULL) {
		fscanf(f, "MemTotal: %f kB\nMemFree: %f kB\nBuffers: %f kB\nCached: %f kB\n",
			&total, &free, &buffers, &cached);
		fclose(f);
		mem = (total - free - buffers - cached) / total;
	}
	else {
		mem = 0.;
	}
	sprintf(buf, "%cMem\x02%.2f", '\x01', mem);
	return buf;
}

char *get_bat(char *buf)
{
	FILE *f;
	float now, full;
	int ac;

	if((f = fopen(BATTERY_NOW, "r")) != NULL) {
		fscanf(f, "%f", &now);
		fclose(f);
	}
	else {
		now = 0.;
	}
	if((f = fopen(BATTERY_FULL, "r")) != NULL) {
		fscanf(f, "%f", &full);
		fclose(f);
	}
	else {
		full = 0.;
	}
	if((f = fopen(ON_AC, "r")) != NULL) {
		fscanf(f, "%d", &ac);
		fclose(f);
	}
	else {
		ac = 0;
	}
	if(ac)
		sprintf(buf, "%cAc\x02%.2f", '\x01', now / full);
	else
		sprintf(buf, "%cBat\x02%.2f", '\x01', now / full);
	return buf;
}

long get_total_jiffies()
{
	FILE *f;
	long j[7], total;

	if((f = fopen("/proc/stat", "r")) != NULL) {
		fscanf(f, "cpu %ld %ld %ld %ld %ld %ld %ld",
			&j[0], &j[1], &j[2], &j[3], &j[4], &j[5], &j[6]);
		total = j[0] + j[1] + j[2] + j[3] + j[4] + j[5] + j[6];
		fclose(f);
	}
	else {
		total = 0;
	}
	return total;
}

long get_work_jiffies()
{
	FILE *f;
	long j[3], work;

	if((f = fopen("/proc/stat", "r")) != NULL) {
		fscanf(f, "cpu %ld %ld %ld", &j[0], &j[1], &j[2]);
		work = j[0] + j[1] + j[2];
		fclose(f);
	}
	else {
		work = 0;
	}
	return work;
}

char *get_cpu(char *buf, long total_jiffies, long work_jiffies)
{
	long total_jiffies_now, work_jiffies_now;
	long work_over_period, total_over_period;
	float cpu;
	
	total_jiffies_now = get_total_jiffies();
	work_jiffies_now = get_work_jiffies();
	work_over_period = work_jiffies_now - work_jiffies;
	total_over_period = total_jiffies_now - total_jiffies;
	if(total_over_period > 0)
		cpu = ((float)work_over_period / (float)total_over_period);
	else
		cpu = 0.0;
	sprintf(buf, "%cCpu\x02%.2f", '\x01', cpu);
	return buf;
}

int is_up(char *device)
{
	FILE *f;
	char fn[50], state[5];

	sprintf(fn, "/sys/class/net/%s/operstate", device);
	if((f = fopen(fn, "r")) != NULL) {
		fscanf(f, "%s", state);
		fclose(f);
		if(strcmp(state, "up") == 0)
			return 1;
	}
	return 0;
}

char *get_net(char *buf)
{
	int skfd;
	struct wireless_info *winfo;
	
	if(is_up(WIRED_DEVICE)) {
		sprintf(buf, "%cEth\x02On", '\x01');
	}
	else if(is_up(WIRELESS_DEVICE)) {
		winfo = malloc(sizeof(struct wireless_info));
		memset(winfo, 0, sizeof(struct wireless_info));
		skfd = iw_sockets_open();
		if (iw_get_basic_config(skfd, WIRELESS_DEVICE, &(winfo->b)) > -1) {
			if (iw_get_stats(skfd, WIRELESS_DEVICE, &(winfo->stats),
					&winfo->range, winfo->has_range) >= 0)
				winfo->has_stats = 1;
			if (iw_get_range_info(skfd, WIRELESS_DEVICE, &(winfo->range)) >= 0)
				winfo->has_range = 1;
			if (winfo->b.has_essid && winfo->b.essid_on) {
				winfo->b.essid[0] = toupper(winfo->b.essid[0]);
				sprintf(buf, "%c%s\x02%d", '\x01', winfo->b.essid,
					(winfo->stats.qual.qual * 100) / winfo->range.max_qual.qual);
			}
		}
		free(winfo);
	}
	else {
		sprintf(buf, "%cEth\x02No", '\x01');
	}
	return buf;
}

char *get_mpd(char *buf)
{
	const char *artist = NULL;
	const char *title = NULL;
	struct mpd_connection *conn;
	struct mpd_status *status;
	struct mpd_song *song;

	conn = mpd_connection_new(NULL, 0, 30000);
	if(mpd_connection_get_error(conn)) {
		strcpy(buf, "");
	}
	else {
	 	mpd_command_list_begin(conn, true);
		mpd_send_status(conn);
		mpd_send_current_song(conn);
		mpd_command_list_end(conn);
		status = mpd_recv_status(conn);
		if (status && mpd_status_get_state(status) != MPD_STATE_STOP) {
			mpd_response_next(conn);
			song = mpd_recv_song(conn);
			artist = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);
			title = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
			sprintf(buf, "\x01%s\x02%s", artist, title);
			mpd_song_free(song);
		}
		else {
			strcpy(buf, "");
		}
		mpd_response_finish(conn);
	}
	mpd_connection_free(conn);
	return buf;
}

char *get_vol(char *buf)
{
	FILE *f;
	char fn[50];
	char vol[4];
	
	sprintf(fn, "%s/.volume", getenv("HOME"));
	if((f = fopen(fn, "r")) == NULL) {
		sprintf(buf, "N/A");
		return buf;
	}
	fscanf(f, "%s", vol);
	fclose(f);
	sprintf(buf, "\x01Vol\x02%s", vol);
	return buf;
}

int main(void)
{
	Display *dpy;
	Window root;
	char status[512], time[32], net[64], mpd[128], vol[16], bat[16], cpu[16], mem[16];
	long total_jiffies, work_jiffies;

	if((dpy = XOpenDisplay(NULL)) == NULL) {
		fprintf(stderr, "error: could not open display\n");
		return 1;
	}
	root = XRootWindow(dpy, DefaultScreen(dpy));

	total_jiffies = get_total_jiffies();
	work_jiffies = get_work_jiffies();
	
	while(1) {
		get_cpu(cpu, total_jiffies, work_jiffies);
		get_mem(mem);
		get_bat(bat);
		get_net(net);
		get_time(time, sizeof(time), CLOCK_FORMAT);
		get_mpd(mpd);
		get_vol(vol);

		sprintf(status, "%s %s %s %s %s %s %s", mpd, cpu, mem, bat, net, vol, time);

		total_jiffies = get_total_jiffies();
		work_jiffies = get_work_jiffies();

		XStoreName(dpy, root, status);
		XFlush(dpy);
		sleep(UPDATE_INTERVAL);
	}

	XCloseDisplay(dpy);
	return 0;
}
