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
    free(latencyUrl);
    free(downloadUrl);
    free(uploadUrl);
    free(serverList);
    free(speedTestConfig);
}

int InitSpeedTest(lua_State* L)
{
    freeMem();
    // parameters from lua is not supported yet
    totalTransfered = 1024 * 1024;
    totalToBeTransfered = 1024 * 1024;
    totalDownloadTestCount = 1;
    randomizeBestServers = 0;
    speedTestConfig = NULL;
    downloadUrl = NULL;
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
        lua_pushstring(L, "length");
        lua_pushnumber(L, speedTestConfig->uploadThreadConfig.length);
        lua_settable(L, -3);
        lua_settable(L, -3);

        lua_pushstring(L, "downloadThreadConfig");
        lua_newtable(L);
        lua_pushstring(L, "threadsCount");
        lua_pushnumber(L, speedTestConfig->downloadThreadConfig.threadsCount);
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
        serverList = getServers(&serverCount, "http://www.speedtest.net/speedtest-servers-static.php");
        if (serverCount == 0)
        {
            // Primary server is not responding. Let's give a try with secondary one.
            serverList = getServers(&serverCount, "http://c.speedtest.net/speedtest-servers-static.php");
        }
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
            for(i=0; i<serverCount; i++)
                serverList[i]->distance = haversineDistance(speedTestConfig->lat,
                    speedTestConfig->lon,
                    serverList[i]->lat,
                    serverList[i]->lon);

            qsort(serverList, serverCount, sizeof(SPEEDTESTSERVER_T *),
                        (int (*)(const void *,const void *)) sortServers);

            if (randomizeBestServers != 0) {
                srand(time(NULL));
                selectedServer = rand() % randomizeBestServers;
            }
            downloadUrl = getServerDownloadUrl(serverList[selectedServer]->url);
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

            for(i=0; i<serverCount; i++){
                free(serverList[i]->url);
                free(serverList[i]->name);
                free(serverList[i]->sponsor);
                free(serverList[i]->country);
                free(serverList[i]);
            }
        }
    }
    
    return 1;
}

int TestSpeed(lua_State* L)
{
    lua_newtable(L);
    latencyUrl = getLatencyUrl(uploadUrl);
    if ((NULL == uploadUrl) || (NULL == downloadUrl) || (NULL == latencyUrl))
    {
        lua_pushstring(L, "errorCode");
        lua_pushnumber(L, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "errorInfo");
        lua_pushstring(L, "uploadUrl or downloadUrl or latencyUrl is NULL.");
        lua_settable(L, -3);
    } else 
    {
        SPEEDTESTRS_T* result = malloc(sizeof(SPEEDTESTRS_T));
        if (NULL == result) 
        {
            lua_pushstring(L, "errorCode");
            lua_pushnumber(L, 1);
            lua_settable(L, -3);

            lua_pushstring(L, "errorInfo");
            lua_pushstring(L, "Fail to get memory for SPEEDTESTRS_T.");
            lua_settable(L, -3);
        } else 
        {
            memset(result, 0, sizeof(SPEEDTESTRS_T));
            // first step: test Latency
            testLatency(latencyUrl, result);
            if (result->errorCode != 0)
            {
                lua_pushstring(L, "errorCode");
                lua_pushnumber(L, result->errorCode);
                lua_settable(L, -3);

                lua_pushstring(L, "errorInfo");
                lua_pushstring(L, result->errorInfo);
                lua_settable(L, -3);
            } else
            {
                lua_pushstring(L, "errorCode");
                lua_pushnumber(L, result->errorCode);
                lua_settable(L, -3);

                lua_pushstring(L, "latency");
                lua_pushnumber(L, result->speed);
                lua_settable(L, -3);
                // second step: test download
                memset(result, 0, sizeof(SPEEDTESTRS_T));
                testDownload(downloadUrl, result);
                lua_pushstring(L, "download");
                lua_pushnumber(L, result->speed);
                lua_settable(L, -3);
                // third step: test upload
                memset(result, 0, sizeof(SPEEDTESTRS_T));
                testUpload(uploadUrl, result);
                lua_pushstring(L, "upload");
                lua_pushnumber(L, result->speed);
                lua_settable(L, -3);
            }
        }
        free(result);
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