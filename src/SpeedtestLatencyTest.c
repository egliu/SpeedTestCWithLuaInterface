#include "SpeedtestConfig.h"
#include "SpeedtestServers.h"
#include "SpeedtestLatencyTest.h"
#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>

/* Latency test is just testing time taken to download single latency.txt file
which contains 10 chars: "test=test\n"*/
#define LATENCY_SIZE 10

void testLatency(const char *url, SPEEDTESTRS_T *rst)
{
  char buffer[LATENCY_SIZE] = {0};
  int sockId, size = -1;
	struct timeval tval_start;

  gettimeofday(&tval_start, NULL);
	sockId = httpGetRequestSocket(url);

	if(sockId == 0)
	{
		rst->errorCode = 1;
		strcpy(rst->errorInfo, "Unable to open socket for testing latency!");
		return;
	}

	while(size != 0)
	{
	  size = httpRecv(sockId, buffer, LATENCY_SIZE);
		if (size == -1)
    {
			rst->errorCode = 1;
			strcpy(rst->errorInfo, "Cannot download latency!");
			return;
    }
	}
	httpClose(sockId);
	rst->errorCode = 0;
	rst->speed = (int)(getElapsedTime(tval_start) * 1000.0f);
}
