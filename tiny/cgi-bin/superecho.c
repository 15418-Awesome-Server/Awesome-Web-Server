#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLINE 8192

int main(int argc, char **argv)
{
  char content[MAXLINE];
  int i;

  /* Make the HTML response */
  sprintf(content, "<html><head><title>THE SUPER ECHO</title></head>\r\n");
  sprintf(content, "%s<body><p>\r\n", content);

  /* Print out arguments here */
  for(i = 0; i < argc; i++)
    sprintf(content, "%s%s<br />\r\n", content, argv[i]);

  sprintf(content, "%s</p></body></html>", content);

  /* Make the HTTP response */
  printf("Content-length: %d\r\n", strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout);
  exit(0);
}
