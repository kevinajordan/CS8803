#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct blob_t blob_t;

struct blob_t {
  char buffer[4096];

  int done;
  pthread_mutex_t done_mutex;
  pthread_cond_t done_cv;
};

int main(int argc, char** argv) {
  if (argc == 1) {
    // this branch is the producer
    int shm_fd = shm_open("/salty", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(blob_t));

    void* ptr = mmap(NULL, sizeof(blob_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) {
      printf("Map failed #1\n");
      return -1;
    }
    close(shm_fd);

    blob_t* blob = (blob_t*) ptr;

    // now we have a blob at a location in shared memory. let's
    // initialize the blob to say that it isn't done yet, and put a
    // message in there

    blob->done = 0;
    sprintf(blob->buffer, "%s", "LTB #3");

    // we also have to initialize the blob-internal mutex and cv that
    // we're going to use to have the producer process block and wait

    pthread_mutexattr_t attrmutex;
    pthread_mutexattr_init(&attrmutex);
    pthread_mutexattr_setpshared(&attrmutex, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&blob->done_mutex, &attrmutex);

    pthread_condattr_t attrcond;
    pthread_condattr_init(&attrcond);
    pthread_condattr_setpshared(&attrcond, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&blob->done_cv, &attrcond);

    // use the blob-internal (which is to say, shared!) mutex and cv
    // to wait for the consumer process to tell us we're done

    pthread_mutex_lock(&blob->done_mutex);
    while (blob->done == 0) {
      pthread_cond_wait(&blob->done_cv, &blob->done_mutex);
    }
    pthread_mutex_unlock(&blob->done_mutex);

    // at this point the consumer must have finished its work, so we
    // can unmap and unlink the shared memory

    munmap(ptr, sizeof(blob_t));
    shm_unlink("/shm_scratch");

    // and exit

    return 0;
  } else {
    // this branch is the consumer
    int shm_fd = shm_open("/salty", O_CREAT | O_RDWR, 0666);

    void* ptr = mmap(NULL, sizeof(blob_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) {
      printf("Map failed #2\n");
      return -1;
    }
    close(shm_fd);

    blob_t* blob = (blob_t*) ptr;

    // all the above gets us a reference to a blob in shared memory,
    // but not just any old blob, it's the one that the producer
    // created!

    // so let's find out what amazing message the producer left here for us
    printf("Found a message in shared memory: %s\n", blob->buffer);

    // the producer is waiting for us to say we're done, so let's let
    // it know that we are
    pthread_mutex_lock(&blob->done_mutex);
    blob->done = 1;
    pthread_cond_signal(&blob->done_cv);
    pthread_mutex_unlock(&blob->done_mutex);

    // now that we're done with this shared memory, we can unmap it
    // (note that we shouldn't unlink it from here, we didn't create
    // the page, so we're not responsible for destroying it -- that's
    // the producer's job)

    munmap(ptr, sizeof(blob_t));

    // and exit

    return 0;
  }
}
