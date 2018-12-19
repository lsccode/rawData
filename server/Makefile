CC := arm-linux-gnueabihf-gcc
LIBEV := libevarm.a
#CC := gcc
#LIBEV := libev.a
CPPFLAGS += -I ../include

ALL := ser raw
all:$(ALL)

ser:server.o rawDevice.o
	$(CC) -o $@ $^ $(LIBEV) -lm

raw:rawDevice.o	raw.o
	$(CC) -o $@ $^ $(LIBEV) -lm

install:
	cp -rf $(ALL) ../	

clean:
	rm -rf *.o *.d $(ALL);cd ../;rm -rf $(ALL)

