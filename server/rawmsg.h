#ifndef __RAW_MSG_H__
#define __RAW_MSG_H__

#define SENSOR_IO_MSG_CMD  _IOW ('i', 4, struct sensor_io_msg)

#define SENSOR_IO_MSG_START        (0x01)  //To ask SENSOR module start to work, and go into running state.
#define SENSOR_IO_MSG_STOP        (0x02)  //To ask SENSOR module stop to work, and go into stop state.
#define SENSOR_IO_MSG_SET_RAWBUF         (0x03)     //Configure sensor RAW data buffer queue.
#define SENSOR_IO_MSG_SET_RAWINFO           (0x04)      //Set raw information
#define SENSOR_IO_MSG_MASK     (0xff)

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

struct sensor_msg_start {
    unsigned int sensor;
};

struct sensor_msg_stop {
    unsigned int sensor;
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

enum sensor_type {
    SENSOR_TYPE_A = 1 << 0,
    SENSOR_TYPE_B = 1 << 1
};

#endif
