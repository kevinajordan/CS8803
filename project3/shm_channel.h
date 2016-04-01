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
#define MAX_CACHE_REQUEST_LEN 256


typedef struct ctrl_msg{
    int segment_size;
    int num_segments;
}ctrl_msg;


typedef struct segment_item{
    int   segment_index;
    char* segment_id;
    void* segment_ptr;
}segment_item;


enum sm{ SM_GET_FILESIZE, SM_GET_DATA};

typedef struct thread_packet{
    char requested_file [MAX_CACHE_REQUEST_LEN];
    int file_size;
    int chunk_size;
    int segment_size;
    int cache_hit;
    unsigned int segment_index;
}thread_packet;



mqd_t create_message_queue(char* _name, int _flags, int _msg_sz, int _max_msgs);
void send_message(mqd_t _mqd, void* _msg, int _msg_len, unsigned int priority);
void receive_message(mqd_t _mqd, char* _buffer);
struct mq_attr get_queue_attr(mqd_t _mqd);


void  shm_create_segments(steque_t* segment_q, int _num_segments, int _segment_size);
char* shm_init_id(int _index);
void* shm_map_segment(char* _segment_id, int _segment_size);



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