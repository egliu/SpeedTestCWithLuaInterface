/*
    Server list parsing functions.

    Michał Obrembski (byku@byku.com.pl)
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SpeedtestServers.h"
#include "http.h"

void parseServer(SPEEDTESTSERVER_T *result, const char *configline)
{
    /* TODO: Remove Switch, replace it with something space-friendly
    result = malloc(sizeof(SPEEDTESTSERVER_T));*/
    int tokenSize = 0;
    char *posBegin = NULL;
    char *posEnd = NULL;
    char *subStr = NULL;
    const char *tokens[8] = {"url=\"","lat=\"", "lon=\"", "name=\"",
                                "country=\"", "cc=\"", "sponsor=\"", "id=\""};
    int count = 0;
    for(count=0; count<8; count++) {
        posBegin = strstr(configline, tokens[count]);
        if (NULL == posBegin) {
            return;
        }
        tokenSize = strlen(tokens[count]);
        posEnd = strstr(posBegin+tokenSize, "\"");
        if (NULL == posEnd) {
            return;
        }
        subStr = calloc(sizeof(char), posEnd-posBegin-tokenSize+1);
        strncpy(subStr, posBegin+tokenSize, posEnd-posBegin-tokenSize);
        subStr[posEnd-posBegin-tokenSize] = '\0';
        switch(count) {
            case 0:
                result->url = subStr;
                break;
            case 1:
                result->lat = strtof(subStr, NULL);
                free(subStr);
                break;
            case 2:
                result->lon = strtof(subStr, NULL);
                free(subStr);
                break;
            case 3:
                result->name = subStr;
                break;
            case 4:
                result->country = subStr;
                break;
            case 6:
                result->sponsor = subStr;
                break;
            case 7:
                result->id = strtol(subStr, NULL, 10);
                free(subStr);
                break;
            default:
                free(subStr);
                break;
        }
    }
}

SPEEDTESTSERVER_T **getServers(int *serverCount, const char *infraUrl)
{
    char buffer[1500] = {0};
    SPEEDTESTSERVER_T **list = NULL;
    int sockId = httpGetRequestSocket(infraUrl);
    if(sockId) {
        long size;
        while((size = recvLine(sockId, buffer, sizeof(buffer))) > 0)
        {
            buffer[size + 1] = '\0';
            if(strlen(buffer) > 25)
            {
                /*Ommiting XML invocation...*/
                if (strstr(buffer, "<?xml"))
                    continue;
                /*TODO: Fix case when server entry doesn't fit in TCP packet*/
                if(buffer[0] == '<' && buffer[size - 1] == '>')
                {
                    *serverCount = *serverCount + 1;
                    list = (SPEEDTESTSERVER_T**)realloc(list,
                        sizeof(SPEEDTESTSERVER_T**) * (*serverCount));
                    if(list == NULL) {
                        fprintf(stderr, "Unable to allocate memory for servers!\n");
                        return NULL;
                    }
                    list[*serverCount - 1] = malloc(sizeof(SPEEDTESTSERVER_T));
                    if(list[*serverCount - 1])
                        parseServer(list[*serverCount - 1],buffer);
                }
            }
        }
        httpClose(sockId);
        return list;
    }
    return NULL;
}

static char *modifyServerUrl(char *serverUrl, const char *urlFile)
{
  size_t urlSize = strlen(serverUrl);
  char *upload = strstr(serverUrl, "upload.php");
  if(upload == NULL) {
      printf("Download URL parsing error - cannot find upload.php in %s\n",
          serverUrl);
      return NULL;
  }
  size_t uploadSize = strlen(upload);
  size_t totalSize = (urlSize - uploadSize) +
      strlen(urlFile) + 1;
  char *result = (char*)malloc(sizeof(char) * totalSize);
  result[(urlSize - uploadSize)] = '\0';
  memcpy(result, serverUrl, urlSize - uploadSize);
  strcat(result, urlFile);
  return result;
}

char *getServerDownloadUrl(char *serverUrl)
{
  return modifyServerUrl(serverUrl, "random350x350.jpg");
}

char *getLatencyUrl(char *serverUrl)
{
  return modifyServerUrl(serverUrl, "latency.txt");
}
