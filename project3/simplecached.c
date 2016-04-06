#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <pthread.h>

#include "shm_channel.h"
#include "simplecache.h"
#include <pthread.h>


#define MAX_LEN 256

pthread_mutex_t seg_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  seg_cond = PTHREAD_COND_INITIALIZER;

ctrl_msg ctrl;
steque_t* segment_q;
mqd_t cmq_tx_fd;
mqd_t cmq_rx_fd;
char* cmq_tx_str = "/control-cache-proxy";
char* cmq_rx_str = "/control-proxy-cache";


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

void  perform_task(void* _thread_info);
void  send_file(mqd_t _ctrl_mq_tx, mqd_t _ctrl_mq_rx, int _fd, thread_packet thr_pkt, segment_item* seg);
thread_info_t*  init_threads(int _num_threads);
void  clean_control_mq(char* cmq_tx_str, char* cmq_rx_str, int cmq_tx_fd, int cmq_rx_fd);
void   sync_with_proxy();
segment_item* find_segment(int index, int* found);


int main(int argc, char **argv) {
	int nthreads = 1;
    char option_char;

    char *cachedir = "locals.txt";


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
		dbg("Can't catch SIGINT...exiting.\n");
		exit(EXIT_FAILURE);
	}

	if (signal(SIGTERM, _sig_handler) == SIG_ERR){
		dbg("Can't catch SIGTERM...exiting.\n");
		exit(EXIT_FAILURE);
	}

	/* Initializing the cache */
	simplecache_init(cachedir);
    
    /* Two-way handshake sync */
    sync_with_proxy();
    
    /* Create shared memory segments */
    segment_q = (steque_t*) malloc(sizeof(steque_t));
    shm_create_segments(segment_q, ctrl.num_segments, ctrl.segment_size, 0);
    
    /* Initialize and run threads, each calls 'perform_task()' */
    thread_info_t* tinfo = init_threads(nthreads);
    
    //Pthread_join calls each thread's exit routine
    for(int i = 0; i < nthreads; i++){
        pthread_join(tinfo[i].thread_id, NULL);
    }
    
    /* Clean up allocated memory */
    clean_control_mq(cmq_tx_str, cmq_rx_str, cmq_tx_fd, cmq_rx_fd);
    shm_clean_segments();
    free(tinfo);

}

/* A requests itself lest the cache now what segment to use when
 * sending the response.  We need to find this segment in our 
 * segment queue.  No need to worry about thread-safety because
 * proxy guarantees to use only one segment at a time */
segment_item* find_segment(int index, int* found){
    int i = 0;
    segment_item* seg = NULL;
    *found = 0;
    while(i < ctrl.num_segments){
        seg = (segment_item*)steque_front(segment_q);
        steque_cycle(segment_q);
        if(seg->segment_index == index){
            *found = 1;
            break;
        }
    }
    return seg;
}

/* Function call by individual threads.  Threads atomically
 * dequeue segments from the segement queue and perform
 * file transfer if file is present in cache */
void perform_task(void* _thread_info){
    char len[MAX_LEN];
    int found;
    segment_item* seg;
    thread_packet thr_pkt;

    while(1){
        //Wait to receive request from client
        rx_mq(cmq_rx_fd, (void*)&thr_pkt, sizeof(thr_pkt));

        thr_pkt.segment_size = ctrl.segment_size;
        thr_pkt.chunk_size = -1;
        
        /* Lookup cache */
        int fd = simplecache_get(thr_pkt.requested_file);

        /* Get shared memory segment */
        seg = find_segment(thr_pkt.segment_index, &found);
        ASSERT(found);

        dbg("Rx request.. seg idx=%d, thr_id=%x path: %s\n", thr_pkt.segment_index, (int)pthread_self(), thr_pkt.requested_file);
        
        if(fd == -1){
            thr_pkt.cache_hit = 0;
            thr_pkt.file_size = 0;
            dbg("File not found in cache\n");
            tx_mq(seg->mq_data_tx, (void*)&thr_pkt, sizeof(thr_pkt));
        }
        else{
            unsigned file_size = lseek(fd, 0, SEEK_END); ASSERT(file_size > 0);
            lseek(fd, 0, SEEK_SET);
            memset(len, 0, sizeof(len));
            sprintf(len, "%d", file_size);
            thr_pkt.cache_hit = 1;
            thr_pkt.file_size = atoi(len);
            
            /* Send ACK, client can then read shared-memory segment */
            tx_mq(seg->mq_data_tx, (void*)&thr_pkt, sizeof(thr_pkt));
            
            
            
            send_file(seg->mq_data_tx, seg->mq_data_rx, fd, thr_pkt, seg);
            dbg("----- Transfer Complete! ---  seg idx=%d, thr_id=%x\n", seg->segment_index, (int)pthread_self());
            
        }
        
        //dbg("Reading Segment: %s \n", (char*)seg->segment_ptr);
    }

    
    free(_thread_info);
}


void send_file(mqd_t _ctrl_mq_tx, mqd_t _ctrl_mq_rx, int _fd, thread_packet thr_pkt, segment_item* _seg){
    int total_bytes_rx = 0;
    int n;
    ASSERT(thr_pkt.segment_size > 0);
    
    while (total_bytes_rx < thr_pkt.file_size){
        int offset = thr_pkt.file_size - total_bytes_rx;
        ASSERT(thr_pkt.file_size > total_bytes_rx);
        thr_pkt.chunk_size = (offset < thr_pkt.segment_size) ? offset : thr_pkt.segment_size;
        

        n = pread(_fd, _seg->segment_ptr, thr_pkt.chunk_size, total_bytes_rx);
        if(n < 0){ //EOF
            dbg("1) seg: %d, n: %d, fs: %d, chunk: %d, offset:%d \n", _seg->segment_index, n, thr_pkt.file_size, thr_pkt.chunk_size, offset);
            dbg("1) seg: %d, %d/%d..", _seg->segment_index, total_bytes_rx, thr_pkt.file_size);
            ASSERT(0);
        }
        /*if(n != thr_pkt.chunk_size){
            dbg("2) seg: %d, n: %d, fs: %d, chunk: %d, offset:%d \n", _seg->segment_index, n, thr_pkt.file_size, thr_pkt.chunk_size, offset);
            dbg("2) seg: %d, %d/%d..", _seg->segment_index, total_bytes_rx, thr_pkt.file_size);
            ASSERT(0);
        }
        */
        
        if(n == 0){
            dbg("2) seg: %d, n: %d, fs: %d, chunk: %d, offset:%d \n", _seg->segment_index, n, thr_pkt.file_size, thr_pkt.chunk_size, offset);
            dbg("2) seg: %d, %d/%d..", _seg->segment_index, total_bytes_rx, thr_pkt.file_size);
            ASSERT(total_bytes_rx == thr_pkt.file_size);
            return;
        }
        //sprintf(_seg->segment_ptr, "%s", _seg->segment_ptr);
        
        //dbg("Client can read data now. Chunk: %d, Hit = %d\n", thr_pkt.chunk_size, thr_pkt.cache_hit);


        thr_pkt.chunk_size = n;
        ASSERT(thr_pkt.chunk_size <= thr_pkt.segment_size);
        tx_mq(_seg->mq_data_tx, (void*)&thr_pkt, sizeof(thr_pkt));

        total_bytes_rx += n;

        //Wait until client reads the data and send an ACK back to the cache
        rx_mq(_seg->mq_data_rx, (void*)&thr_pkt, sizeof(thr_pkt));
        
    }
}


thread_info_t* init_threads(int _num_threads){
    char name[MAX_LEN];
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    thread_info_t* thread_info = calloc(_num_threads, sizeof(thread_info_t));
    
    
    for (int i = 0; i < _num_threads; i++) {
        memset(name, 0, MAX_LEN);
        snprintf(name, MAX_LEN,"thread_%d",i);
        thread_info[i].name = strdup(name);
        thread_info[i].thread_num = i;
        
        int success = pthread_create(&thread_info[i].thread_id, &attributes, (void*)&perform_task, &thread_info[i]);
        ASSERT(success == 0);
    }
    
    pthread_attr_destroy(&attributes);
    
    return thread_info;
}


/* Function makes sure both processes are ready to process req/resps */
void sync_with_proxy(){
    cmq_tx_fd = create_message_queue(cmq_tx_str, O_CREAT | O_RDWR,  sizeof(thread_packet), MAX_MSGS);
    cmq_rx_fd = create_message_queue(cmq_rx_str, O_CREAT | O_RDWR,  sizeof(thread_packet), MAX_MSGS);
    
    /* Two-way handshake sync */
    rx_mq(cmq_rx_fd, (char*)&ctrl, sizeof(thread_packet));
    tx_mq(cmq_tx_fd, (char*)&ctrl, sizeof(thread_packet));
}


/* Clean up control message queues */
void clean_control_mq(char* cmq_tx_str, char* cmq_rx_str, int cmq_tx_fd, int cmq_rx_fd){
    int n = mq_unlink(cmq_tx_str);
    ASSERT(n == 0);
    n = mq_unlink(cmq_rx_str);
    ASSERT(n == 0);
    
    mq_close(cmq_tx_fd);
    mq_close(cmq_rx_fd);
}



