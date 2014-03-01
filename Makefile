CC=gcc

CFLAGS += -ansi -std=c99 -ggdb
CFLAGS += $(shell pkg-config --cflags json)
LDFLAGS += $(shell pkg-config --libs json)

LIBS=-llightwaverf -lwiringPi -lpaho-mqtt3as

lightwaverf-mqtt-adapter: lightwaverf-mqtt-adapter.c
	gcc -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f *.o lightwaverf-mqtt-adapter 
