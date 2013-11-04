#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <iwlib.h>
#include <mpd/client.h>
#include <X11/Xlib.h>

#define UPDATE_INTERVAL 2
#define CLOCK_FORMAT    "\x01%a\x02%d\x01%b\x02%H:%M"
#define WIRED_DEVICE    "enp3s0"
#define WIRELESS_DEVICE "wlp2s0"
#define BATTERY_FULL    "/sys/class/power_supply/BAT0/energy_full"
#define BATTERY_NOW     "/sys/class/power_supply/BAT0/energy_now"
#define ON_AC           "/sys/class/power_supply/ADP1/online"
#define VOLUME          "/home/ok/.volume"

char *get_time(char *buf, int bufsize)
{
	time_t tm;

	time(&tm);
	strftime(buf, bufsize, CLOCK_FORMAT, localtime(&tm));
	return buf;
}

char *get_mem(char *buf)
{
	FILE *fp;
	float total, free, buffers, cached, mem;

	fp = fopen("/proc/meminfo", "r");
	if(fp != NULL) {
		fscanf(fp, "MemTotal: %f kB\nMemFree: %f kB\nBuffers: %f kB\nCached: %f kB\n",
			&total, &free, &buffers, &cached);
		fclose(fp);
		mem = (total - free - buffers - cached) / total;
	} else
		mem = 0.;
	snprintf(buf, 10, "%cMem\x02%.2f", '\x01', mem);
	return buf;
}

char *get_bat(char *buf)
{
	FILE *fp;
	float now, full;
	int ac;

	fp = fopen(BATTERY_NOW, "r");
	if(fp != NULL) {
		fscanf(fp, "%f", &now);
		fclose(fp);
	} else
		now = 0.;

	fp = fopen(BATTERY_FULL, "r");
	if(fp != NULL) {
		fscanf(fp, "%f", &full);
		fclose(fp);
	} else
		full = 1.;

	fp = fopen(ON_AC, "r");
	if(fp != NULL) {
		fscanf(fp, "%d", &ac);
		fclose(fp);
	} else
		ac = 0;

	if(ac)
		snprintf(buf, 9, "%cAc\x02%.2f", '\x01', now / full);
	else
		snprintf(buf, 10, "%cBat\x02%.2f", '\x01', now / full);
	return buf;
}

long get_jiffies(int n)
{
	FILE *fp;
	int i;
	long j, jiffies;

	fp = fopen("/proc/stat", "r");
	if(fp == NULL)
		return 0;
	
	fscanf(fp, "cpu %ld", &jiffies);
	for(i = 0; i < n - 1; i++) {
		fscanf(fp, "%ld", &j);
		jiffies += j;
	}
	fclose(fp);
	return jiffies;
}

long get_total_jiffies(void)
{
	return get_jiffies(7);
}

long get_work_jiffies(void)
{
	return get_jiffies(3);
}

char *get_cpu(char *buf, long total_jiffies, long work_jiffies)
{
	long work_over_period, total_over_period;
	float cpu;
	
	work_over_period = get_work_jiffies() - work_jiffies;
	total_over_period = get_total_jiffies() - total_jiffies;
	if(total_over_period > 0)
		cpu = (float)work_over_period / (float)total_over_period;
	else
		cpu = 0.;
	snprintf(buf, 10, "%cCpu\x02%.2f", '\x01', cpu);
	return buf;
}

int is_up(char *device)
{
	FILE *fp;
	char fn[32], state[5];

	snprintf(fn, sizeof(fn), "/sys/class/net/%s/operstate", device);
	fp = fopen(fn, "r");
	if(fp == NULL)
		return 0;
	fscanf(fp, "%s", state);
	fclose(fp);
	if(strcmp(state, "up") == 0)
		return 1;
	return 0;
}

char *get_net(char *buf)
{
	int skfd;
	char essid[58];
	struct wireless_info *winfo;
	
	if(is_up(WIRED_DEVICE))
		snprintf(buf, 8, "%cEth\x02On", '\x01');
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
				strncpy(essid, winfo->b.essid, sizeof(essid));
				essid[0] = toupper(essid[0]);
				snprintf(buf, 64, "%c%s\x02%d", '\x01', essid,
					(winfo->stats.qual.qual * 100) / winfo->range.max_qual.qual);
			}
		}
		free(winfo);
	} else
		snprintf(buf, 8, "%cEth\x02No", '\x01');
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
	if(mpd_connection_get_error(conn))
		buf[0] = '\0';
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
			snprintf(buf, 128, "\x01%s\x02%s", artist, title);
			mpd_song_free(song);
		} else
			buf[0] = '\0';
		mpd_response_finish(conn);
	}
	mpd_connection_free(conn);
	return buf;
}

char *get_vol(char *buf)
{
	FILE *fp;
	char *fn;
	char vol[4];
	
	fn = malloc(sizeof(VOLUME));
	snprintf(fn, sizeof(VOLUME), VOLUME);
	fp = fopen(fn, "r");
	free(fn);
	if(fp == NULL) {
		snprintf(buf, 9, "\x01Vol\x02N/A");
		return buf;
	}
	fscanf(fp, "%s", vol);
	fclose(fp);
	snprintf(buf, 9, "\x01Vol\x02%s", vol);
	return buf;
}

int main(void)
{
	Display *dpy;
	Window root;
	char status[512], time[32], net[64], mpd[128], vol[16], bat[16], cpu[16], mem[16];
	long total_jiffies, work_jiffies;

	dpy = XOpenDisplay(NULL);
	if(dpy == NULL) {
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
		get_time(time, sizeof(time));
		get_mpd(mpd);
		get_vol(vol);

		snprintf(status, sizeof(status), "%s %s %s %s %s %s %s", mpd, cpu, mem, bat, net, vol, time);

		total_jiffies = get_total_jiffies();
		work_jiffies = get_work_jiffies();

		XStoreName(dpy, root, status);
		XFlush(dpy);
		sleep(UPDATE_INTERVAL);
	}

	XCloseDisplay(dpy);
	return 0;
}
