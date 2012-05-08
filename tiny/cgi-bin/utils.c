#include <string.h>
#include <stdlib.h>

void reverse(char *s)
{
  int len = strlen(s);
  int i;
  for(i = 0; i < len/2; i++){
    s[i] = s[len-i-1];
  }
  return s;
}

void sort(char* s){
  int len = strlen(s);
  int i, j;
  int max;

  for(i = 0; i < len; i++){
    max = i;
    for(j = i + 1; j < len; j++){
      if(s[j] > s[max]){
        max = i;
      }
    }
    if(i != max){
      s[i] ^= s[max];
      s[max] ^= s[i];
      s[i] ^= s[max];
    }
  }
}

/* This should print out the right half of s recursively. Currently it does
 * something weird that isn't that. Fix. */
void right(char *s){
  int len = strlen(s);
  sprintf(s, "%s%s\n", s, s+len/2);
  if(len > 0)
    right(s+len/2);
}
