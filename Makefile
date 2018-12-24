TARGET_DIR := $(PWD)/output
COMPILE_DIR := client server

.PHONY:$(COMPILE_DIR)
all:$(COMPILE_DIR)

$(COMPILE_DIR):
	$(MAKE) --directory=$@ $(TARGET)

install:
	$(MAKE) TARGET=install
clean:
	$(MAKE) TARGET=clean

export 	TARGET_DIR
