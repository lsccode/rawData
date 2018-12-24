CC := gcc
CPPFLAGS += -I ../include

ALL := cl hardwrite
all:$(ALL)

cl:client.o
	$(CC) -o $@ $^ libev.a -lm -lpthread

hardwrite:hardWrite.c
	$(CC) -o $@ $^

install:
	cp -rf $(ALL) $(TARGET_DIR)	

clean:
	rm -rf *.o *.d $(ALL);cd $(TARGET_DIR);rm -rf $(ALL)
