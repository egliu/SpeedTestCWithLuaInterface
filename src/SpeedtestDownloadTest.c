#include "SpeedtestConfig.h"
#include "SpeedtestServers.h"
#include "SpeedtestDownloadTest.h"
#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>

static void *__downloadThread(void *arg)
{
  THREADARGS_T *threadConfig = (THREADARGS_T*)arg;
	char buffer[BUFFER_SIZE] = {0};
	int size = 1;
	int sockId = httpGetRequestSocket(threadConfig->url);

	if(sockId == 0)
	{
		fprintf(stderr, "Unable to open socket for Download!");
		pthread_exit(NULL);
	}

	while((size != 0) && (getElapsedTime(tval_start) < threadConfig->timeout))
	{
		size = httpRecv(sockId, buffer, BUFFER_SIZE);
    threadConfig->transferedBytes += size;
	}
	httpClose(sockId);

	return NULL;
}

float testDownload(const char *url)
{
  size_t numOfThreads = speedTestConfig->downloadThreadConfig.count *
    speedTestConfig->downloadThreadConfig.sizeLength;
  THREADARGS_T *param = (THREADARGS_T *)calloc(numOfThreads, sizeof(THREADARGS_T));
  int i;
  float speed = 0;
  unsigned long totalTransfered = 0;

  /* Initialize and start threads */
  gettimeofday(&tval_start, NULL);
  for (int sizeCount=0; sizeCount<speedTestConfig->downloadThreadConfig.sizeLength; sizeCount++) {
    char *downloadUrl = getServerDownloadUrl(url, speedTestConfig->downloadThreadConfig.sizes[sizeCount]);
    for (int threadCount=0; threadCount<speedTestConfig->downloadThreadConfig.count; threadCount++) {
      param[sizeCount*speedTestConfig->downloadThreadConfig.count + threadCount].url = strdup(downloadUrl);
      param[sizeCount*speedTestConfig->downloadThreadConfig.count + threadCount].timeout = 
        speedTestConfig->downloadThreadConfig.length;
      if (param[sizeCount*speedTestConfig->downloadThreadConfig.count + threadCount].url) {
        pthread_create(&param[sizeCount*speedTestConfig->downloadThreadConfig.count + threadCount].tid,
          NULL, &__downloadThread, &param[sizeCount*speedTestConfig->downloadThreadConfig.count + threadCount]);
      }
    }
    free(downloadUrl);
  }
  /* Wait for all threads */
  for (i = 0; i < numOfThreads; i++) {
    pthread_join(param[i].tid, NULL);
    if (param[i].transferedBytes) {
      /* There's no reason that we transfered nothing except error occured */
      totalTransfered += param[i].transferedBytes;
    }
    /* Cleanup */
    free(param[i].url);
  }
  free(param);
  speed = totalTransfered / getElapsedTime(tval_start) / 1024 / 1024 * 8;
  return speed;
}
