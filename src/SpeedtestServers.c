/*
    Server list parsing functions.

    Michał Obrembski (byku@byku.com.pl)
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "SpeedtestServers.h"
#include "http.h"
#include "SpeedtestConfig.h"

bool isIgnored(char *id, char *ignoreServers)
{
    bool ignored = false;
    char *pos = NULL;
    
    pos = strstr(ignoreServers, id);
    if (NULL != pos) {
        char *nextChar = pos+strlen(id);
        if ((nextChar[0] == ',') || (nextChar[0] == '\0')) {
            ignored = true;
        }
    }
    return ignored;
}

SPEEDTESTSERVER_T* parseServer(const char *configline, char *ignoreServers)
{
    int tokenSize = 0;
    char *posBegin = NULL;
    char *posEnd = NULL;
    char *subStr = NULL;
    SPEEDTESTSERVER_T *result = NULL;
    const char *tokens[8] = {"id=\"", "url=\"","lat=\"", "lon=\"",
                             "name=\"", "country=\"", "sponsor=\""};
    int count = 0;
    for(count=0; count<7; count++) {
        posBegin = strstr(configline, tokens[count]);
        if (NULL == posBegin) {
            break;
        }
        tokenSize = strlen(tokens[count]);
        posEnd = strstr(posBegin+tokenSize, "\"");
        if (NULL == posEnd) {
            break;
        }
        subStr = calloc(sizeof(char), posEnd-posBegin-tokenSize+1);
        if (subStr) {
            strncpy(subStr, posBegin+tokenSize, posEnd-posBegin-tokenSize);
            subStr[posEnd-posBegin-tokenSize] = '\0';
        } else {
            fprintf(stderr, "Unable to allocate memory for subStr!\n");
            return NULL;
        }
        if (count == 0) {
            if (false == isIgnored(subStr, ignoreServers)) {
                result = malloc(sizeof(SPEEDTESTSERVER_T));
                if (result) {
                    memset(result, 0, sizeof(SPEEDTESTSERVER_T));
                    result->url = subStr;
                } else {
                    fprintf(stderr, "Unable to allocate memory for SPEEDTESTSERVER_T!\n");
                    return NULL;
                }
            } else {
                free(subStr);
                break;
            }
        }
        switch(count) {
            case 1:
                result->url = subStr;
                break;
            case 2:
                result->lat = strtof(subStr, NULL);
                free(subStr);
                break;
            case 3:
                result->lon = strtof(subStr, NULL);
                free(subStr);
                break;
            case 4:
                result->name = subStr;
                break;
            case 5:
                result->country = subStr;
                break;
            case 6:
                result->sponsor = subStr;
                break;
            default:
                free(subStr);
                break;
        }
    }
    return result;
}

SPEEDTESTSERVER_T **getServers(int *serverCount, char *ignoreServers, float lat, float lon)
{
    char buffer[1500] = {0};
    SPEEDTESTSERVER_T **list = NULL;
    char *urls[] = {"http://www.speedtest.net/speedtest-servers-static.php",
        "http://c.speedtest.net/speedtest-servers-static.php",
        "http://www.speedtest.net/speedtest-servers.php",
        "http://c.speedtest.net/speedtest-servers.php"};
    const u_int32_t urlsCount = 4;
    u_int32_t count = 0;
    u_int32_t reallocCount = 0;

    /* malloc the size as the macro SERVER_SIZE defines */
    list = (SPEEDTESTSERVER_T**)calloc(SERVER_SIZE, sizeof(SPEEDTESTSERVER_T**));
    if (NULL == list) {
        fprintf(stderr, "Unable to allocate memory for servers!\n");
        return NULL;
    }
    for(count = 0; count < urlsCount; count++) {
        int sockId = httpGetRequestSocket(urls[count]);
        if (sockId) {
            long size = 0;
            while((size = recvLine(sockId, buffer, sizeof(buffer))) > 0) {
                buffer[size + 1] = '\0';
                if (strlen(buffer) > 25) {
                    /* Omiting XML invocation... */
                    if (strstr(buffer, "<?xml")) {
                        continue;
                    }
                    if ((buffer[0] == '<') && (buffer[size-1] == '>')) {
                        if (*serverCount >= (SERVER_SIZE+reallocCount*SERVER_SIZE/2)) {
                            reallocCount++;
                            list = (SPEEDTESTSERVER_T**)realloc(list,
                                sizeof(SPEEDTESTSERVER_T**)*(SERVER_SIZE+reallocCount*SERVER_SIZE/2));
                            if (NULL == list) {
                                fprintf(stderr, "Unable to allocate memory for servers!\n");
                                return NULL;
                            }
                        }
                        list[*serverCount] = parseServer(buffer, ignoreServers);
                        if (NULL != list[*serverCount]) {
                            list[*serverCount]->distance = haversineDistance(lat, lon,
                                list[*serverCount]->lat, list[*serverCount]->lon);
                            *serverCount = *serverCount + 1;
                        }
                    }
                }
            }
            httpClose(sockId);
        }
        if (*serverCount > 0) {
            break;
        }
    }

    return list;
}

static char *modifyServerUrl(char *serverUrl, const char *urlFile)
{
  size_t urlSize = strlen(serverUrl);
  char *upload = NULL;
  /* find the last '/' */
  for(int count=0; count<urlSize; count++) {
      char *pos = serverUrl + urlSize - count - 1;
      if (*pos == '/') {
          upload = pos + 1;
          break;
      }
  }
  if(upload == NULL) {
      fprintf(stderr, "Server URL parsing error - in %s\n",
          serverUrl);
      return NULL;
  }
  size_t uploadSize = strlen(upload);
  size_t totalSize = (urlSize - uploadSize) +
      strlen(urlFile) + 1;
  char *result = (char*)malloc(sizeof(char) * totalSize);
  if (NULL == result) {
      fprintf(stderr, "malloc in modifyServerUrl failed");
      return NULL;
  }
  result[(urlSize - uploadSize)] = '\0';
  memcpy(result, serverUrl, urlSize - uploadSize);
  strcat(result, urlFile);
  return result;
}

char *getServerDownloadUrl(char *serverUrl, int size)
{
  char front[] = {"random"};
  char middle[] = {"x"};
  char back[] = {".jpg"};
  char str[10] = {0};
  sprintf(str, "%d", size);
  char *full = malloc(strlen(front)+strlen(str)*2+strlen(middle)+strlen(back)+1);
  if (NULL == full) {
      fprintf(stderr, "malloc failed in getServerDownloadUrl");
      return NULL;
  }
  strcpy(full, front);
  strcat(full, str);
  strcat(full, middle);
  strcat(full, str);
  strcat(full, back);
  char *result = modifyServerUrl(serverUrl, full);
  free(full);
  return result;
}

char *getLatencyUrl(char *serverUrl)
{
  return modifyServerUrl(serverUrl, "latency.txt");
}
