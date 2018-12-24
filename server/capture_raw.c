#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <libgen.h>
#include <linux/types.h>
#include <strings.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <sys/mman.h>

#define DEVICE_FILE "/dev/sensor-device"
#define MEM_DEVICE_FILE "/dev/memory_dev"

#define SENSOR_IO_MSG_CMD		_IOW ('i', 4, struct sensor_io_msg)
#define MEMORY_IO_MEMORY_SPACE		_IOWR ('i', 4, struct memory_space)

#define SENSOR_IO_MSG_START				    (0x01)		//To ask SENSOR module start to work, and go into running state.
#define SENSOR_IO_MSG_STOP				    (0x02)		//To ask SENSOR module stop to work, and go into stop state.
#define SENSOR_IO_MSG_SET_RAWBUF	        (0x03)	    //Configure sensor RAW data buffer queue.
#define SENSOR_IO_MSG_SET_RAWINFO           (0x04)      //Set raw information
#define SENSOR_IO_MSG_MASK					(0xff)

struct sensor_msg_rawinfo
{
    unsigned int frame_w;         /* Video frame width    */
    unsigned int frame_h;         /* Video frame height   */
    unsigned int image_format;    /* It only can be RAW8, RAW10, RAW12 or RAW14*/
    unsigned int raw_pkt_fmt;
    unsigned int bayer_pattern;
};

struct sensor_msg_rawbuf
{
    unsigned int rawbuf_addr;     /* RAW buffer address   */
    unsigned int rawbuf_size;     /* RAW buffer size      */
    unsigned int queue_num;       /* RAW frame queue size */
};

struct sensor_msg_header {
    unsigned int msg_id;
    unsigned int msg_length;
};

struct sensor_io_msg
{
    struct sensor_msg_header header;
    unsigned char data[128];
};

struct sensor_msg_start {
    unsigned int sensor;
};

struct sensor_msg_stop {
    unsigned int sensor;
};

struct memory_space {
	unsigned int addr;
	unsigned int size;
};

enum sensor_type {
    SENSOR_TYPE_A = 1 << 0,
    SENSOR_TYPE_B = 1 << 1
};

int open_sensor_device(const char *pathname)
{
	int fd = -1;

	if ((fd = open(pathname, O_RDWR | O_SYNC)) == -1) {
		fprintf(stderr, "open %s failed, [%s]\n", pathname, strerror(errno));
		return -1;
	}
	printf("device %s opened.\n", pathname);
	fflush(stdout);
	return fd;
}

int close_sensor_device(int fd)
{
	if(-1 != fd)
	{
		close(fd);
		fd = -1;
	}

	return 0;
}

int set_frame_info(int fd, struct sensor_msg_rawinfo *rawinfo)
{
	struct sensor_io_msg sensor_msg;
	sensor_msg.header.msg_id = SENSOR_IO_MSG_SET_RAWINFO;
	sensor_msg.header.msg_length = sizeof(struct sensor_msg_rawinfo);

	memcpy(sensor_msg.data, rawinfo, sizeof(struct sensor_msg_rawinfo));

	if (ioctl(fd, SENSOR_IO_MSG_CMD, &sensor_msg) < 0 ) {
		fprintf(stderr, "failed to set map table: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

int set_raw_buffer(int fd, struct sensor_msg_rawbuf *rawbuf)
{
	struct sensor_io_msg sensor_msg;

	sensor_msg.header.msg_id = SENSOR_IO_MSG_SET_RAWBUF;
	sensor_msg.header.msg_length = sizeof(struct sensor_msg_rawbuf);

	memcpy(sensor_msg.data, rawbuf, sizeof(struct sensor_msg_rawbuf));

	if (ioctl(fd, SENSOR_IO_MSG_CMD, &sensor_msg) < 0 ) {
		fprintf(stderr, "failed to set map table: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

int start_sensor(int fd)
{
	struct sensor_io_msg sensor_msg;
	struct sensor_msg_start msg_start;

	sensor_msg.header.msg_id = SENSOR_IO_MSG_START;
	sensor_msg.header.msg_length = sizeof(struct sensor_msg_start);

	msg_start.sensor = SENSOR_TYPE_A;
	memcpy(sensor_msg.data, &msg_start, sizeof(struct sensor_msg_start));

	if (ioctl(fd, SENSOR_IO_MSG_CMD, &sensor_msg) < 0 ) {
		fprintf(stderr, "failed to set map table: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

int stop_sensor(int fd)
{
	struct sensor_io_msg sensor_msg;
	struct sensor_msg_stop msg_stop;

	sensor_msg.header.msg_id = SENSOR_IO_MSG_STOP;
	sensor_msg.header.msg_length = sizeof(struct sensor_msg_stop);

	msg_stop.sensor = SENSOR_TYPE_A;
	memcpy(sensor_msg.data, &msg_stop, sizeof(struct sensor_msg_stop));

	if (ioctl(fd, SENSOR_IO_MSG_CMD, &sensor_msg) < 0 ) {
		fprintf(stderr, "failed to set map table: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

#define FRAME_WIDTH 672
#define FRAME_HEIGHT 380

int main(int argc, char *argv[])
{
	struct sensor_msg_rawbuf *rawbuf = NULL;
	//struct sensor_msg_rawinfo rawinfo;
	unsigned buf_size = FRAME_WIDTH*FRAME_HEIGHT*2;
	int read_num = 0;
	int set_num = 0;
	int fd;
	int size;
	int ret;

	fd = open_sensor_device(DEVICE_FILE);
	if(-1 == fd)
	{
		printf("open_sensor_device failed.\n");
		return 0;

	}

	rawbuf = (struct sensor_msg_rawbuf *)malloc(sizeof(struct sensor_msg_rawbuf));
	if(NULL != rawbuf)
	{
		printf("malloc rawbuf ok.\n");
		rawbuf->rawbuf_addr = 0x9F000000;
		rawbuf->rawbuf_size = buf_size*8;
		rawbuf->queue_num   = 8;
	}
	else
	{
		free(rawbuf);
		close_sensor_device(fd);
		return 0;
	}

	ret = set_raw_buffer(fd, rawbuf);
	if(ret)
	{
		printf("set_raw_buffer failed.\n");
		free(rawbuf);
		close_sensor_device(fd);
		return 0;
	}
	printf("set_raw_buffer ok.\n");

	ret = start_sensor(fd);
	if(ret)
	{
		printf("start_sensor failed.\n");
		free(rawbuf);
		close_sensor_device(fd);
		return 0;
	}

	printf("start_sensor ok.\n");

	while(1)
	{
		size = read(fd, (void *)rawbuf, sizeof(struct sensor_msg_rawbuf));
		if(size != -1)
		{
			read_num++;
			printf("read buffer num:%d\n", read_num);
			printf("read buffer addr:0x%x\n", rawbuf->rawbuf_addr);
			printf("read buffer size:0x%x\n", rawbuf->rawbuf_size);

			if((read_num > 2) && (read_num < 40))
			{
				set_num = (read_num-2)%8;

				if(set_num > 0)
					set_num = set_num -1;
				else
					set_num = 7;

				printf("set number:%d.\n", set_num);
				rawbuf->rawbuf_size = buf_size;
				rawbuf->rawbuf_addr = 0x9F000000 + rawbuf->rawbuf_size*set_num;
				rawbuf->queue_num   = 1;

				printf("set buffer addr:0x%x\n", rawbuf->rawbuf_addr);
				set_raw_buffer(fd, rawbuf);
			}

			if(read_num > 40)
				break;

		}

	}

	if(NULL != rawbuf)
		free(rawbuf);

	stop_sensor(fd);
	close_sensor_device(fd);

	printf("capture raw finished.\n");

	return 0;
}


