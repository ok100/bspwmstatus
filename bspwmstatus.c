#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/wireless.h>
#include <mpd/client.h>

#include "config.h"

#define TOTAL_JIFFIES   get_jiffies(7)
#define WORK_JIFFIES    get_jiffies(3)
#define EVENT_BUF_LEN   (1024 * (sizeof(struct inotify_event) + 16))

char wm[1024] = {'\0'};
char status[1024] = {'\0'};
char title[1024] = {'\0'};
long total_jiffies, work_jiffies;

void get_time(char *buf, size_t bufsize)
{
	time_t tm;
	char fmt[bufsize];

	time(&tm);
	snprintf(fmt, bufsize, CLOCK_FORMAT, COLOR1, COLOR2, COLOR1, COLOR2);
	strftime(buf, bufsize, fmt, localtime(&tm));
}

void get_mem(char *buf, size_t bufsize)
{
	FILE *fp;
	float total, free, buffers, cached;

	fp = fopen("/proc/meminfo", "r");
	if(fp != NULL) {
		fscanf(fp, "MemTotal: %f kB\nMemFree: %f kB\nBuffers: %f kB\nCached: %f kB\n",
				&total, &free, &buffers, &cached);
		fclose(fp);
		snprintf(buf, bufsize, "%sMem %s%.2f", COLOR1, COLOR2,
				(total - free - buffers - cached) / total);
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
		snprintf(buf, bufsize, "%s%s %s%.2f", COLOR1, ac ? "Ac" : "Bat", COLOR2, now / full);
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

void get_cpu(char *buf, size_t bufsize)
{
	long work_over_period, total_over_period;
	float cpu;
	
	work_over_period = WORK_JIFFIES - work_jiffies;
	total_over_period = TOTAL_JIFFIES - total_jiffies;
	if(total_over_period > 0)
		cpu = (float)work_over_period / (float)total_over_period;
	else
		cpu = 0;
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
	if(strncmp(state, "up", 2) == 0)
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
	if(fp != NULL) {
		fscanf(fp, "%s", vol);
		fclose(fp);
	}
	snprintf(buf, bufsize, "%sVol %s%s", COLOR1, COLOR2, fp != NULL ? vol : "N/A");
}

int dzen_strlen(char *str)
{
	int len = 0;
	size_t i;

	for(i = 0; i < strlen(str); i++) {
		if(str[i] == '^' && i + 3 < strlen(str) && str[i + 3] == '(')
			while(str[i] != ')')
				i++;
		else if((str[i] & 0xc0) != 0x80)
			len++;
	}
	return len;
}

void print_bar(void)
{
	size_t i, j;
	int len;

	len = dzen_strlen(wm) + dzen_strlen(status) + 5;
	printf("%s   %s", wm, COLOR_TITLE);
	for(i = 0; i < strlen(title); i++) {
		if((title[i] & 0xc0) == 0x80) {
			putchar(title[i]);
			continue;
		}
		len++;
		if(len + 2 == PANEL_WIDTH && i + 3 < strlen(title)) {
			for(j = 0; j < 3; j++)
				putchar('.');
			len = PANEL_WIDTH;
			break;
		}
		putchar(title[i]);
	}
	while(len < PANEL_WIDTH) {
		len++;
		putchar(' ');
	}
	printf("   %s%s\n", COLOR_BG, status);
	fflush(stdout);
}

void update_status(void)
{
	char time[128], net[128], mpd[128], vol[64], bat[64], cpu[64], mem[64];

	get_cpu(cpu, sizeof(cpu));
	get_mem(mem, sizeof(mem));
	get_bat(bat, sizeof(bat));
	get_net(net, sizeof(net));
	get_time(time, sizeof(time));
	get_mpd(mpd, sizeof(mpd));
	get_vol(vol, sizeof(vol));

	snprintf(status, sizeof(status), STATUS_FORMAT);
}

void *status_loop(void *ptr)
{
	while(1) {
		update_status();
		print_bar();

		total_jiffies = TOTAL_JIFFIES;
		work_jiffies = WORK_JIFFIES;

		sleep(UPDATE_INTERVAL);
	}
}

void *volume_loop(void *ptr)
{
	int fd, wd;
	char buf[EVENT_BUF_LEN];

	fd = inotify_init();
	if(fd == -1)
		perror("error: failed to initialize inotify");

	wd = inotify_add_watch(fd, VOLUME, IN_MODIFY);
	if(wd == -1)
		perror("error: failed to add inotify watch");

	while(1) {
		read(fd, buf, EVENT_BUF_LEN);
		update_status();
		print_bar();
	}

	inotify_rm_watch(fd, wd);
	close(fd);
}

int main(void)
{
	FILE *fifo;
	pthread_t sl, vl;
	char buf[1024];
	char *d;

	total_jiffies = TOTAL_JIFFIES;
	work_jiffies = WORK_JIFFIES;

	fifo = fopen(PANEL_FIFO, "r");
	if(fifo == NULL) {
		perror("error: failed to open panel fifo");
		return 1;
	}

	if(pthread_create(&sl, NULL, status_loop, NULL) != 0) {
		perror("error: failed to create status thread");
		return 1;
	}
	
	if(pthread_create(&vl, NULL, volume_loop, NULL) != 0) {
		perror("error: failed to create volume thread");
		return 1;
	}

	while(fgets(buf, sizeof(buf), fifo) != NULL) {
		buf[strlen(buf) - 1] = '\0';  // Strip newline character
		switch(*buf) {
		case 'T':
			// Window title
			strncpy(title, buf + 1, sizeof(title));
			break;
		case 'W':
			// BSPWM status
			*wm = '\0';
			strtok(buf, ":");
			while((d = strtok(NULL, ":")) != NULL) {
				if(*d == 'L')
					break;

				// Desktop color
				if(*d == 'u')
					strncat(wm, COLOR_URG, sizeof(wm));
				else
					strncat(wm, isupper(*d) ? COLOR_SEL : COLOR1, sizeof(wm));
				
				// Occupied character
				if(*d == 'o' || *d == 'O' || *d == 'u' || *d == 'U')
					strncat(wm, OCCUPIED, sizeof(wm));
				else
					strncat(wm, " ", sizeof(wm));
				
				// Desktop name
				strncat(wm, d + 1, sizeof(wm));

				strncat(wm, " ", sizeof(wm));
			}
			wm[strlen(wm) - 1] = '\0';
			break;
		}
		print_bar();
	}

	pthread_exit(&sl);
	pthread_exit(&vl);
	fclose(fifo);

	return 0;
}
