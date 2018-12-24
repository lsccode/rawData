CC := arm-linux-gnueabihf-gcc
LIBEV := libevarm.a
#CC := gcc
#LIBEV := libev.a
CPPFLAGS += -I ../include

ALL := ser raw demo 
all:$(ALL)

ser:server.o rawDevice.o virtualDevice.o
	$(CC) -o $@ $^ $(LIBEV) -lm -lpthread

raw:rawDevice.o	raw.o
	$(CC) -o $@ $^ $(LIBEV) -lm

demo:capture_raw.o
	$(CC) -o $@ $^	

install:
	cp -rf $(ALL) $(TARGET_DIR)	

clean:
	rm -rf *.o *.d $(ALL);cd $(TARGET_DIR);rm -rf $(ALL)

