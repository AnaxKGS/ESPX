#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
 
#define QUEUESIZE 10
int LOOP;
int producers;
int consumers;
 
double total_time;
int total;
  
void *producer (void *args);
void *consumer (void *args);
 
void sincalc(void*);
void coscalc(void*);
void *functions[2] = {&sincalc, &coscalc};
 
typedef struct {
    void *(*work)(void*);
    void *arg;
} workFunction;
 
typedef struct {
  workFunction buf[QUEUESIZE];
  long head, tail;
  int full, empty;
  pthread_mutex_t *mut;
  pthread_cond_t *notFull, *notEmpty;
  double stime[QUEUESIZE];
} queue;
 
void sincalc(void* a){
    printf("The sine of %f is %f\n", *(float*) a, sin(*(float*) a));
}
void coscalc(void* a){
    printf("The cosine of %f is %f\n", *(float*)a, cos(*(float*) a));
}
 
queue *queueInit (void);
void queueDelete (queue *q);
void queueAdd (queue *q, workFunction in);
void queueDel (queue *q, workFunction *out);
int flag;
 
int main(int argc, char const *argv[])
{
  if (argc!=4){
    printf("Need more arguments\nLOOP producers consumers\n");
    exit(-1);
  }
  LOOP = atoi(argv[1]);
  producers = atoi(argv[2]);
  consumers = atoi(argv[3]);
  printf("%d %d %d %d\n", QUEUESIZE, LOOP, producers, consumers);
  flag=0;
  total_time=0;
  total=0;
  queue *fifo;
  pthread_t pro[producers], con[consumers];
  int rc;
 
  fifo = queueInit ();
  if (fifo ==  NULL) {
    fprintf (stderr, "main: Queue Init failed.\n");
    exit (1);
  }
 
  for(int t=0; t<producers; t++){
    rc = pthread_create (&pro[t], NULL, producer, fifo);
    if (rc){
        printf("Error!");
        exit(-1);
    }
  }
 
  for(int t=0; t<consumers; t++){
    rc = pthread_create (&con[t], NULL, consumer, fifo);
    if (rc){
        printf("Error!");
        exit(-1);
    }
  }
 
  for(int t=0; t<producers; t++){
    rc = pthread_join(pro[t], NULL);
    if (rc){
        printf("Error!");
        exit(-1);
    }
  }
 
  flag=1;
  pthread_cond_signal(fifo->notEmpty);
 
  for(int t=0; t<consumers; t++){
    rc = pthread_join(con[t], NULL);
    if (rc){
        printf("Error!");
        exit(-1);
    }
  }
 
  queueDelete (fifo);
 
  printf("Average Wait Time: %f seconds\nAfter %d tasks completed\n", total_time/total,total);
 
  return 0;
}
 
void *producer (void *q)
{
  srand((int)time(NULL) ^ (int)pthread_self());
  queue *fifo;
  int i;
  workFunction w[LOOP];
  float arg[LOOP];
  fifo = (queue *)q;
 
  for (i = 0; i < LOOP; i++) {
    pthread_mutex_lock (fifo->mut);
    while (fifo->full) {
      printf ("producer: queue FULL.\n");
      pthread_cond_wait (fifo->notFull, fifo->mut);
    }
    w[i].work = functions[rand()%2];
    arg[i] = ((double)rand() / (double)RAND_MAX ) * 2 * M_PI;
    w[i].arg = &arg[i];
 
    queueAdd (fifo, w[i]);
    pthread_mutex_unlock (fifo->mut);
    pthread_cond_signal (fifo->notEmpty);
  }
 
  return (NULL);
}
 
void *consumer (void *q)
{
  queue *fifo;
  workFunction d;
 
  fifo = (queue *)q;
 
  while(1) {
    pthread_mutex_lock (fifo->mut);
    while (fifo->empty) {
      printf ("consumer: queue EMPTY.\n");
      pthread_cond_wait(fifo->notEmpty, fifo->mut);
      if (flag==1){
        pthread_mutex_unlock (fifo->mut);
        pthread_cond_signal (fifo->notEmpty);
        return (NULL);
      }
    }
    queueDel (fifo, &d);
    d.work(d.arg);
    pthread_mutex_unlock (fifo->mut);
    pthread_cond_signal (fifo->notFull);
  }
}
 
queue *queueInit (void)
{
  queue *q;
 
  q = (queue *)malloc (sizeof (queue));
  if (q == NULL) return (NULL);
 
  q->empty = 1;
  q->full = 0;
  q->head = 0;
  q->tail = 0;
  q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
  pthread_mutex_init (q->mut, NULL);
  q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notFull, NULL);
  q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notEmpty, NULL);  
 
  return (q);
}
 
void queueDelete (queue *q)
{
  pthread_mutex_destroy (q->mut);
  free (q->mut);
  pthread_cond_destroy (q->notFull);
  free (q->notFull);
  pthread_cond_destroy (q->notEmpty);
  free (q->notEmpty);
  free (q);
}
 
void queueAdd (queue *q, workFunction in)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
 
  q->buf[q->tail] = in;
  q->stime[q->tail] = tv.tv_sec + 1.0e-06 * tv.tv_usec;
  q->tail++;
  if (q->tail == QUEUESIZE)
    q->tail = 0;
  if (q->tail == q->head)
    q->full = 1;
  q->empty = 0;
 
  return;
}
 
void queueDel (queue *q, workFunction *out)
{
  *out = q->buf[q->head];
 
  struct timeval tv;
  double current;
 
  gettimeofday(&tv, NULL);
  current = tv.tv_sec + 1.0e-06 * tv.tv_usec;
  total_time += current - q->stime[q->head];
  total += 1;
  printf("Wait time: %f seconds\n", current - q->stime[q->head]);
 
  q->head++;
  if (q->head == QUEUESIZE)
    q->head = 0;
  if (q->head == q->tail)
    q->empty = 1;
  q->full = 0;
 
  return;
}