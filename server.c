/* csci4061 S2016 Assignment 4
* section: one_digit_number
* date: mm/dd/yy
* names: Name of each member of the team (for partners)
* UMN Internet ID, Student ID (xxxxxxxx, 4444444), (for partners)
*/

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "util.h"

#define MAX_THREADS 100
#define MAX_QUEUE_SIZE 100
#define MAX_REQUEST_LENGTH 1024

//Structure for a single request.
typedef struct request
{
  int m_socket;
  char  m_szRequest[MAX_REQUEST_LENGTH];
} request_t;

/* The mutex lock */
pthread_mutex_t request_queue_access= PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t log_access =  PTHREAD_MUTEX_INITIALIZER;
/* the condiitonal variables */
pthread_cond_t some_request = PTHREAD_COND_INITIALIZER;
pthread_cond_t free_slot = PTHREAD_COND_INITIALIZER;

/* the bounded buffer to store request*/
request_t* request_queue;
int queue_size;
/* request counter and in for insert position and out for push position */
int count=0;
int in=0;
int out=0;
//root direcotry of web
char* root_directory;
//log file fp
FILE *log_fp;

void insert_queue(request_t* request){
  pthread_mutex_lock(&request_queue_access);
  while (count == MAX_QUEUE_SIZE){
    pthread_cond_wait(&free_slot, &request_queue_access);
  }
  request_queue[in] =*request;
  in = (in+1)%queue_size;
  count++;
  pthread_cond_signal(&some_request);
  pthread_mutex_unlock(&request_queue_access);
}

void pop_queue(request_t* request){
  pthread_mutex_lock(&request_queue_access);
  while(count == 0){
    pthread_cond_wait(&some_request, &request_queue_access);
  }
  *request = request_queue[out];
  out = (out+1)%queue_size;
  count--;
  pthread_cond_signal(&free_slot);
  pthread_mutex_unlock(&request_queue_access);
}

void log_request(int thread_id, int num_requests, request_t* request, int log_content){
  pthread_mutex_lock(&log_access);
  //error for file not found
  if (log_content<0){
    fprintf(log_fp, "[%d][%d][%d][%s][%s]\n", thread_id, num_requests, request->m_socket, request->m_szRequest, "File not found");
  }
  //print num_bytes_read to log
  else{
    fprintf(log_fp, "[%d][%d][%d][%s][%d]\n", thread_id, num_requests, request->m_socket, request->m_szRequest, log_content);
  }
  //ensure what just wrote went to the disk
  fflush(log_fp);
  pthread_mutex_unlock(&log_access);
}
/*repeatedly accept an incoming connection,
read the request from the connection,
insert the request in the request queue
*/
void * dispatch(void * arg)
{
  //assigne ONE file discriptor to each thread to accept the connection
  int fd;
  char filename[1024];
  request_t * request;
  request = (request_t*) malloc(sizeof(request_t));
  while ((fd =accept_connection())>=0){
    //repeatedly read the request from the connection
    if (get_request(fd, filename)!=0){
      //when get faulty request, do NOT exit, but recover it by "continue"
      printf("get_request failure due to faulty requests\n");
      continue;
    }

    request->m_socket =fd;
    strcpy(request->m_szRequest,filename);
    insert_queue(request);
  }
  free(request);
  return NULL;
}

void * worker(void * arg)
{
  int thread_id =* (int*)arg;
  int num_requests=0;
  int file_size;
  char *read_buff;
  char content_type[15];
  int num_bytes_read;
  request_t * request;
  request = (request_t*) malloc(sizeof(request_t));
  //repeatedly monitor the request queue and pick up the request from it
  while (1){
    pop_queue(request);
    /*serve the request back to the client by using return_result()
    if there was any problem with accessing the file, use return_error() instead
    */
    //obtain the file path to read the file associated wot the fd
    num_requests++;
    char path[MAX_REQUEST_LENGTH];
    strcpy(path, root_directory);
    strcat(path, request->m_szRequest);
    //open the file to obtain file pointer
    FILE * new_fp;
    if ((new_fp = fopen(path, "rb"))==NULL){
      printf("failed to open file in path:%s\n", path);
      log_request(thread_id, num_requests, request, -1);
      continue;
    }
    //obtain the actual size of file
    struct stat stat_struct;
    if(stat(path, &stat_struct)>=0){
      file_size = stat_struct.st_size;
    }
    else{
      fprintf(stderr, "failed to obtain the actual size of the file via stat\n");
      continue;
    }
    //read num_bytes_read from file
    read_buff = (char*) malloc(sizeof(char)* (file_size+1));
    num_bytes_read = fread(read_buff, sizeof(char), file_size, new_fp);
    if(num_bytes_read != file_size){
      fprintf(stderr, "failed to read file %s with size %d\n", path, file_size);
      log_request(thread_id, num_requests, request, -1);
      continue;
    }
    fclose(new_fp);
    //set content_type according to the suffix
    if(strstr(request->m_szRequest, ".html") || strstr(request->m_szRequest, ".htm") )
      strcpy(content_type, "text/html");
    else if (strstr(request->m_szRequest, ".jpg"))
      strcpy(content_type, "image/jpeg");
    else if (strstr(request->m_szRequest, ".gif"))
      strcpy(content_type, "image/gif");
    else
      strcpy(content_type, "text/plain");

    //return result/error to client
    if(return_result(request->m_socket, content_type,read_buff, file_size)!=0){
      return_error(request->m_socket, request->m_szRequest);
    }
    //log the successfully handled request
    log_request(thread_id, num_requests, request, num_bytes_read);

  }
  free(request);
  free(read_buff);
  return NULL;
}
/*run server as following
./web_server port path num_dispatchers num_workers qlen [cache_entries]
*/
int main(int argc, char **argv)
{
  //Create log file
  log_fp = fopen("web_server_log", "w+");
  if (log_fp == NULL){
      printf("Error in creating the log file!\n");
      return -1;
  }
  printf("successfully create the log file\n");
  //first error checking for the arguments in "./web_server port path num_dispatchers num_workers qlen [cache_entries]".
  if(argc != 6 && argc != 7)
  {
    printf("usage: %s port path num_dispatchers num_workers queue_length [cache_size]\n", argv[0]);
    return -1;
  }
  //check port
  int port = atoi(argv[1]);
  if(port <1025 || port >65535){
    fprintf(stderr, "port number should fall in 1025-65535, please input a valid port number\n");
    return -1;
  }
  //check path
  root_directory = (char*)malloc(sizeof(char) * strlen(argv[2]));
  strcpy(root_directory,argv[2]);
  if(chdir(root_directory)==-1){
    printf(stderr, "Invalid path! Failed to change to current working directory\n");
    return -1;
  }
  //check num_dispatchers
  int num_dispatchers = atoi(argv[3]);
  if(num_dispatchers > MAX_THREADS){
    fprintf(stderr, "Invalid num_dispatchers! It exceeds the maximum number of dispath threads\n");
    return -1;
  }
  //check num_workers
  int num_workers = atoi(argv[4]);
  if (num_workers> MAX_THREADS){
    fprintf(stderr, "Invalid num_workers! It exceeds the maximum number of workers\n");
    return -1;
  }
  //check length of queue
  queue_size = atoi(argv[5]);
  if (queue_size > MAX_QUEUE_SIZE){
    fprintf(stderr, "Invalid queue length! It exceeds the maximum length of queue\n");
    return -1;
  }
  //allocate memory to request queue based on input lenght
  if ((request_queue = (request_t*) malloc(queue_size * sizeof(request_t)))==NULL){
    fprintf(stderr, "failed to allocate memory to the request queue\n");
    return -1;
  };

  //initializes the connection once in the main thread
  init(port);
  printf("Starting server on port %d: %d disp, %d work\n", port, num_dispatchers, num_workers);
  /*
create an array of dispatchers and an array of workers,
each consists of threadId, regNum, path
  */
  int i;
  int error;
  pthread_t dispatchers[MAX_THREADS], workers[MAX_THREADS];
  //loop through each dispatcher to create dispatch thread
  for (i=0; i<num_dispatchers; i++){
    if ((error = pthread_create(&dispatchers[i], NULL, dispatch, NULL))!=0){
      fprintf(stderr, "fail to create dispatch thread %d: %s\n", i+1, strerror(error));
      return -1;
    }
  }
  //loog through each worker to create each worker thread
  int thread_ids[num_workers];
  for (i=0; i<num_workers ; i++){
    thread_ids[i]=i;
    if((error=pthread_create(&workers[i], NULL, worker, (void*)&thread_ids[i]))!=0){
      fprintf(stderr, "fail to create worker thread %d: %s\n", i+1, strerror(error));
      return -1;
    }
  }
  printf("Call init() first and make a dispatcher and worker threads\n");
  // join/wait the dispatch threads
  for(i=0; i< num_dispatchers; i++){
    if (error=pthread_join(dispatchers[i], NULL)){
      fprintf(stderr,"failed to join the dispatch thread %d: %s\n", i+1, strerror(error));
    }
  }
  // join/wait the worker threads
  for(i=0; i< num_workers; i++){
    if (error=pthread_join(workers[i], NULL)){
      fprintf(stderr,"failed to join the worker thread %d: %s\n", i+1, strerror(error));
    }
  }
  /*clean up
  close the log file
  destroy the mutex
  free any malloc
  */

  fclose(log_fp);

  if(pthread_mutex_destroy(&request_queue_access))
    fprintf(stderr, "failed to destroy mutex lock request_queue_access\n");
  if(pthread_mutex_destroy(&log_access))
    fprintf(stderr, "failed to destropy muxtex lock log_access\n");
  if(pthread_cond_destroy(&some_request))
  fprintf(stderr, "failed to destropy conditional variable some_request\n");
  if(pthread_cond_destroy(&free_slot))
  fprintf(stderr, "failed to destropy conditional variable free_slot\n");

  free(request_queue);
  free(root_directory);
  return 0;
}
