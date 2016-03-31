#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>

#include "shm_channel.h"
#include "simplecache.h"

#define MAX_CACHE_REQUEST_LEN 256

static void _sig_handler(int signo){
	if (signo == SIGINT || signo == SIGTERM){
		/* Unlink IPC mechanisms here*/
		exit(signo);
	}
}

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"      \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -h                  Show this help message\n"                              

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"nthreads",           required_argument,      NULL,           't'},
  {"cachedir",           required_argument,      NULL,           'c'},
  {"help",               no_argument,            NULL,           'h'},
  {NULL,                 0,                      NULL,             0}
};

void Usage() {
  fprintf(stdout, "%s", USAGE);
}

int main(int argc, char **argv) {
	int nthreads = 1;
    char len[MAX_CACHE_REQUEST_LEN];
	char *cachedir = "locals.txt";
    char* rxq = "/proxy-to-cache";
    char* txq = "/cache-to-proxy";
    char* cxq = "/control-cache-proxy";
    char option_char;
    char buffer[MSG_SIZE];
    ctrl_msg ctrl;


	while ((option_char = getopt_long(argc, argv, "t:c:h", gLongOptions, NULL)) != -1) {
		switch (option_char) {
			case 't': // thread-count
				nthreads = atoi(optarg);
				break;   
			case 'c': //cache directory
				cachedir = optarg;
				break;
			case 'h': // help
				Usage();
				exit(0);
				break;    
			default:
				Usage();
				exit(1);
		}
	}

	if (signal(SIGINT, _sig_handler) == SIG_ERR){
		fprintf(stderr, "Can't catch SIGINT...exiting.\n");
		exit(EXIT_FAILURE);
	}

	if (signal(SIGTERM, _sig_handler) == SIG_ERR){
		fprintf(stderr, "Can't catch SIGTERM...exiting.\n");
		exit(EXIT_FAILURE);
	}

	/* Initializing the cache */
	simplecache_init(cachedir);
    
    


    mqd_t tx_mqd = create_message_queue(txq, O_CREAT | O_RDWR,  MSG_SIZE, MAX_MSGS);
    mqd_t rx_mqd = create_message_queue(rxq, O_CREAT | O_RDWR,  MSG_SIZE, MAX_MSGS);
    mqd_t cx_mqd = create_message_queue(cxq, O_CREAT | O_RDWR,  sizeof(ctrl_msg), MAX_MSGS);


    
    int brx = mq_receive(cx_mqd, (char*)&ctrl, sizeof(ctrl), 0);
    if(brx < 0){
        perror("mq_receive(ctrl_msg)");
        exit(EXIT_FAILURE);
    }
    
    
    fprintf(stderr, "Ctrl, seg_size=%d, num_seg=%d\n", ctrl.segment_size, ctrl.num_segments);
    steque_t* segment_q = (steque_t*) malloc(sizeof(steque_t));
    shm_init_segments2(segment_q, ctrl.segment_size, ctrl.num_segments);
    
    segment_item* seg = (segment_item*) steque_front(segment_q);
    
    
    sprintf(seg->mem, "%s", "Hi to proxy (cache)");

    //fprintf(stderr, "Segment id = %s\n", seg->segment_id);
    fprintf(stderr, "Writing: %s\n", seg->mem);

    

    

    



    while(1){
        receive_message(rx_mqd, &buffer[0]);
        int fd = simplecache_get(&buffer[0]);
        if(fd == -1){
            fprintf(stderr, "File not found in cache\n");
            send_message(tx_mqd, "Miss", 4, NOT_FOUND_PRIORITY);
        }
        else{
            unsigned file_size = lseek(fd, 0, SEEK_END); ASSERT(file_size > 0);
            lseek(fd, 0, SEEK_SET);
            memset(len, 0, sizeof(len));
            sprintf(len, "%d", file_size);
            send_message(tx_mqd, len, strlen(len)+1, HEADER_PRIORITY);
            send_file(tx_mqd, fd);
        }
        
        printf(stderr, "Reading Segment: %s \n", seg->mem);
    }
    mq_unlink(rxq);
    mq_unlink(txq);
    
    
    
    
}



