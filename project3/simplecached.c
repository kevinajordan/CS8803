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

void send_file(mqd_t _tx_mqd, mqd_t _rx_mqd, int _fd, thread_packet thr_pkt, segment_item* seg);


int main(int argc, char **argv) {
	int nthreads = 1;
    char len[MAX_CACHE_REQUEST_LEN];
	char *cachedir = "locals.txt";
    char* rxq = "/proxy-to-cache";
    char* txq = "/cache-to-proxy";
    char* cxq_tx = "/control-cache-proxy";
    char* cxq_rx = "/control-proxy-cache";
    char option_char;
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
    
    

    errno = 0;

    mqd_t tx_mqd = create_message_queue(txq, O_CREAT | O_RDWR,  sizeof(thread_packet), MAX_MSGS);
    mqd_t rx_mqd = create_message_queue(rxq, O_CREAT | O_RDWR,  sizeof(thread_packet), MAX_MSGS);
    mqd_t cx_mqd_tx = create_message_queue(cxq_tx, O_CREAT | O_RDWR,  sizeof(ctrl_msg), MAX_MSGS);
    mqd_t cx_mqd_rx = create_message_queue(cxq_rx, O_CREAT | O_RDWR,  sizeof(ctrl_msg), MAX_MSGS);

    
    
      printf("rx ctrl\n");
    int status= mq_receive(cx_mqd_rx, (char*)&ctrl, sizeof(ctrl_msg), 0);
    printf("status =%d\n", status);
    ASSERT(status >= 0);
    
      printf("sending ctrl\n");
    status = mq_send(cx_mqd_tx, (char*)&ctrl, sizeof(ctrl_msg), 0);
    ASSERT(status >= 0);
    
    //mq_unlink(txq);
    //mq_unlink(rxq);

    mq_close(cx_mqd_tx);
    mq_close(cx_mqd_rx);
    
    //fprintf(stderr, "Ctrl, seg_size=%d, num_seg=%d\n", ctrl.segment_size, ctrl.num_segments);
    steque_t* segment_q = (steque_t*) malloc(sizeof(steque_t));
    shm_create_segments(segment_q, ctrl.num_segments, ctrl.segment_size);
    
    segment_item* seg = (segment_item*) steque_front(segment_q);
    int ctrl_priority = 0;//ctrl.num_segments;
    
    
             
    //fprintf(stderr, "Segment id = %s\n", seg->segment_id);
    thread_packet thr_pkt;
    
    while(1){
        fprintf(stderr, "Waiting for request... ctrl =%d\n", ctrl_priority);

        //Get request
        int bytes_received = mq_receive(rx_mqd, (void*)&thr_pkt, sizeof(thr_pkt), ctrl_priority);
        
        thr_pkt.segment_size = ctrl.segment_size;
        thr_pkt.chunk_size = ctrl.segment_size;
        
        ASSERT(bytes_received >= 0);
        int fd = simplecache_get(thr_pkt.requested_file);
        fprintf(stderr, "Requests recieved.. seg=%d, fd=%d, Path: %s\n", thr_pkt.segment_index, fd, thr_pkt.requested_file);

        if(fd == -1){
            thr_pkt.cache_hit = 0;
            fprintf(stderr, "File not found in cache\n");
            send_message(tx_mqd, (void*)&thr_pkt, sizeof(thr_pkt), thr_pkt.segment_index);
        }
        else{
            unsigned file_size = lseek(fd, 0, SEEK_END); ASSERT(file_size > 0);
            lseek(fd, 0, SEEK_SET);
            memset(len, 0, sizeof(len));
            sprintf(len, "%d", file_size);
            thr_pkt.cache_hit = 1;
            thr_pkt.file_size = atoi(len);
        
            fprintf(stderr, "Sending request ACK..\n");
            send_message(tx_mqd, (void*)&thr_pkt, sizeof(thr_pkt), thr_pkt.segment_index);
            
            send_file(tx_mqd, rx_mqd, fd, thr_pkt, seg);
            fprintf(stderr,"\n----- Transfer Complete! --- seg=%d\n", thr_pkt.segment_index);
    
        }
        
        //fprintf(stderr, "Reading Segment: %s \n", (char*)seg->segment_ptr);
    }
     
    
    mq_unlink(rxq);
    mq_unlink(txq);
}

void send_file(mqd_t _tx_mqd, mqd_t _rx_mqd, int _fd, thread_packet thr_pkt, segment_item* _seg){
    int total_bytes_rx = 0;
    int n;
    
    while (total_bytes_rx < thr_pkt.file_size){
        int offset = thr_pkt.file_size - total_bytes_rx;
        thr_pkt.chunk_size = (offset < thr_pkt.segment_size) ? offset : thr_pkt.segment_size;
        

        n = read(_fd, _seg->segment_ptr, thr_pkt.chunk_size);
        ASSERT(n == thr_pkt.chunk_size);
        

        //sprintf(_seg->segment_ptr, "%s", _seg->segment_ptr);
        //fprintf(stderr,"n: %d, chunk: %d \n", n, thr_pkt.chunk_size);
        
        //fprintf(stderr,"Client can read data now. Chunk: %d, Hit = %d\n", thr_pkt.chunk_size, thr_pkt.cache_hit);
        
        fprintf(stderr,"%d/%d .. ", total_bytes_rx, thr_pkt.file_size);

        send_message(_tx_mqd, (void*)&thr_pkt, sizeof(thr_pkt), thr_pkt.segment_index);

        total_bytes_rx += thr_pkt.chunk_size;
        
        //fprintf(stderr,"Waiting for data ACK\n");
        n = mq_receive(_rx_mqd, (void*)&thr_pkt, sizeof(thr_pkt), &(thr_pkt.segment_index));
        ASSERT(n >= 0);
        
    }
}




