/*
    Client configuration parsing functions.

    Michał Obrembski (byku@byku.com.pl)
*/
#include "SpeedtestConfig.h"
#include "http.h"
#include "url.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

const char *ConfigLineIdentitier[] = {"<client", "<upload", "<server-config", "<download"};

float haversineDistance(float lat1, float lon1, float lat2, float lon2)
{
    float dx, dy, dz, a, b;
    lon1 -= lon2;
    lon1 *= TO_RAD, lat1 *= TO_RAD, lat2 *= TO_RAD;

    dz = sin(lat1) - sin(lat2);
    dx = cos(lon1) * cos(lat1) - cos(lat2);
    dy = sin(lon1) * cos(lat1);
    a = (dx * dx + dy * dy + dz * dz);
    b = sqrt(a) / 2;
#ifdef USE_ASIN
    return 2 * R * asin(b);
#else
    return 2 * R * atan2(b, sqrt(1 - b * b));
#endif
}

static void getValue(const char* _str, const char* _key, char* _value)
{
	char *beginning, *end;
	if ((beginning = strstr(_str, _key)) != NULL) {
		beginning += strlen(_key); /* With an extra " */
		end = strchr(beginning, '"');
		if (end)
    {
			strncpy(_value, beginning, end - beginning);
    }
	}
}

static void parseClient(const char *configline, SPEEDTESTCONFIG_T **result_p)
{
	SPEEDTESTCONFIG_T *result = *result_p;
  char lat[16] = {0};
  char lon[16] = {0};

	if(sscanf(configline,"%*[^\"]\"%15[^\"]\"%*[^\"]\"%15[^\"]\"%*[^\"]\"%15[^\"]\"%*[^\"]\"%255[^\"]\"",
					result->ip, lat, lon, result->isp)!=4)
	{
			fprintf(stderr,"Cannot parse all fields! Config line: %s", configline);
			// Set a wrong value here to indicate something wrong happened
			result->lat = INCORRECTVALUE;
			result->lon = INCORRECTVALUE;
			return;
	}
	result->lat = strtof(lat, NULL);
	result->lon = strtof(lon, NULL);
}

static void parseUpload(const char *configline, SPEEDTESTCONFIG_T **result_p)
{
	SPEEDTESTCONFIG_T *result = *result_p;
	char threads[8] = {"2"}, testlength[8] = {"10"}, ratios[8] = {"5"}, maxchunkcount[8] = {"50"};
	int upSizes[] = {32768, 65536, 131072, 262144, 524288, 1048576, 7340032};
	int upSizesLength = ARRAY_SIZE(upSizes);
	int ratio = 5;
	int size = 0;

	getValue(configline, "testlength=\"", testlength);
	getValue(configline, "threads=\"", threads);
	getValue(configline, "ratio=\"", ratios);
	getValue(configline, "maxchunkcount=\"", maxchunkcount);

	result->uploadThreadConfig.threadsCount = atoi(threads);
	result->uploadThreadConfig.length = atoi(testlength);
	ratio = atoi(ratios);

	if (ratio <= upSizesLength) {
		size = upSizesLength - ratio + 1;
		result->uploadThreadConfig.sizes = calloc(size, sizeof(int));
		for(int count=0; count<size; count++) {
			result->uploadThreadConfig.sizes[count] = upSizes[ratio-1+count];
		}
		result->uploadThreadConfig.sizeLength = size;
		result->uploadThreadConfig.count = (int)(ceil(atoi(maxchunkcount) / size));
		result->upload_max = result->uploadThreadConfig.count * size;
	}
}

static void parseDownload(const char *configline, SPEEDTESTCONFIG_T **result_p)
{
	SPEEDTESTCONFIG_T *result = *result_p;
	char count[8] = {"4"};
	char length[8] = {"10"};
	int downSizes[] = {350, 500, 750, 1000, 1500, 2000, 2500, 3000, 3500, 4000};
	int downSizesLength = ARRAY_SIZE(downSizes);

	getValue(configline, "threadsperurl=\"", count);
	getValue(configline, "testlength=\"", length);

	result->downloadThreadConfig.count = atoi(count);
	result->downloadThreadConfig.length = atoi(length);
	result->downloadThreadConfig.sizes = calloc(downSizesLength, sizeof(int));
	result->downloadThreadConfig.sizeLength = downSizesLength;
	for(int count=0; count<downSizesLength; count++) {
		result->downloadThreadConfig.sizes[count] = downSizes[count];
	}
}

static void parseServerConfig(const char *configline, SPEEDTESTCONFIG_T **result_p)
{
	SPEEDTESTCONFIG_T *result = *result_p;
	char threadcount[8] = {"4"};
	char ignoreids[65535] = {0};

	getValue(configline, "threadcount=\"", threadcount);
	getValue(configline, "ignoreids=\"", ignoreids);

	result->downloadThreadConfig.threadsCount = atoi(threadcount)*2;
	strncpy(result->ignoreServers, ignoreids, 65535);
}

SPEEDTESTCONFIG_T *getConfig()
{
    SPEEDTESTCONFIG_T *result = NULL;
    char buffer[0xFFFF] = {0};
		int i, parsed = 0;
    long size;
		void (*parsefuncs[])(const char *configline, SPEEDTESTCONFIG_T **result_p)
							= { parseClient, parseUpload, parseDownload, parseServerConfig };
    int sockId = httpGetRequestSocket("http://www.speedtest.net/speedtest-config.php");

		if(!sockId)
		{
			return NULL; /* Cannot connect to server */
		}

    while((size = recvLine(sockId, buffer, sizeof(buffer))) > 0)
    {
      buffer[size + 1] = '\0';
      for (i = 0; i < ARRAY_SIZE(ConfigLineIdentitier); i++)
			{
      	if(strncmp(buffer, ConfigLineIdentitier[i], strlen(ConfigLineIdentitier[i])))
				{
					continue;
				}

				if (!result) {
					result = malloc(sizeof(struct speedtestConfig));
					if (!result) {
						/* Out of memory */
						return NULL;
					}
				}
				parsefuncs[i](buffer, &result);
        if (i == 0) {
        	/* The '<client' one is required */
					parsed += ARRAY_SIZE(ConfigLineIdentitier);
      	}
				parsed++;
        break;
    	}

      if (parsed == ARRAY_SIZE(ConfigLineIdentitier) * 2) {
				break;
      }
    }

      /* Cleanup */
		httpClose(sockId);
		if ((parsed < ARRAY_SIZE(ConfigLineIdentitier))
			&& result) {
			/* The required one is missing, we won't continue */
			free(result);
			result = NULL;
		}

		return result;
}
