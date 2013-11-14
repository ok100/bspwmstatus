#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/wireless.h>
#include <mpd/client.h>

#define COLOR1          "^fg(#6B5A4B)"
#define COLOR2          "^fg(#9A875F)"
#define UPDATE_INTERVAL 2
#define CLOCK_FORMAT    "^fg(#6B5A4B)%a ^fg(#9A875F)%d ^fg(#6B5A4B)%b ^fg(#9A875F)%H:%M"
#define WIRED_DEVICE    "enp3s0"
#define WIRELESS_DEVICE "wlp2s0"
#define BATTERY_FULL    "/sys/class/power_supply/BAT0/energy_full"
#define BATTERY_NOW     "/sys/class/power_supply/BAT0/energy_now"
#define ON_AC           "/sys/class/power_supply/ADP1/online"
#define VOLUME          "/home/ok/.volume"

#define TOTAL_JIFFIES get_jiffies(7)
#define WORK_JIFFIES get_jiffies(3)

void get_time(char *buf, size_t bufsize)
{
	time_t tm;

	time(&tm);
	strftime(buf, bufsize, CLOCK_FORMAT, localtime(&tm));
}

void get_mem(char *buf, size_t bufsize)
{
	FILE *fp;
	float total, free, buffers, cached;

	fp = fopen("/proc/meminfo", "r");
	if(fp != NULL) {
		fscanf(fp, "MemTotal: %f kB\nMemFree: %f kB\nBuffers: %f kB\nCached: %f kB\n",
			&total, &free, &buffers, &cached);
		snprintf(buf, bufsize, "%sMem %s%.2f", COLOR1, COLOR2, (total - free - buffers - cached) / total);
		fclose(fp);
	} else
		snprintf(buf, bufsize, "%sMem %sN/A", COLOR1, COLOR2);
}

void get_bat(char *buf, size_t bufsize)
{
	FILE *f1p, *f2p, *f3p;
	float now, full;
	int ac;

	f1p = fopen(BATTERY_NOW, "r");
	f2p = fopen(BATTERY_FULL, "r");
	f3p = fopen(ON_AC, "r");
	if(f1p == NULL || f2p == NULL || f3p == NULL)
		snprintf(buf, bufsize, "%sBat %sN/A", COLOR1, COLOR2);
	else {
		fscanf(f1p, "%f", &now);
		fscanf(f2p, "%f", &full);
		fscanf(f3p, "%d", &ac);
		if(ac)
			snprintf(buf, bufsize, "%sAc %s%.2f", COLOR1, COLOR2, now / full);
		else
			snprintf(buf, bufsize, "%sBat %s%.2f", COLOR1, COLOR2, now / full);
	}
	if(f1p != NULL)
		fclose(f1p);
	if(f2p != NULL)
		fclose(f2p);
	if(f3p != NULL)
		fclose(f3p);
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

void get_cpu(char *buf, size_t bufsize, long total_jiffies, long work_jiffies)
{
	long work_over_period, total_over_period;
	float cpu;
	
	work_over_period = WORK_JIFFIES - work_jiffies;
	total_over_period = TOTAL_JIFFIES - total_jiffies;
	if(total_over_period > 0)
		cpu = (float)work_over_period / (float)total_over_period;
	else
		cpu = 0.;
	snprintf(buf, bufsize, "%sCpu %s%.2f", COLOR1, COLOR2, cpu);
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

void get_net(char *buf, size_t bufsize)
{
	int sockfd, qual = 0;
	char ssid[IW_ESSID_MAX_SIZE + 1] = "N/A";
	struct iwreq wreq;
	struct iw_statistics stats;
	
	if(is_up(WIRED_DEVICE))
		snprintf(buf, bufsize, "%sEth %sOn", COLOR1, COLOR2);
	else if(is_up(WIRELESS_DEVICE)) {
		memset(&wreq, 0, sizeof(struct iwreq));
		sprintf(wreq.ifr_name, WIRELESS_DEVICE);
		sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		if(sockfd != -1) {
			wreq.u.essid.pointer = ssid;
			wreq.u.essid.length = sizeof(ssid);
			if(!ioctl(sockfd, SIOCGIWESSID, &wreq))
				ssid[0] = toupper(ssid[0]);

			wreq.u.data.pointer = (caddr_t) &stats;
			wreq.u.data.length = sizeof(struct iw_statistics);
			wreq.u.data.flags = 1;
			if(!ioctl(sockfd, SIOCGIWSTATS, &wreq))
				qual = stats.qual.qual;
		}
		snprintf(buf, bufsize, "%s%s %s%d", COLOR1, ssid, COLOR2, qual);
		close(sockfd);
	} else
		snprintf(buf, bufsize, "%sEth %sNo", COLOR1, COLOR2);
}

void get_mpd(char *buf, size_t bufsize)
{
	const char *artist = NULL, *title = NULL;
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
			snprintf(buf, bufsize, "%s%s %s%s", COLOR1, artist, COLOR2, title);
			mpd_song_free(song);
		} else
			buf[0] = '\0';
		mpd_response_finish(conn);
	}
	mpd_connection_free(conn);
}

void get_vol(char *buf, size_t bufsize)
{
	FILE *fp;
	char vol[4];
	
	fp = fopen(VOLUME, "r");
	if(fp == NULL)
		snprintf(buf, bufsize, "%sVol %sN/A", COLOR1, COLOR2);
	else {
		fscanf(fp, "%s", vol);
		fclose(fp);
		snprintf(buf, bufsize, "%sVol %s%s", COLOR1, COLOR2, vol);
	}
}

int main(void)
{
	char time[67], net[128], mpd[128], vol[34], bat[34], cpu[34], mem[34];
	long total_jiffies, work_jiffies;

	total_jiffies = TOTAL_JIFFIES;
	work_jiffies = WORK_JIFFIES;
	
	while(1) {
		get_cpu(cpu, sizeof(cpu), total_jiffies, work_jiffies);
		get_mem(mem, sizeof(mem));
		get_bat(bat, sizeof(bat));
		get_net(net, sizeof(net));
		get_time(time, sizeof(time));
		get_mpd(mpd, sizeof(mpd));
		get_vol(vol, sizeof(vol));

		fprintf(stdout, "S%s  %s  %s  %s  %s  %s  %s\n", mpd, cpu, mem, bat, net, vol, time);

		total_jiffies = TOTAL_JIFFIES;
		work_jiffies = WORK_JIFFIES;

		fflush(stdout);
		sleep(UPDATE_INTERVAL);
	}

	return 0;
}
