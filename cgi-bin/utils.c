#define MAX_STRLEN 100000
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

long getsec()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec;
}

double gettime()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void reverse(char *t)
{
  int len = strlen(t);
  int i;
  for(i = 0; i < len/2; i++){
    t[i] ^= t[len-i-1];
    t[len-i-1] ^= t[i];
    t[i] ^= t[len-i-1];
  }
  return;
}

void sort(char* t){
  int len = strlen(t);
  int i, j;
  int min;

  for(i = 0; i < len; i++){
    min = i;
    if (t[i] == '\n') continue;
    for(j = i + 1; j < len; j++){
      if(t[j] < t[min] && t[j] != '\n'){
        min = j;
      }
    }
    if(i != min){
      t[i] ^= t[min];
      t[min] ^= t[i];
      t[i] ^= t[min];
    } 
  }
}

void right(char *t){
  char *copy;

  int len = strlen(t);
  
  copy = strdup(t + len/2);
  strcat(t, copy);
  free(copy);

  if(len > 2)
    right(t + len);
}
