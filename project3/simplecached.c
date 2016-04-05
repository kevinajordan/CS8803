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
mqd_t cx_mqd_tx;
mqd_t cx_mqd_rx;

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

void perform_task(void* _thread_info);
void send_file(mqd_t _ctrl_mq_tx, mqd_t _ctrl_mq_rx, int _fd, thread_packet thr_pkt, segment_item* seg);
thread_info_t* init_threads(int _num_threads);



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
    
    char* cxq_tx = "/control-cache-proxy";
    char* cxq_rx = "/control-proxy-cache";

    
    cx_mqd_tx = create_message_queue(cxq_tx, O_CREAT | O_RDWR,  sizeof(thread_packet), MAX_MSGS);
    cx_mqd_rx = create_message_queue(cxq_rx, O_CREAT | O_RDWR,  sizeof(thread_packet), MAX_MSGS);
    
    
    
    printf("rx ctrl\n");
    int status= mq_receive(cx_mqd_rx, (char*)&ctrl, sizeof(thread_packet), 0);
    printf("status =%d\n", status);
    ASSERT(status >= 0);
    
    printf("sending ctrl\n");
    status = mq_send(cx_mqd_tx, (char*)&ctrl, sizeof(thread_packet), 0);
    ASSERT(status >= 0);
    
    mq_unlink(cxq_tx);
    mq_unlink(cxq_rx);
    
    //mq_close(cx_mqd_tx);
    //mq_close(cx_mqd_rx);
    
    
    segment_q = (steque_t*) malloc(sizeof(steque_t));
    shm_create_segments(segment_q, ctrl.num_segments, ctrl.segment_size, 0);
    
    thread_info_t* tinfo = init_threads(nthreads);
    for(int i = 0; i < nthreads; i++){
        pthread_join(tinfo[i].thread_id, NULL);
    }
}

/* Function call by individual threads.  Threads atomically
 * dequeue segments from the segement queue and perform
 * file transfer if file is present in cache */
void perform_task(void* _thread_info){
    thread_info_t *thread_info = _thread_info;
    printf("ID: %d\n", thread_info->thread_num);
    char len[MAX_LEN];
    //char* rxq = "/proxy-to-cache";
    //char* txq = "/cache-to-proxy";

    //dbg("Ctrl, seg_size=%d, num_seg=%d\n", ctrl.segment_size, ctrl.num_segments);
    
    
    /* Get the next available segment, thread-safe
    pthread_mutex_lock(&seg_mutex);
    
    while(steque_isempty(segment_q))
        pthread_cond_wait(&seg_cond, &seg_mutex);
    
    pthread_mutex_unlock(&seg_mutex);
    */
    segment_item* seg;

    
    //= (segment_item*) steque_front(segment_q
    
    //dbg("Segment id = %s\n", seg->segment_id);
    thread_packet thr_pkt;
    

    
    
    while(1){
        //Get request
        int bytes_received = mq_receive(cx_mqd_rx, (void*)&thr_pkt, sizeof(thr_pkt), 0);

        thr_pkt.segment_size = ctrl.segment_size;
        thr_pkt.chunk_size = -1;
        
        
        ASSERT(bytes_received >= 0);
        int fd = simplecache_get(thr_pkt.requested_file);
        dbg("Rx request.. seg idx=%d, thr_id=%x path: %s\n", thr_pkt.segment_index, (int)pthread_self(), thr_pkt.requested_file);
        
        
        
        
        int i = 0;
        int found = 0;
        while(i < ctrl.num_segments){
            seg = (segment_item*)steque_front(segment_q);
            steque_cycle(segment_q);
            if(seg->segment_index == thr_pkt.segment_index){
                found = 1;
                break;
            }
        }
        
        
        
        if(fd == -1){
            thr_pkt.cache_hit = 0;
            thr_pkt.file_size = 0;
            dbg("File not found in cache\n");
            send_message(seg->mq_data_tx, (void*)&thr_pkt, sizeof(thr_pkt), thr_pkt.segment_index);
        }
        else{
            unsigned file_size = lseek(fd, 0, SEEK_END); ASSERT(file_size > 0);
            lseek(fd, 0, SEEK_SET);
            memset(len, 0, sizeof(len));
            sprintf(len, "%d", file_size);
            thr_pkt.cache_hit = 1;
            thr_pkt.file_size = atoi(len);
            
            //dbg("Sending request ACK..\n");
            send_message(seg->mq_data_tx, (void*)&thr_pkt, sizeof(thr_pkt), thr_pkt.segment_index);
            

            
            ASSERT(found);
            
            
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
        send_message(_seg->mq_data_tx, (void*)&thr_pkt, sizeof(thr_pkt), 0);

        total_bytes_rx += n;
        dbg("4) seg: %d, n: %d, fs: %d, chunk: %d, offset:%d, %d/%d..\n", _seg->segment_index, n, thr_pkt.file_size, thr_pkt.chunk_size, offset, total_bytes_rx, thr_pkt.file_size);

        //dbg("Waiting for data ACK\n");
        n = mq_receive(_seg->mq_data_rx, (void*)&thr_pkt, sizeof(thr_pkt), 0);
        ASSERT(n >= 0);
        
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



