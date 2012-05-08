#include "../csapp.h"
#include <mpi.h>
#include <sys/time.h>

#define STRESS_TIME 100 
#define FILES_LENGTH 35
#define FILES_STEP 17
#define MAX_REQUESTS 100
#define STAGGER_INTERVAL 5

static char *files[] = { "home.html",
                         "godzilla.jpg",
                         "elasticity/pics.html",
                         "elasticity/saki.png",
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
                         "automate/src5.html",
                         "cgi-bin/a?1",
                         "cgi-bin/a?2",
                         "cgi-bin/a?3",
                         "cgi-bin/b?1",
                         "cgi-bin/b?2",
                         "cgi-bin/b?3",
                         "cgi-bin/c?1",
                         "cgi-bin/c?2",
                         "cgi-bin/c?3",
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
  double start, end, cur, br;
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
  int total = 0;

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

  while(gettime() < start + STAGGER_INTERVAL * procID);  

  runStart = gettime();
  br = runStart;
  end = runStart + STRESS_TIME;

  printf("procID %d starting\n", procID);

  while(gettime() < end)
  {
/*    printf("Getting redirect response...");
    fflush(stdout); */
    /* Get redirection response */
    fd = open_clientfd(hostname, port);

    rio_readinitb(&rio, fd);
    sprintf(buf, "GET /%s HTTP/1.0\r\n\r\n", files[file % FILES_LENGTH ]);
    rio_writen(fd, buf, strlen(buf));
/*    printf("done.\n"); */

    rio_readlineb(&rio, resp, MAXLINE);
    rio_readlineb(&rio, resp, MAXLINE);

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

  printf("procID %d done\n", procID);
  MPI_Reduce(&num_requests, &total, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

  MPI_Finalize();

  runEnd = gettime();

  if (procID == 0)
    printf("Total number of fulfilled requests: %d\n", total);

  return 0;

}
