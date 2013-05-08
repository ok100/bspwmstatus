#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include <iwlib.h>
#include <mpd/client.h>
#include <X11/Xlib.h>

#define UPDATE_INTERVAL 2
#define CLOCK_FORMAT "%H:%M"
#define VOLUME "Master"
#define WIRED_DEVICE "enp4s0"
#define WIRELESS_DEVICE "wlan0"

char *get_time(char *buf, int bufsize, char *format)
{
	time_t tm;

	time(&tm);
	strftime(buf, bufsize, format, localtime(&tm));
	return buf;
}

float get_mem(void)
{
	FILE *f;
	float total, free, buffers, cached, mem;

	f = fopen("/proc/meminfo", "r");
	fscanf(f, "MemTotal: %f kB\nMemFree: %f kB\nBuffers: %f kB\nCached: %f kB\n",
		&total, &free, &buffers, &cached);
	fclose(f);
	mem = (total - free - buffers - cached) / total;
	return mem;
}

long get_total_jiffies()
{
	FILE *f;
	long j[7], total;

	f = fopen("/proc/stat", "r");
	fscanf(f, "cpu %ld %ld %ld %ld %ld %ld %ld",
		&j[0], &j[1], &j[2], &j[3], &j[4], &j[5], &j[6]);
	fclose(f);
	total = j[0] + j[1] + j[2] + j[3] + j[4] + j[5] + j[6];
	return total;
}

long get_work_jiffies()
{
	FILE *f;
	long j[3], work;

	f = fopen("/proc/stat", "r");
	fscanf(f, "cpu %ld %ld %ld", &j[0], &j[1], &j[2]);
	fclose(f);
	work = j[0] + j[1] + j[2];
	return work;
}

float get_cpu(long total_jiffies, long work_jiffies)
{
	long total_jiffies_now, work_jiffies_now, work_over_period, total_over_period;
	float cpu;
	
	total_jiffies_now = get_total_jiffies();
	work_jiffies_now = get_work_jiffies();
	work_over_period = work_jiffies_now - work_jiffies;
	total_over_period = total_jiffies_now - total_jiffies;
	if(total_over_period > 0)
		cpu = ((float)work_over_period / (float)total_over_period);
	else
		cpu = 0.0;
	return cpu;
}

int is_up(char *device)
{
	FILE *f;
	char fn[50], state[5];

	sprintf(fn, "/sys/class/net/%s/operstate", device);
	f = fopen(fn, "r");
	if(f != NULL) {
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
		strcpy(buf, "On");
		return buf;
	}
	else if(is_up(WIRELESS_DEVICE)) {
		winfo = malloc(sizeof(struct wireless_info));
		memset(winfo, 0, sizeof(struct wireless_info));

		skfd = iw_sockets_open();
		if (iw_get_basic_config(skfd, WIRELESS_DEVICE, &(winfo->b)) > -1) {
			if (winfo->b.has_essid && winfo->b.essid_on) {
				strcpy(buf, winfo->b.essid);
				return buf;
			}
		}
	}
	strcpy(buf, "No");
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
			sprintf(buf, "%s\x02%s", artist, title);
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
	long vol, min, max;
	int vol_perc, unmuted;
	snd_mixer_t *mixer;
	snd_mixer_elem_t *pcm_mixer, *mas_mixer;
	snd_mixer_selem_id_t *vol_info, *mute_info;

	snd_mixer_open(&mixer, 0);
	snd_mixer_attach(mixer, "default");
	snd_mixer_selem_register(mixer, NULL, NULL);
	snd_mixer_load(mixer);
	snd_mixer_selem_id_malloc(&vol_info);
	snd_mixer_selem_id_malloc(&mute_info);
	snd_mixer_selem_id_set_name(vol_info, VOLUME);
	snd_mixer_selem_id_set_name(mute_info, VOLUME);
	pcm_mixer = snd_mixer_find_selem(mixer, vol_info);
	mas_mixer = snd_mixer_find_selem(mixer, mute_info);
	snd_mixer_selem_get_playback_volume_range((snd_mixer_elem_t *)pcm_mixer, &min, &max);
	snd_mixer_selem_get_playback_volume(pcm_mixer, SND_MIXER_SCHN_MONO, &vol);
	snd_mixer_selem_get_playback_switch(mas_mixer, SND_MIXER_SCHN_MONO, &unmuted);
	if(unmuted) {
		vol_perc = (vol * 100) / max;
		sprintf(buf, "%d", vol_perc);
	}
	else {
		strcpy(buf, "M");
	}
	snd_mixer_selem_id_free(vol_info);
	snd_mixer_selem_id_free(mute_info);
	snd_mixer_close(mixer);

	return buf;
}

int main(void)
{
	Display *dpy;
	Window root;
	char status[200], time[30], net[20], mpd[100], vol[4];
	long total_jiffies, work_jiffies;
	float cpu, mem;

	dpy = XOpenDisplay(NULL);
	if(dpy == NULL) {
		fprintf(stderr, "error: could not open display\n");
		return 1;
	}
	root = XRootWindow(dpy, DefaultScreen(dpy));

	total_jiffies = get_total_jiffies();
	work_jiffies = get_work_jiffies();
	
	while(1) {
		cpu = get_cpu(total_jiffies, work_jiffies);
		mem = get_mem();
		get_net(net);
		get_time(time, sizeof(time), CLOCK_FORMAT);
		get_mpd(mpd);
		get_vol(vol);

		sprintf(status, "\x01%s\x01 Cpu\x02%.2f \x01Mem\x02%.2f \x01Net\x02%s \x01Vol\x02%s \x02%s",
			mpd, cpu, mem, net, vol, time);

		total_jiffies = get_total_jiffies();
		work_jiffies = get_work_jiffies();

		XStoreName(dpy, root, status);
		XFlush(dpy);
		sleep(UPDATE_INTERVAL);
	}

	XCloseDisplay(dpy);
	return 0;
}
