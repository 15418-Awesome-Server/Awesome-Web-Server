#include <stdlib.h>
#include <stdio.h>

int main()
{
  printf("%c\n", '\x3b');

  char thing[] = { '3', 'B' };
  int d;
  sscanf(thing, "%x", &d);
  printf("%d\n", d);
  char c = (char)d;
  printf("%c\n", c);
}
