/* $begin Awesomemain */
/*
 * Awesome.c - A distributed, balanced, elastic web server capable of caching certain kinds
 * of responses. Each process, distributed over multiple machines, has one of three roles.
 *
 * ROOT - Also known as the "Dispatcher." Receives connection requests and sends an HTTP 307
 * redirect response to a worker server to actually serve the content. Also manages the
 * response cache, so in the right circumstances it will check the cache and potentially serve
 * the request itself.
 *
 * BALANCER - Responsible for the workload management of the server system. Keeps track of
 * the number of requests currently being serviced by each worker, and keeps the ROOT updated
 * on which worker to redirect incoming responses to for best performance. Additionally,
 * dynamically adds or removes workers from the system to deal with fluctuating traffic.
 *
 * WORKER - Serves the content requested by the client. Awesome requires that all dynamic
 * content be requested via POST; it then turns the request body into appropriate arguments
 * to run the C program specified in the request.
 */
#include "csapp.h"
#include "hosts.h"
#include "cache.h"
#include "cgi-bin/utils.c"
#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>

/* Define the max length of a large string */
#define MAX_STRLEN 100000

/* Define the MPI root process to be 0 and the balancer to be 1 */
#define ROOT 0
#define BALANCER 1

/* Define the length of a host address
 *
 * ghcXX.ghc.andrew.cmu.edu - 24 characters plus one null terminator */
#define HOST_LENGTH 25

/* Define number of initial servers (TODO: Change to command-line arg */
#define INIT_SERVERS 2 

/* Define minimum and maximum load factors */
#define MIN_THRESHOLD 0.3
#define MAX_THRESHOLD 2

/* Define the minimum size to cache something */
#define MIN_CACHEOBJ_SIZE 1000

/* Define loop limit */
#define LOOP_LIMIT 10

/* Define the time for off servers to sleep */
#define SLEEP_INTERVAL 1

/* Define MPI tag synonyms */
#define FLAG 0
#define NEW_REQUEST 1
#define REQUEST_DONE 2
#define CHANGE_REDIRECT 3

/* Define synonyms for the various HTTP methods */
#define METHODS_LENGTH 9
#define HEAD 0
#define GET 1
#define POST 2
#define PUT 3
#define DELETE 4
#define TRACE 5
#define OPTIONS 6
#define CONNECT 7
#define PATCH 8

/* Create the HTTP methods array */
static char* METHODS[] = {  "HEAD",
                            "GET",
                            "POST",
                            "PUT",
                            "DELETE",
                            "TRACE",
                            "OPTIONS",
                            "CONNECT",
                            "PATCH"
};


void doit(int fd, int procID);
int  read_requesthdrs(rio_t *rp);
int  parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, int met, rio_t* rp, int len, int procID);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, int met, rio_t* rp, int len);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);
int  find_method(char *method);
void respond_trace();
void respond_options();
void parse_body(char *body, char *args, int len);
void parse_percent(char *body, char *args, int *i, int *j);
int send_redirect(int fd, int host, rio_t *rio, int port);
void do_compute(char *s, int fd, char *cgiargs, int procID);
void print_array(int *arr, int len);

int main(int argc, char **argv) 
{
    int listenfd, connfd, port, clientlen;
    int on_flag = 0;
    int recv_flag;
    int procID;
    int *processes;
    int *rr_array;
    int i;
    struct sockaddr_in clientaddr;
    int redirect = 2;
    long *num_requests;
    int comm_size;
    int redirect_flag, request_flag, done_flag;
    int request_handler;
    int request_done;
    MPI_Status req_stat, update_stat, finish_stat, power_stat; 
    MPI_Request req, update, finish, power;
    MPI_Datatype HOST_TYPE;

    fflush(stdout);
    Signal(SIGPIPE, SIG_IGN);

    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &procID);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
    MPI_Type_vector(HOST_LENGTH, 1, 1, MPI_CHAR, &HOST_TYPE);

    /* Check command line args */
    if (argc != 2) {
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    	exit(1);
    }
    port = atoi(argv[1]) + procID;

    /* Initialize the cache */
    init_cache();

    processes = (int *)malloc(sizeof(int) * comm_size);
    rr_array = (int *)malloc(sizeof(int) * comm_size / 2);
    if (procID == ROOT) printf("rr_array: %p\n", rr_array);

    for (i = 0; i < comm_size / 2; i++)
      rr_array[i] = BALANCER + 1 + i;

    for(i = 0; i < comm_size; i++)
      processes[i] = i;

    /* Initialize the first servers */
    if (procID > BALANCER)
    {
      if (procID < BALANCER+1 + INIT_SERVERS)
        on_flag = 0;
      else
        on_flag = -1;
    }
    else
      on_flag = -2;

    /* Initialize the load balancer's array of number of requests per worker */
    num_requests = (long *)malloc(sizeof(long) * comm_size);
    
    /************************** Message testing code ************************
    if (procID == 0) {
      val = 4;
      MPI_Send(&val, 1, MPI_INT, 1, 1, MPI_COMM_WORLD);
    }

    if (procID == 1) {
      val = 1;
      printf("%d\n", val);
      MPI_Recv(&val, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
      printf("%d\n", val);
    }
     ************************** Message testing code ************************/

    /* Root dispatcher loop */
    if (procID == ROOT)
    {
      rio_t rio;
      int rr = 0;
      int i;

      listenfd = Open_listenfd(port);
      printf("procID %d is ROOT, waiting for connections with PID %d\n", procID, getpid());

      /* Post receive to change the client to redirect to */
      /*MPI_Irecv(rr_array, comm_size / 2, MPI_INT, BALANCER, CHANGE_REDIRECT, MPI_COMM_WORLD, &req);*/
      MPI_Irecv(&redirect, 1, MPI_INT, BALANCER, CHANGE_REDIRECT, MPI_COMM_WORLD, &req);

      while (1) {
        
        /* Test for reception of redirection message */
        MPI_Test(&req, &redirect_flag, &req_stat);

        /* If message was received, acknowledge and post new receive */
        while (redirect_flag)
        {
          /*MPI_Irecv(rr_array, comm_size / 2, MPI_INT, BALANCER, CHANGE_REDIRECT, MPI_COMM_WORLD, &req);*/
          MPI_Irecv(&redirect, 1, MPI_INT, BALANCER, CHANGE_REDIRECT, MPI_COMM_WORLD, &req);
          MPI_Test(&req, &redirect_flag, &req_stat);
        }

        clientlen=sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        rio_readinitb(&rio, connfd);

/*        printf("rr_array: [");
        for(i = 0; i < comm_size / 2; i++)
          printf("%d, ", rr_array[i]);
        printf("]\n"); */

        /* Select the actual redirect location here */
        /*while(rr_array[rr] < 0)*/
        /*{*/
          /*rr = (rr + 1) % (comm_size / 2);*/
        /*}*/

        /*send_redirect(connfd, rr_array[rr], &rio, port);*/
        send_redirect(connfd, redirect, &rio, port);
        Close(connfd);

        /* Tell load balancer to increment the number of requests being handled */
        /*MPI_Send(&(rr_array[rr]), 1, MPI_INT, BALANCER, NEW_REQUEST, MPI_COMM_WORLD);*/
        MPI_Send(&redirect, 1, MPI_INT, BALANCER, NEW_REQUEST, MPI_COMM_WORLD);

        rr = (rr + 1) % (comm_size / 2);
      }
    }

    if (procID == BALANCER)
    {
      int min_ranks[] = { 0, 0 };
      int reqs;
      int totalReqs = 0;
      int on = 1;
      int off = 0;
      int serversOn = INIT_SERVERS;
      int rr_array_length = (serversOn+1) / 2;
      int i, j;
      double old = 0;
      int printThing = 0;
      int new_request_limit = 0;
      int request_done_limit = 0;
      int max_rank, passed_on, new_max;


      for (i = 0; i < comm_size; i++)
      {
        if (i == ROOT || i == BALANCER) num_requests[i] = -2;
        else if (i > BALANCER && i < BALANCER+1 + INIT_SERVERS) num_requests[i] = 0;
        else num_requests[i] = -1;
      }
     
      printf("procID %d is BALANCER\n", procID);
      printf("[");
      for(i = 0; i < comm_size; i++)
        printf("%d ", num_requests[i]);
      printf("]\n");
      
      MPI_Irecv(&request_handler, 1, MPI_INT, ROOT, NEW_REQUEST, MPI_COMM_WORLD, &update);
      MPI_Irecv(&request_done, 1, MPI_INT, MPI_ANY_SOURCE, REQUEST_DONE, MPI_COMM_WORLD, &finish);
     
      while (1) {
        new_request_limit = 0;
        request_done_limit = 0;
        max_rank = 0;
        passed_on = 0;
        new_max = 0;
        rr_array_length = (serversOn+1) / 2;
        
        /*if (printThing % 1000000 == 0)
        {
          printThing = 0;
          printf("[");
          for(i = 0; i < comm_size; i++)
            printf("%d ", num_requests[i]);
          printf("]\n");
        }*/

        /* Test for reception of new request message */
        MPI_Test(&update, &request_flag, &update_stat);

        /* If message was received, acknowledge, update, and post new receive */
        while (request_flag)
        {
/*          printf("BALANCER notified of new request\n"); */

          if (num_requests[request_handler] > -1)
            num_requests[request_handler]++;

        /*  printf("Request new  at %d: [ ", request_handler);
          for(i = 0; i < comm_size; i++)
            printf("%d ", num_requests[i]);
          printf("]\n"); */

          MPI_Irecv(&request_handler, 1, MPI_INT, ROOT, NEW_REQUEST, MPI_COMM_WORLD, &update);  
          MPI_Test(&update, &request_flag, &update_stat);
        }

        /* Test for reception of a message indicating a request is fulfilled */
        MPI_Test(&finish, &done_flag, &finish_stat);

        /* If message was received, acknowledge, update, and post new receive */
        while (done_flag)
        {
  /*      printf("BALANCER notified that WORKER %d fulfilled requests\n", request_done); */
          if (num_requests[request_done] > -1)
            num_requests[request_done]--;
  
/*          printf("Request done at %d: [ ", request_done);
          for( i = 0; i < comm_size; i++)
            printf("%d ", num_requests[i]);
          printf("]\n"); */
  
          MPI_Irecv(&request_done, 1, MPI_INT, MPI_ANY_SOURCE, REQUEST_DONE, MPI_COMM_WORLD, &finish);
          MPI_Test(&finish, &done_flag, &finish_stat);
        }

        min_ranks[0] = BALANCER + 1;
        min_ranks[1] = BALANCER + 1;

        totalReqs = 0;

        /* Find total number of requests and create the array to send to the dispatcher */
/*        for(i = BALANCER + 1; i < comm_size; i++)
        {
          reqs = num_requests[i];

          /* Only consider servers that are on
          if (reqs > -1)
          {
            totalReqs += reqs;

            if (reqs < num_requests[min_ranks[0]] || num_requests[min_ranks[0]] < 0)
            {
              min_ranks[1] = min_ranks[0];
              min_ranks[0] = i;
            }

            /* Collect the first (serversOn+1)/2 servers into the array to send
            if (passed_on < rr_array_length)
            {
              rr_array[passed_on++] = i;
              if (reqs > num_requests[max_rank]) max_rank = i;
            }
            /* Select the minimally-full n/2 servers to sen
            else
            {
              new_max = 0;

              /* If the number of requests of this processor is less than the number of
               * requests on the most burdened processor, replace it and find the new
               * maximum
              if (reqs < num_requests[max_rank])
              {
                for(j = 0; j < rr_array_length; j++)
                {
                  if (rr_array[j] == max_rank)
                    rr_array[j] = i;
                  if (num_requests[rr_array[j]] > num_requests[new_max])
                    new_max = rr_array[j];
                }
                max_rank = new_max;
              }
            }
          }
          else if (reqs < -2)
          {
            if (getsec() + reqs > SLEEP_INTERVAL)
              num_requests[i] = 0;
          }
        }

        for(i = rr_array_length; i < comm_size / 2; i++)
          rr_array[i] = -1;*/

        /* Find the server with the fewest pending requests and select it */
        /* Elasticity: Find total number of requests */
        for (i = BALANCER + 1; i < comm_size; i++)
        {
          reqs = num_requests[i];
          if (reqs > -1)
          {
            totalReqs += reqs;
            if (reqs < num_requests[min_ranks[0]] || num_requests[min_ranks[0]] < 0)
            {
              min_ranks[1] = min_ranks[0];
              min_ranks[0] = i;
            }
          }
          if (reqs < -2)
          {
            if (getsec() + reqs > SLEEP_INTERVAL)
            {
              printf("Redirects now eligible to server %d\n", i);
              num_requests[i] = 0;
            }
          }
        }
        
        /*if (gettime() - old > 0)*/
        /*{*/
          old = gettime();
          if (serversOn > INIT_SERVERS && (double)totalReqs / serversOn < MIN_THRESHOLD)
          {
            /* Turn off server that was selected earlier */
            if (min_ranks[0] == redirect)
              min_ranks[0] = min_ranks[1];
            MPI_Isend(&off, 1, MPI_INT, min_ranks[0], FLAG, MPI_COMM_WORLD, MPI_REQUEST_NULL);
            num_requests[min_ranks[0]] = -1;
            serversOn--;
            continue;
          }

          /* Send the root its new array */
          /* printf("Sending ");
          print_array(rr_array, comm_size / 2); */
          /*MPI_Isend(rr_array, comm_size / 2, MPI_INT, ROOT, CHANGE_REDIRECT, MPI_COMM_WORLD, MPI_REQUEST_NULL);*/

          /* If different from before, tell the root */
          if (min_ranks[0] != redirect)
          {
            redirect = min_ranks[0];
            MPI_Isend(&redirect, 1, MPI_INT, ROOT, CHANGE_REDIRECT, MPI_COMM_WORLD, MPI_REQUEST_NULL);
          } 

          if (serversOn < comm_size - 2 && (double)totalReqs / serversOn > MAX_THRESHOLD)
          {
            /* Find first off server and turn it on */
            for (i = BALANCER + 1; i < comm_size; i++)
            {
              if (num_requests[i] == -1)
              {
                printf("Turning on server %d\n", i);
                MPI_Isend(&on, 1, MPI_INT, i, FLAG, MPI_COMM_WORLD, MPI_REQUEST_NULL);
                num_requests[i] = -1 * getsec();
                serversOn++;
                break;
              }
            }
          }
      }

     free(num_requests);
    }

    /* Worker servers loop */
    if (procID > BALANCER)
    {
      fd_set set;
      int new_con;
      struct timeval timeout;

      on_flag++;
      init_cache();
      listenfd = Open_listenfd(port);

      printf("procID %d is WORKER, waiting for redirects on port %d and is %s\n", procID, port, on_flag ? "ON" : "OFF");

      /* Post receive to change the on-state of the server */
      MPI_Irecv(&on_flag, 1, MPI_INT, BALANCER, FLAG, MPI_COMM_WORLD, &power);

      /* Start server loop */
      while (1) {

        FD_ZERO(&set);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
  
        FD_SET(listenfd, &set);
        
        /* Test if server needs to be turned on or off */
        MPI_Test(&power, &recv_flag, &power_stat);

        /* If message was received, acknowledge and post new receive */
        while (recv_flag)
        {
          recv_flag = 0;
   /*       printf("Worker %d received on/off message with value: %d\n", procID, on_flag); */
          MPI_Irecv(&on_flag, 1, MPI_INT, BALANCER, FLAG, MPI_COMM_WORLD, &power);
          MPI_Test(&power, &recv_flag, &power_stat);
        }

        /* Get request and work on it */
        clientlen = sizeof(clientaddr);
        
        do {
          new_con = select(listenfd+1, &set, NULL, NULL, &timeout);
  
          if (FD_ISSET(listenfd, &set))
          {
    	     connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    	     doit(connfd, procID);
    	     Close(connfd);
           MPI_Isend(&procID, 1, MPI_INT, BALANCER, REQUEST_DONE, MPI_COMM_WORLD, MPI_REQUEST_NULL);
          }
        } while (new_con > 0);

        /* Check if server is on or off, work accordingly */
        if (!on_flag)
        {
          /*printf("Server %d sleeping\n", procID);*/
          sleep(SLEEP_INTERVAL);
        }
      }
    }

    free(processes);

    /* Never called, but in the bizarre case this is needed, it should be here */
    MPI_Finalize();
    return 0;
}
/* $end main */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd, int procID) 
{
    int is_static;
    int met, len = 0;
    int err;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE], filetype[MAXLINE];
    char resp_buf[MAXBUF];
    rio_t rio;
    cacheobj *obj;
  
    /* Read request line and headers */
    rio_readinitb(&rio, fd);
    if( (err = rio_readlineb(&rio, buf, MAXLINE)) == -1)
      return;
    sscanf(buf, "%s %s %s", method, uri, version);

    met = find_method(method);
    switch(met) {
      case HEAD: len = read_requesthdrs(&rio); break;
      case GET: len = read_requesthdrs(&rio); break;
      case POST: len = read_requesthdrs(&rio); break;
      case TRACE: respond_trace(fd, buf); return;
      case OPTIONS: respond_options(fd); return;
      default: clienterror(fd, method, "501", "Not Implemented",
                   "Awesome does not implement this method");
    }

/*    if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) { 
       clienterror(fd, method, "501", "Not Implemented",
                "Awesome does not implement this method");
       return;
    }
    read_requesthdrs(&rio); */

    /*if ( (obj = in_cache(uri)) )
    {
      get_filetype(filename, filetype);
      sprintf(resp_buf, "HTTP/1.0 200 OK\r\n");
      sprintf(resp_buf, "%sServer: Awesome Web Server\r\n", resp_buf);
      sprintf(resp_buf, "%sContent-length: %d\r\n", resp_buf, obj->size);
      sprintf(resp_buf, "%sContent-type: %s\r\n\r\n", resp_buf, filetype);    
      rio_writen(fd, resp_buf, strlen(resp_buf));
      rio_writen(fd, obj->obj, obj->size);
      return;
    }*/

    /* Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs);
    if (stat(filename, &sbuf) < 0) {
	    clienterror(fd, filename, "404", "Not found",
		    "Awesome couldn't find this file");
	    return;
    }


    if ( strstr(filename, "cgi-bin/a") || strstr(filename, "cgi-bin/b") || strstr(filename, "cgi-bin/c") )
    {
      do_compute(filename, fd, cgiargs, procID);
      return;
    }

    if (is_static) { /* Serve static content */
	    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
	        clienterror(fd, filename, "403", "Forbidden",
			    "Awesome couldn't read the file");
	        return;
	    } 
      serve_static(fd, filename, sbuf.st_size, met, &rio, len, procID);
    }
    else { /* Serve dynamic content */
	    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
	      clienterror(fd, filename, "403", "Forbidden",
			  "Awesome couldn't run the CGI program");
	      return;
	    }
	    serve_dynamic(fd, filename, cgiargs, met, &rio, len);
    }
}
/* $end doit */


/*
 * send_redirect - The dispatcher redirects the client to a worker server
 * */
int send_redirect(int fd, int host, rio_t *rio, int port)
{
  char buf[MAXBUF], uri[MAXLINE], filename[MAXLINE], cgiargs[MAXLINE];
  char method[MAXLINE], version[MAXLINE];
  char retbuf[MAXBUF] = { 0 };
  int err;

  /* Read request line and headers */
  if( (err = rio_readlineb(rio, buf, MAXLINE)) == -1 )
    return -1;

  sscanf(buf, "%s %s %s", method, uri, version);
  parse_uri(uri, filename, cgiargs);

  sprintf(retbuf, "HTTP/1.1 307 Temporary Redirect\r\n");
  sprintf(retbuf, "%sLocation: http://%s:%d/%s", retbuf, hosts[host], host+port, filename+2);
  if (strlen(cgiargs) != 0)
    sprintf(retbuf, "%s?%s", retbuf, cgiargs);
  sprintf(retbuf, "%s\r\n", retbuf);

  rio_writen(fd, retbuf, strlen(retbuf));
  return 0;
}


/* 
 * find_method - Determine the HTTP method in the request
 */
/* $begin find_method */
int find_method(char *method)
{
  int i;
  for (i = 0; i < METHODS_LENGTH; i++)
  {
    if (!strcasecmp(method, METHODS[i])) { 
      return i;
    }
  }
  return -1;
}

/*
 * read_requesthdrs - read and parse HTTP request headers
 */
/* $begin read_requesthdrs */
int read_requesthdrs(rio_t *rp) 
{
    char buf[MAXLINE];
    char *p;
    int len = 0;
    int rio_ret;

/*    printf("Reading request headers\n"); */
    rio_ret = rio_readlineb(rp, buf, MAXLINE);
  /*  printf("%s", buf); */
    while(rio_ret > 0 && strcmp(buf, "\r\n")) {
/*	    printf("%s", buf); */
      if ( (p = strstr(buf, "Content-Length: ")) != NULL)
      {
        p += strlen("Content-length: ");
        len = atoi(p);
      }
    rio_ret =  rio_readlineb(rp, buf, MAXLINE);
    }
    return len;
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {  /* Static content */
	    strcpy(cgiargs, "");
    	strcpy(filename, ".");
    	strcat(filename, uri);
    	if (uri[strlen(uri)-1] == '/')
	      strcat(filename, "home.html");
	    return 1;
    }
    else {  /* Dynamic content */
	    ptr = index(uri, '?');
	    if (ptr) {
	      strcpy(cgiargs, ptr+1);
	      *ptr = '\0';
	    }
	    else 
	      strcpy(cgiargs, "");
  	  strcpy(filename, ".");
  	  strcat(filename, uri);
  	  return 0;
    }
}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client 
 */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize, int met, rio_t* rp, int len, int procID) 
{
    int srcfd;
    char *srcp, filetype[MAXLINE];
    char buf[MAXBUF] = { 0 };
    char body[len];
    char args[MAXLINE];

    /* Read the message body if this is a POST request */
    if (met == POST)
    {
      rio_readnb(rp, body, len);
    }

    /* Parse the message body to turn it into arguments */
    parse_body(body, args, len);

    /* Send response headers to client */
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Awesome Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    rio_writen(fd, buf, strlen(buf));

    if (met != HEAD) {
      /* Send response body to client */
      srcfd = Open(filename, O_RDONLY, 0);
      srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
      Close(srcfd);
      rio_writen(fd, srcp, filesize);
      /*cache_object(srcp, filesize, filename);*/
      Munmap(srcp, filesize);
    }
}

void parse_body(char *body, char *args, int len)
{
  int i, j = 0;


  args[j++] = '-';
  args[j++] = '-';

  for(i = 0; i <= len; i++, j++)
  {
    switch(body[i])
    {
      /* Define vertical tab (\x0b) to be illegal character */
      case '+': args[j] = ' '; break;
      case '=': args[j] = '=';
                args[++j] = '\"';
                break;
      case '&':
                args[j] = '\"';
                args[++j] = '\x0b';
                args[++j] = '-';
                args[++j] = '-';
                break;
      case '%': parse_percent(body, args, &i, &j); break;
      default:  args[j] = body[i];
    }
  }

  args[j-1] = '\"';
  args[j] = '\0';
}

void parse_percent(char *body, char *args, int *i, int *j)
{
  char code[2];
  int d;
  char new;
  code[0] = body[++(*i)];
  code[1] = body[++(*i)];

  sscanf(code, "%x", &d);
  new = (char)d;
  args[*j] = new;
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype) 
{
    if (strstr(filename, ".html"))
	    strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
    	strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
    	strcpy(filetype, "image/jpeg");
    else
    	strcpy(filetype, "text/plain");
}  
/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs, int met, rio_t *rp, int len) 
{
    char buf[MAXLINE];
    char body[len+1];
    char args[MAXLINE];
    long parsed[MAXLINE / 2];
    char *tok;
    int i = 0;

    /* Read the message body if this is a POST request */
    if (met == POST)
    {
/*      printf("Reading %d bytes of data\n", len); */
      rio_readnb(rp, body, len);
      body[len] = '\0';
/*      printf("Read: %s\n", body); */
      parse_body(body, args, len);
/*      printf("Done parsing body\n"); */
    }
    else
    {
      printf("Not post?\n");
      parse_body(body, args, len);
    }

    tok = strtok(args, "\x0b");
    do {
      parsed[i++] = (long)tok;
    } while( (tok = strtok(NULL, "\x0b")) );
    parsed[i] = (long)NULL;


    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Awesome Web Server\r\n", buf);
    rio_writen(fd, buf, strlen(buf));
  
    if (Fork() == 0) { /* child */
    	/* Real server would set all CGI vars here */
    	setenv("QUERY_STRING", cgiargs, 1); 
      fflush(stdout);
    	Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */
    	Execve(filename, (char **)(parsed), environ); /* Run CGI program */
    }
    Wait(NULL); /* Parent waits for and reaps child */
}
/* $end serve_dynamic */

void do_compute(char *s, int fd, char *cgiargs, int procID)
{
  char filestr[2*MAX_STRLEN] = { 0 };
  char buf[MAXBUF];
  char filename[MAXLINE];
  char *resource;  
  int fileno;
  FILE *file;

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Awesome Web Server\r\n", buf);

  resource = strdup(s);

  if (strlen(cgiargs) == 0)
    fileno = 1;
  else
    fileno = atoi(cgiargs);

  strcpy(filename, "strings/s");
  if (fileno == 1) strcat(filename, "1.txt"); 
  else if (fileno == 2) strcat(filename, "2.txt"); 
  else if (fileno == 3) strcat(filename, "3.txt"); 
  else
  {
    cgiargs[0] = '1';
    cgiargs[1] = '\0';
    strcat(filename, "1.txt");
  }
  
  file = fopen(filename, "rt");
  fread(filestr, 1, MAX_STRLEN, file);
  fclose(file);

  if( strstr(s, "cgi-bin/a") )
    reverse(filestr);
  else if ( strstr(s, "cgi-bin/b") )
    sort(filestr);
  else if ( strstr(s, "cgi-bin/c") )
    right(filestr);

  sprintf(buf, "%sContent-length %zd\r\n", buf, strlen(filestr));
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, "text/plain");

  rio_writen(fd, buf, strlen(buf));
  rio_writen(fd, filestr, strlen(filestr));
  
  strcat(s, "?");
  strcat(s, cgiargs);
  
/*  if (!in_cache(s+1))
  {
    cache_object(filestr, strlen(filestr), s+1);
  } */

}


/*
 * respond_trace - Respond to TRACE requests
 */
/* $begin respond_trace */
void respond_trace(int fd, char *request)
{
  char buf[MAXLINE];

  sprintf(buf, request);
  rio_writen(fd, buf, strlen(buf));
}

/*
 * respond_options - Respond to OPTIONS requests
 */
/* $begin respond_options */
void respond_options(int fd)
{
  char buf[MAXLINE];

  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Awesome Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sAllow: GET, HEAD, POST, TRACE, OPTIONS\r\n", buf);
  sprintf(buf, "%sContent-length: 0\r\n", buf);
  rio_writen(fd, buf, strlen(buf));
}
/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Awesome Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Awesome Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    rio_writen(fd, buf, strlen(buf));
    rio_writen(fd, body, strlen(body));
}
/* $end clienterror */

void print_array(int *arr, int len)
{
  int i;

  printf("array: [");
  for(i = 0; i < len; i++)
    printf("%d, ", arr[i]);
  printf("]\n");

}
