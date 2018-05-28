#include "SpeedtestUploadTest.h"
#include "SpeedtestConfig.h"
#include "SpeedtestServers.h"
#include "Speedtest.h"
#include "http.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <stdbool.h>

static void __appendTimestamp(const char *url, char *buff, int buff_len)
{
	char delim = '?';
	char *p = strchr(url, '?');

	if (p)
		delim = '&';
	snprintf(buff, buff_len, "%s%cx=%llu", url, delim, (unsigned long long)time(NULL));
}

static void *__uploadThread(void *arg)
{
  /* Testing upload... */
	THREADARGS_T *threadConfig = (THREADARGS_T *)arg;
	int size, sockId;
	unsigned long totalTransfered = 0;
	char uploadUrl[1024];

	__appendTimestamp(threadConfig->url, uploadUrl, sizeof(uploadUrl));
	/* FIXME: totalToBeTransfered should be readonly while the upload thread is running */
  totalTransfered = threadConfig->totalToBeTransfered;
  sockId = httpPutRequestSocket(uploadUrl, totalTransfered);
  if(sockId == 0)
  {
      fprintf(stderr, "Unable to open socket for Upload!");
      pthread_exit(NULL);
  }

  while((totalTransfered != 0) && (getElapsedTime(tval_start) < threadConfig->timeout))
  {
  	if (totalTransfered > BUFFER_SIZE)
    {
  	  size = httpSend(sockId, uploadDataBuffer, BUFFER_SIZE);
  	}
    else
    {
			char buffer[BUFFER_SIZE] = {0};
			memcpy(buffer, uploadDataBuffer, BUFFER_SIZE);
  		buffer[totalTransfered - 1] = '\n'; /* Indicate terminated */
  		size = httpSend(sockId, buffer, totalTransfered);
  	}

    totalTransfered -= size;
		threadConfig->transferedBytes += size;
		
	}
	/* Cleanup */
	httpClose(sockId);

	return NULL;
}

float testUpload(const char *url)
{
  size_t numOfThreads = speedTestConfig->uploadThreadConfig.count *
		speedTestConfig->uploadThreadConfig.sizeLength;
	THREADARGS_T *param = (THREADARGS_T *) calloc(numOfThreads, sizeof(THREADARGS_T));
	bool outofRange = false;
	char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	/* Build the random buffer */
  srand(time(NULL));
	for (int count=0; count<BUFFER_SIZE; count++)
  {
    uploadDataBuffer[count] = alphabet[rand() % ARRAY_SIZE(alphabet)];
  }

	gettimeofday(&tval_start, NULL);
	for (int sizeCount=0; sizeCount<speedTestConfig->uploadThreadConfig.sizeLength; sizeCount++) {
		for (int threadsCount=0; threadsCount<speedTestConfig->uploadThreadConfig.count; threadsCount++) {
			
			int currentNum = sizeCount*speedTestConfig->uploadThreadConfig.count+threadsCount;
			if (currentNum < speedTestConfig->upload_max) {
				/* Initializing some parameters */
				param[currentNum].totalToBeTransfered = speedTestConfig->uploadThreadConfig.sizes[sizeCount];
				param[currentNum].url = strdup(url);
				param[currentNum].timeout = speedTestConfig->uploadThreadConfig.length;
				if (param[currentNum].url) {
					pthread_create(&param[currentNum].tid, NULL, &__uploadThread, &param[currentNum]);
				}
			} else {
				outofRange = true;
				break;
			}
		}
		if (outofRange == true) {
			break;
		}
	}

	/* Refresh */
	unsigned long totalTransfered = 0;
	float speed = 0;

	/* Wait for all threads */
	for (int count = 0; count < numOfThreads; count++) {
		if (param[count].tid) {
			pthread_join(param[count].tid, NULL);
			if (param[count].transferedBytes) {
				/* There's no reason that we transfered nothing except error occured */
				totalTransfered += param[count].transferedBytes;
			}
			/* Cleanup */
			free(param[count].url);
		}
	}
	free(param);
	speed = totalTransfered / getElapsedTime(tval_start) / 1024 / 1024 * 8;
	return speed;
}
