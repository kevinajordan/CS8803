#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include "gfserver.h"
#include "content.h"
#include "steque.h"

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  webproxy [options]\n"                                                      \
"options:\n"                                                                  \
"  -p                  Listen port (Default: 8888)\n"                         \
"  -c                  Content file mapping keys to content files\n"          \
"  -h                  Show this help message\n"                              

extern ssize_t handler_get(gfcontext_t *ctx, char *path, void* arg);
extern pthread_mutex_t  req_mutex;
extern steque_t         req_queue;
extern pthread_cond_t   req_cond;
extern void handler_init();

/* Main ========================================================= */
int main(int argc, char **argv) {
  int option_char = 0, num_threads = 1;
  unsigned short port = 8888;
  char *content = "content.txt";
  gfserver_t *gfs;

  // Parse and set command line arguments
  while ((option_char = getopt(argc, argv, "p:t:s:h")) != -1) {
    switch (option_char) {
      case 'p': // listen-port
        port = atoi(optarg);
        break;
      case 't': // num of threads
        num_threads = atoi(optarg);
        if (num_threads < 1){
            num_threads = 1;
        }
        break;
      case 'c': // file-path
        content = optarg;
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
  
  content_init(content);

  handler_init(num_threads);
    
  /*Initializing server*/
  gfs = gfserver_create();

  /*Setting options*/
  gfserver_set_port(gfs, port);
  gfserver_set_maxpending(gfs, 100);
  gfserver_set_handler(gfs, handler_get);
  gfserver_set_handlerarg(gfs, NULL);

  /*Loops forever*/
  gfserver_serve(gfs);
}