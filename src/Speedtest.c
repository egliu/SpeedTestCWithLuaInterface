/*
	Main program.

	Michał Obrembski (byku@byku.com.pl)
*/
#include "http.h"
#include "SpeedtestConfig.h"
#include "SpeedtestServers.h"
#include "Speedtest.h"
#include "SpeedtestLatencyTest.h"
#include "SpeedtestDownloadTest.h"
#include "SpeedtestUploadTest.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <lauxlib.h>
#include <lualib.h>

// strdup isnt a C99 function, so we need it to define itself
char *strdup(const char *str)
{
    int n = strlen(str) + 1;
    char *dup = malloc(n);
    if(dup)
    {
        strcpy(dup, str);
    }
    return dup;
}

int sortServers(SPEEDTESTSERVER_T **srv1, SPEEDTESTSERVER_T **srv2)
{
    return((*srv1)->distance - (*srv2)->distance);
}

float getElapsedTime(struct timeval tval_start) {
    struct timeval tval_end, tval_diff;
    gettimeofday(&tval_end, NULL);
    tval_diff.tv_sec = tval_end.tv_sec - tval_start.tv_sec;
    tval_diff.tv_usec = tval_end.tv_usec - tval_start.tv_usec;
    if(tval_diff.tv_usec < 0) {
        --tval_diff.tv_sec;
        tval_diff.tv_usec += 1000000;
    }
    return (float)tval_diff.tv_sec + (float)tval_diff.tv_usec / 1000000;
}

void freeMem()
{
    if (uploadUrl) {
        free(uploadUrl);
        uploadUrl = NULL;
    }
    if (serverList) {
        free(serverList);
        serverList = NULL;
    }
    if (speedTestConfig) {
        if (speedTestConfig->downloadThreadConfig.sizes) {
            free(speedTestConfig->downloadThreadConfig.sizes);
            speedTestConfig->downloadThreadConfig.sizes = NULL;
        }
        if (speedTestConfig->uploadThreadConfig.sizes) {
            free(speedTestConfig->uploadThreadConfig.sizes);
            speedTestConfig->downloadThreadConfig.sizes = NULL;
        }
        free(speedTestConfig);
        speedTestConfig = NULL;
    }
}

int InitSpeedTest(lua_State* L)
{
    freeMem();
    // parameters from lua is not supported yet
    totalToBeTransfered = 0;
    totalDownloadTestCount = 1;
    randomizeBestServers = 0;
    speedTestConfig = NULL;
    uploadUrl = NULL;
    return 1;
}

int GetConfig(lua_State* L)
{
    speedTestConfig = getConfig();
    lua_newtable(L);
    if (NULL == speedTestConfig) {
        freeMem();

        lua_pushstring(L, "errorCode");
        lua_pushnumber(L, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "errorInfo");
        lua_pushstring(L, "Cannot download speedtest.net configuration.");
        lua_settable(L, -3);
    } else if((speedTestConfig->lat == INCORRECTVALUE) &&
        (speedTestConfig->lon == INCORRECTVALUE))
    {
        freeMem();
        lua_pushstring(L, "errorCode");
        lua_pushnumber(L, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "errorInfo");
        lua_pushstring(L, "Fail to get lat and log info.");
        lua_settable(L, -3);
    }else 
    {
        lua_pushstring(L, "errorCode");
        lua_pushnumber(L, 0);
        lua_settable(L, -3);

        lua_pushstring(L, "ip");
        lua_pushstring(L, speedTestConfig->ip);
        lua_settable(L, -3);

        lua_pushstring(L, "isp");
        lua_pushstring(L, speedTestConfig->isp);
        lua_settable(L, -3);

        lua_pushstring(L, "lat");
        lua_pushnumber(L, speedTestConfig->lat);
        lua_settable(L, -3);

        lua_pushstring(L, "lon");
        lua_pushnumber(L, speedTestConfig->lon);
        lua_settable(L, -3);

        lua_pushstring(L, "uploadThreadConfig");
        lua_newtable(L);
        lua_pushstring(L, "threadsCount");
        lua_pushnumber(L, speedTestConfig->uploadThreadConfig.threadsCount);
        lua_settable(L, -3);
        lua_pushstring(L, "sizeLength");
        lua_pushnumber(L, speedTestConfig->uploadThreadConfig.sizeLength);
        lua_settable(L, -3);
        lua_pushstring(L, "count");
        lua_pushnumber(L, speedTestConfig->uploadThreadConfig.count);
        lua_settable(L, -3);
        lua_pushstring(L, "length");
        lua_pushnumber(L, speedTestConfig->uploadThreadConfig.length);
        lua_settable(L, -3);
        lua_settable(L, -3);

        lua_pushstring(L, "downloadThreadConfig");
        lua_newtable(L);
        lua_pushstring(L, "threadsCount");
        lua_pushnumber(L, speedTestConfig->downloadThreadConfig.threadsCount);
        lua_settable(L, -3);
        lua_pushstring(L, "sizeLength");
        lua_pushnumber(L, speedTestConfig->downloadThreadConfig.sizeLength);
        lua_settable(L, -3);
        lua_pushstring(L, "count");
        lua_pushnumber(L, speedTestConfig->downloadThreadConfig.count);
        lua_settable(L, -3);
        lua_pushstring(L, "length");
        lua_pushnumber(L, speedTestConfig->downloadThreadConfig.length);
        lua_settable(L, -3);
        lua_settable(L, -3);
    }
    return 1;
}

int GetBestServer(lua_State* L)
{
    size_t selectedServer = 0;
    float selectedLatency = 0.0;
    lua_newtable(L);
    if (speedTestConfig == NULL)
    {
        freeMem();

        lua_pushstring(L, "errorCode");
        lua_pushnumber(L, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "errorInfo");
        lua_pushstring(L, "speedTestConfig is NULL, please get config firstly.");
        lua_settable(L, -3);
    } else 
    {
        serverList = getServers(&serverCount, speedTestConfig->ignoreServers,
            speedTestConfig->lat, speedTestConfig->lon);
        if ((serverCount == 0) || (NULL == serverList))
        {
            freeMem();
            lua_pushstring(L, "errorCode");
            lua_pushnumber(L, 1);
            lua_settable(L, -3);

            lua_pushstring(L, "errorInfo");
            lua_pushstring(L, "Cannot download any speedtest.net server.");
            lua_settable(L, -3);
        } else
        {
            qsort(serverList, serverCount, sizeof(SPEEDTESTSERVER_T *),
                        (int (*)(const void *,const void *)) sortServers);

            if (randomizeBestServers != 0) {
                srand(time(NULL));
                selectedServer = rand() % randomizeBestServers;
            } else {
                /* Choose the best server based ping latency in the 5 closest servers */
                int latency[5] = {0};
                for(int count=0; count<5; count++) {
                    char *url = getLatencyUrl(serverList[count]->url);
                    if (NULL != url) {
                        /* test latency 3 times*/
                        for(int tCount=0; tCount<3; tCount++) {
                            latency[count] += testLatency(url);
                        }
                        if (count > 0) {
                            if(latency[count] < latency[selectedServer]) {
                                selectedServer = count;
                            }
                        }
                        free(url);
                    }
                }
                selectedLatency = latency[selectedServer] / 3;
            }
            uploadUrl = malloc(sizeof(char) * strlen(serverList[selectedServer]->url) + 1);
            strcpy(uploadUrl, serverList[selectedServer]->url);

            lua_pushstring(L, "errorCode");
            lua_pushnumber(L, 0);
            lua_settable(L, -3);

            lua_pushstring(L, "serverCount");
            lua_pushnumber(L, serverCount);
            lua_settable(L, -3);

            lua_pushstring(L, "url");
            lua_pushstring(L, serverList[selectedServer]->url);
            lua_settable(L, -3);

            lua_pushstring(L, "lat");
            lua_pushnumber(L, serverList[selectedServer]->lat);
            lua_settable(L, -3);

            lua_pushstring(L, "lon");
            lua_pushnumber(L, serverList[selectedServer]->lon);
            lua_settable(L, -3);

            lua_pushstring(L, "name");
            lua_pushstring(L, serverList[selectedServer]->name);
            lua_settable(L, -3);

            lua_pushstring(L, "country");
            lua_pushstring(L, serverList[selectedServer]->country);
            lua_settable(L, -3);

            lua_pushstring(L, "sponsor");
            lua_pushstring(L, serverList[selectedServer]->sponsor);
            lua_settable(L, -3);

            lua_pushstring(L, "distance");
            lua_pushnumber(L, serverList[selectedServer]->distance);
            lua_settable(L, -3);

            lua_pushstring(L, "id");
            lua_pushnumber(L, serverList[selectedServer]->id);
            lua_settable(L, -3);

            lua_pushstring(L, "latency");
            lua_pushnumber(L, selectedLatency);
            lua_settable(L, -3);

            for(i=0; i<serverCount; i++){
                free(serverList[i]->url);
                free(serverList[i]->name);
                free(serverList[i]->sponsor);
                free(serverList[i]->country);
                free(serverList[i]->id);
                free(serverList[i]);
            }
        }
    }
    
    return 1;
}

int TestSpeed(lua_State* L)
{
    float downloadSpeed = 0;
    float uploadSpeed = 0;
    lua_newtable(L);
    if (NULL == uploadUrl)
    {
        lua_pushstring(L, "errorCode");
        lua_pushnumber(L, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "errorInfo");
        lua_pushstring(L, "uploadUrl is NULL.");
        lua_settable(L, -3);
    } else 
    {
        // test download
        downloadSpeed = testDownload(uploadUrl);
        lua_pushstring(L, "download");
        lua_pushnumber(L, downloadSpeed);
        lua_settable(L, -3);
        // test upload
        uploadSpeed = testUpload(uploadUrl);
        lua_pushstring(L, "upload");
        lua_pushnumber(L, uploadSpeed);
        lua_settable(L, -3);
    }

    freeMem();
    return 1;
}

luaL_Reg mylibs[] = {
    {"InitSpeedTest", InitSpeedTest},
    {"GetConfig", GetConfig},
    {"GetBestServer", GetBestServer},
    {"TestSpeed", TestSpeed},
    {NULL, NULL}
};

int luaopen_SpeedTestC(lua_State* L)
{
    const char* libName = "SpeedTestC";
    luaL_register(L, libName, mylibs);
    return 1;
}