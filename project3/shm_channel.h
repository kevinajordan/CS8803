#ifndef CHANNEL_H
#define CHANNEL_H

#include <mqueue.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "steque.h"
#include <errno.h>

#include <sys/types.h>  /* Type definitions used by many programs */
#include <stdio.h>      /* Standard I/O functions */
#include <stdlib.h>     /* Prototypes of commonly used library functions,
plus EXIT_SUCCESS and EXIT_FAILURE constants */
#include <unistd.h>     /* Prototypes for many system calls */
#include <errno.h>      /* Declares errno and defines error constants */
#include <string.h>     /* Commonly used string-handling functions */

#define MSG_SIZE 2048
#define MAX_MSGS 20
#define NOT_FOUND_PRIORITY 1
#define HEADER_PRIORITY 2
#define ERROR (-1)
#define MAX_SEG 10


typedef struct ctrl_msg{
    int segment_size;
    int num_segments;
}ctrl_msg;


typedef struct segment_item{
    char* mem;
    char* segment_id;
}segment_item;




mqd_t create_message_queue(char* _name, int _flags, int _msg_sz, int _max_msgs);
void send_message(mqd_t _mqd, char* _msg, int _msg_len, int priority);
void send_file(mqd_t _mqd, int _fd);
void receive_message(mqd_t _mqd, char* _buffer);
struct mq_attr get_queue_attr(mqd_t _mqd);


void shm_init_segments(steque_t* _segment_queue, int _num_segments, int _segment_size);
void shm_init_segments2(steque_t* _segment_queue, int _num_segments, int _segment_size);
void shm_init_ids(steque_t* _segment_ids, int _num_segments);
void shm_map_segments(steque_t* _segment_mems, steque_t* _segment_ids, int _segment_size);
void shm_map_segments2(steque_t* _segment_mems, steque_t* _segment_ids, int _segment_size);
void shm_create_segment_info(steque_t* _segment_queue, steque_t* segment_ids, steque_t* _segment_mems);
void shm_create_segment_info2(steque_t* _segment_queue, steque_t* segment_ids, steque_t* _segment_mems);


/* Below helper macro was copied from
http://stackoverflow.com/questions/3056307/how-do-i-use-mqueue-in-a-c-program-on-a-linux-based-system
*/
#define ASSERT(x) \
do { \
if (!(x)) { \
fprintf(stderr, "%s:%d: ", __func__, __LINE__); \
perror(#x); \
exit(-1); \
} \
} while (0) \




#endif