// futex_test.c
#include <pthread.h>
#include <stdio.h>
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
void *tfn(void *p){
  pthread_mutex_lock(&m);
  pthread_mutex_unlock(&m);
  return NULL;
}
int main(){
  pthread_t t;
  pthread_create(&t, NULL, tfn, NULL);
  pthread_join(t, NULL);
  printf("done\n");
  return 0;
}

