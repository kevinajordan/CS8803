#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "shm_channel.h"

#include "gfserver.h"
                                                                \
#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webproxy [options]\n"                                                     \
"options:\n"                                                                  \
"  -n number of segments to use in communication with cache.\n"               \
"  -z the size (in bytes) of the segments. \n"                                    \
"  -p [listen_port]    Listen port (Default: 8888)\n"                         \
"  -t [thread_count]   Num worker threads (Default: 1, Range: 1-1000)\n"      \
"  -s [server]         The server to connect to (Default: Udacity S3 instance)"\
"  -h                  Show this help message\n"                              \
"special options:\n"                                                          \
"  -d [drop_factor]    Drop connects if f*t pending requests (Default: 5).\n"


/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
  {"nsegments",     required_argument,      NULL,           'n'},
  {"segsize",       required_argument,      NULL,           'z'},
  {"port",          required_argument,      NULL,           'p'},
  {"thread-count",  required_argument,      NULL,           't'},
  {"server",        required_argument,      NULL,           's'},         
  {"help",          no_argument,            NULL,           'h'},
  {NULL,            0,                      NULL,             0}
};

extern ssize_t handle_with_cache(gfcontext_t *ctx, char *path, void* arg);

static gfserver_t gfs;

mqd_t tx_mqd;
mqd_t rx_mqd;
mqd_t ctrl_mq_tx;
mqd_t ctrl_mq_rx;
char* cmq_tx_str = "/control-proxy-cache";
char* cmq_rx_str = "/control-cache-proxy";

pthread_mutex_t seg_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  seg_cond  = PTHREAD_COND_INITIALIZER;

static void _sig_handler(int signo){
  if (signo == SIGINT || signo == SIGTERM){
    gfserver_stop(&gfs);
    exit(signo);
  }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
  int i, option_char = 0;
  unsigned short port = 8888;
  unsigned short nworkerthreads = 1;
    int num_segments = 10, segment_size = 1024;
  //char *server = "s3.amazonaws.com/content.udacity-data.com";

  if (signal(SIGINT, _sig_handler) == SIG_ERR){
    fprintf(stderr,"Can't catch SIGINT...exiting.\n");
    exit(EXIT_FAILURE);
  }

  if (signal(SIGTERM, _sig_handler) == SIG_ERR){
    fprintf(stderr,"Can't catch SIGTERM...exiting.\n");
    exit(EXIT_FAILURE);
  }

  // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "n:z:p:t:s:h", gLongOptions, NULL)) != -1) {
        switch (option_char) {
            case 'n':
                num_segments = atoi(optarg);
                break;
            case 'z':
                segment_size = atoi(optarg);
                break;
            case 'p': // listen-port
                port = atoi(optarg);
                break;
            case 't': // thread-count
                nworkerthreads = atoi(optarg);
                break;
            case 's': // file-path
//                server = optarg;
                break;
            case 'h': // help
                fprintf(stdout, "%s", USAGE);
                exit(0);
                break;
            default:
                fprintf(stderr, "%s", USAGE);
                exit(1);
        }
    }
    
    ctrl_msg ctrl;
    ctrl.num_segments = num_segments;
    ctrl.segment_size = segment_size;
  
    /* SHM initialization...*/



    
    //---------------- Sync ------------------
    ctrl_mq_tx = create_message_queue(cmq_tx_str, O_CREAT | O_RDWR,  sizeof(thread_packet), MAX_MSGS);
    ctrl_mq_rx = create_message_queue(cmq_rx_str, O_CREAT | O_RDWR,  sizeof(thread_packet), MAX_MSGS);
    


    tx_mq(ctrl_mq_tx, (char*)&ctrl, sizeof(thread_packet));
    rx_mq(ctrl_mq_rx, (char*)&ctrl, sizeof(thread_packet));
    


    //---------------- Sync ------------------
   
    
    
    steque_t* segment_q = (steque_t*) malloc(sizeof(steque_t));
    shm_create_segments(segment_q, ctrl.num_segments, ctrl.segment_size, 1);

    /*Initializing server*/
    gfserver_init(&gfs, nworkerthreads);

    /*Setting options*/
    gfserver_setopt(&gfs, GFS_PORT, port);
    gfserver_setopt(&gfs, GFS_MAXNPENDING, 10);
    gfserver_setopt(&gfs, GFS_WORKER_FUNC, handle_with_cache);
    for(i = 0; i < nworkerthreads; i++)
    gfserver_setopt(&gfs, GFS_WORKER_ARG, i, segment_q);

    /*Loops forever*/
    gfserver_serve(&gfs);
    
    mq_unlink(cmq_tx_str);
    mq_unlink(cmq_rx_str);
    
    mq_close(ctrl_mq_tx);
    mq_close(ctrl_mq_rx);

}