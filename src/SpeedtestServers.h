#ifndef _SPEEDTEST_SERVERS_
#define _SPEEDTEST_SERVERS_

typedef struct speedtestServer
{
	char *url;
	float lat;
	float lon;
	char *name;
	char *country;
	char *sponsor;
	float distance;
	long id;
} SPEEDTESTSERVER_T;
SPEEDTESTSERVER_T **getServers(int *serverCount, char *ignoreServers, float lat, float lon);
char *getServerDownloadUrl(char *serverUrl, int size);
char *getLatencyUrl(char *serverUrl);
#define SERVER_SIZE 5000
#endif
