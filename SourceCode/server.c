/* csci4061 S2016 Assignment 4
* section: one_digit_number
* date: mm/dd/yy
* names: Name of each member of the team (for partners)
* UMN Internet ID, Student ID (xxxxxxxx, 4444444), (for partners)
*/
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "util.h"
#include <errno.h>

#define MAX_THREADS 100
#define MAX_QUEUE_SIZE 100
#define MAX_REQUEST_LENGTH 1024

//Structure for a single request.
typedef struct request
{
        int             m_socket;
        char    m_szRequest[MAX_REQUEST_LENGTH];
} request_t;

char* path;
request_t req_queue[MAX_QUEUE_SIZE];
int request_num = 0;
int num_workers;
pthread_mutex_t L;
pthread_cond_t CV_free_space = PTHREAD_COND_INITIALIZER, CV_request = PTHREAD_COND_INITIALIZER;

void * dispatch(void * arg)
{
    int fd_dispatcher;

    //Dispatchers loop indefinitley to get requests
    while(1){
      //Check if request buffer full. If it is, wait until space is free


      if ((fd_dispatcher = accept_connection()) < 0){
        fprintf(stderr, "Failed to accept connection. Please retry.\n");
        break;
      }
      //Lock request queue
      pthread_mutex_lock(&L);
      if (request_num == num_workers){
        pthread_cond_wait(&CV_free_space,&L);
      }
      /*Put request in request buffer. If no errors, alert workers
      of new request and unlock buffer*/
      if (get_request(fd_dispatcher, req_queue[request_num].m_szRequest)==0){
        if ((req_queue[request_num].m_socket = fd_dispatcher)!=0){
          fprintf(stderr, "Request was for: %s and socket fd is: %d\n",req_queue[request_num].m_szRequest,  req_queue[request_num].m_socket);
          request_num++;
          pthread_cond_signal(&CV_request);
          pthread_mutex_unlock(&L);
        }
        else
          fprintf(stderr,"Socket creation failed.\n"); //TODO Are we supposed to exit here?
      }
    }
    return NULL;
}

void * worker(void * arg)
{

  int path_length;
  request_t request;
  FILE *f;
  char *buff;
  char *type;

  while(1){
    //If no requests, block until there is one
    pthread_mutex_lock(&L);
    if (request_num==0){
      pthread_cond_wait(&CV_request,&L);
    }
    getcwd(buff, 100);
    fprintf(stderr, "%s\n",buff);
    //If there are requestes to service, retrieve it and release queue
    request = req_queue[request_num-1];
    request_num--;
    fprintf(stderr, " %s\n", request.m_szRequest);

    pthread_cond_signal(&CV_free_space);
    pthread_mutex_unlock(&L);
    if ((f = fopen(request.m_szRequest, "rb"))==NULL){
      fprintf(stderr, "File failed to open. %s\n", strerror(errno));
    }
    //Get file length
    int fileLen;
	  fseek(f, 0, SEEK_END);
	  fileLen=ftell(f);
	  fseek(f, 0, SEEK_SET);
    buff = (char*)malloc(fileLen+1); //TODO Error check this?

    if (fread(buff, fileLen, 1, f) == 0){
      fprintf(stderr, "File empty or could not read it.\n");
    }
    int file_size = sizeof(buff);
    char file_type[12];
    type = strchr(request.m_szRequest, '.');
    fprintf(stderr, "%s\n", type);
  }
  return NULL;
}

int main(int argc, char **argv)
{
      //Error check first.
      if(argc != 6 && argc != 7)
      {
              printf("usage: %s port path num_dispatcher num_workers queue_length [cache_size]\n", argv[0]);
              return -1;
      }

      //Save command line arguments for use in initialization
      int port_num=atoi(argv[1]);
      if (chdir(argv[2])==0){
          fprintf(stderr, "%s\n",strerror(errno));
      }
      char* buff;
      if (getcwd(buff, 100)==NULL){
        fprintf(stderr, "%s\n",strerror(errno));
      }
      fprintf(stderr, "%s\n",buff);
      int dthread_num=atoi(argv[3]);
      int wthread_num=atoi(argv[4]);
      int queue_size=atoi(argv[5]);

      num_workers = wthread_num;

      //initialize mutex
      if (pthread_mutex_init(&L, NULL) != 0)
      {
          fprintf(stderr, "Mutex failed to initialize\n");
          exit(0);
      }

      /*Check that all assumed conditions are met
      otherwise tell user argument max values*/
      if(port_num>=1024 && port_num<=9000 && dthread_num<=MAX_THREADS &&
        wthread_num<=MAX_THREADS && queue_size<=MAX_QUEUE_SIZE)
      {
        init(port_num);
        pthread_t dispatcher_tid[dthread_num], worker_tid[wthread_num];

        printf("Call init() first and make a dispatcher and worker threads\n");
        for (int i = 0; i < dthread_num; i++)
        {
           if(pthread_create (&(dispatcher_tid[i]), NULL, &dispatch, NULL)!=0){
             fprintf(stderr, "Failed to create a dispatcher thread.\n");
             exit(0);
           }
           //pthread_detach(dispatcher_tid[i]); //TODO Is this what we want?
        }
        for (int i = 0; i < wthread_num; i++)
        {
           if(pthread_create (&(worker_tid[i]), NULL, &worker, NULL)!=0){
             fprintf(stderr, "Failed to create a dispatcher thread.\n");
             exit(0);
           }
           //pthread_detach(worker_tid[i]); //TODO Is this what we want?

        }
        //TODO Or do we wnat this?
        /*Should never unblock here unless major failure
        and all threads exit*/
        for (int i = 0; i < dthread_num; i++) {
          pthread_join(dispatcher_tid[i], NULL);
        }
        for (int i = 0; i < wthread_num; i++) {
          pthread_join(worker_tid[i], NULL);
        }
      }
      else
        printf("Max of %d threads or queue size %d exceeded.\n", MAX_THREADS, MAX_QUEUE_SIZE);

      pthread_mutex_destroy(&L);
      return 0;
}
