CC = gcc
CFLAGS = -O0 -g -std=c99 -Wall
FLAGS = -fPIC
LIBOPTS = -shared
LIBS = -lm -lpthread -llua
OBJS = src/Speedtest.c \
	src/SpeedtestConfig.c \
	src/SpeedtestServers.c \
	src/SpeedtestLatencyTest.c \
	src/SpeedtestDownloadTest.c \
	src/SpeedtestUploadTest.c \
	src/url.c \
	src/http.c

SpeedTestC.so: $(OBJS)
	$(CC) $(LIBOPTS) $(FLAGS) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean
clean:
	rm SpeedTestC.so
