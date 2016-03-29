#ifndef CHANNEL_H
#define CHANNEL_H

#include <mqueue.h>


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

mqd_t create_message_queue(char* _name, int _flags, int _msg_sz, int _max_msgs);
void send_message(mqd_t _mqd, char* _msg, int _msg_len, int priority);
void send_file(mqd_t _mqd, int _fd);
int receive_message(mqd_t _mqd, char* _buffer);
struct mq_attr get_queue_attr(mqd_t _mqd);

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