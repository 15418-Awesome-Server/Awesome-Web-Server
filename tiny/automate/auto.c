#include "../csapp.h"
#include <mpi.h>
#include <sys/time.h>

#define STRESS_TIME 15
#define FILES_LENGTH 26
#define FILES_STEP 17

static char *files[] = { "home.html",
                         "godzilla.jpg",
                         "elasticity/pics.html",
                         "elasticity/saki1.png",
                         "elasticity/saki2.png",
                         "elasticity/saki3.png",
                         "elasticity/screenshot.png",
                         "elasticity/rinshankaihou.png",
                         "elasticity/sarah.png",
                         "elasticity/scan.png",
                         "elasticity/schedule.png",
                         "elasticity/select.png",
                         "elasticity/sheepherder.png",
                         "elasticity/shenanigans.png",
                         "elasticity/skypehelp.png",
                         "elasticity/smith.png",
                         "elasticity/smug.png",
                         "elasticity/smugtroll.png",
                         "elasticity/socold.png",
                         "elasticity/sonic.png",
                         "elasticity/sotrivial.png",
                         "automate/src1.html",
                         "automate/src2.html",
                         "automate/src3.html",
                         "automate/src4.html",
                         "automate/src5.html"
};

double gettime()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

int main(int argc, char *argv[])
{
  int procID;
  double start, end;
  double runStart, runEnd;
  char *hostname;
  char *blk_ptr;
  char *tok, *portTok;
  char *redirFile;
  int port;
  int file;
  char buf[8192];
  char pathBuf[8192];
  char resp[8192];
  int fd;
  rio_t rio;
  int num_requests = 0;

  if (argc != 3)
  {
    printf("Need a hostname and a port!\n");
    return 1;
  }

  blk_ptr = (char *)calloc(8192, 1);
  hostname = blk_ptr;
  strcpy(hostname, argv[1]);

  port = atoi(argv[2]);

  MPI_Init(&argc, &argv);

  MPI_Comm_rank(MPI_COMM_WORLD, &procID);
  file = procID;

  start = gettime();
  runStart = start;
  end = start + STRESS_TIME;

  while(num_requests < 10)
  {
/*    printf("Getting redirect response...");
    fflush(stdout); */
    /* Get redirection response */
    fd = open_clientfd(hostname, port);

    rio_readinitb(&rio, fd);
    sprintf(buf, "GET /%s HTTP/1.0\r\n\r\n", files[file % FILES_LENGTH ]);
    rio_writen(fd, buf, strlen(buf));
/*    printf("done.\n"); */

/*    printf("Reading redirect response...");
    fflush(stdout); */
    rio_readlineb(&rio, resp, MAXLINE);
    rio_readlineb(&rio, resp, MAXLINE);
/*    printf("done.\n"); */

    close(fd);
    
    /* resp now has the Location header in it, so parse it */
    tok = strtok(resp, " ");
    tok = strtok(NULL, " ");

    /* tok now has the URI with the port in it. Parse these out. */
    tok = strtok(tok, ":");
    hostname = strtok(NULL, ":");
    hostname += 2;

    /* hostname now has the host to go to, so get port */
    tok = strtok(NULL, ":");
    strcpy(pathBuf, tok);
    portTok = strtok(tok, "/");

    port = atoi(portTok);
    redirFile = pathBuf + strlen(portTok);

    printf("%s %d %s", hostname, port, redirFile);

/*    printf("Sending object request..."); 
    fflush(stdout); */
    /* Fully parsed out, now get the actual requested object */
    fd = open_clientfd(hostname, port);

    rio_readinitb(&rio, fd);
    sprintf(buf, "GET %s HTTP/1.0\r\n\r\n", redirFile);
    rio_writen(fd, buf, strlen(buf));
/*    printf("done.\n"); */

/*    printf("Reading object response...");
    fflush(stdout); */
    while(rio_readlineb(&rio, resp, MAXLINE) > 0);
/*    printf("done.\n"); */

/*    printf("Closing connection...");
    fflush(stdout); */
    close(fd);
/*    printf("done.\n"); */

    file += FILES_STEP;
    strcpy(hostname, argv[1]);
    port = atoi(argv[2]);
    strcpy(redirFile, "\0");
    strcpy(pathBuf, "\0");

    num_requests++;
    
  }
  printf("procID %d about to finalize\n", procID);

  MPI_Finalize();

  runEnd = gettime();

  printf("Time elapsed: %.4f\n", runEnd - runStart);

  return 0;

}
