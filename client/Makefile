CC := gcc
CPPFLAGS += -I ../include

ALL := cl hardwrite
all:$(ALL)

cl:client.o
	$(CC) -o $@ $^ libev.a -lm -lpthread

hardwrite:hardWrite.c
	$(CC) -o $@ $^

install:
	cp -rf $(ALL) ../

clean:
	rm -rf *.o *.d $(ALL);cd ../;rm -rf $(ALL)
