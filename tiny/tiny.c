/* $begin Awesomemain */
/*
 * Awesome.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *     GET method to serve static and dynamic content.
 */
#include "csapp.h"
#include "hosts.h"
#include <mpi.h>

/* Define the MPI root process to be 0 and the balancer to be 1 */
#define ROOT 0
#define BALANCER 1

/* Define the length of a host address
 *
 * ghcXX.ghc.andrew.cmu.edu - 24 characters plus one null terminator */
#define HOST_LENGTH 25

/* Define number of initial servers (TODO: Change to command-line arg */
#define INIT_SERVERS 5

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


void doit(int fd);
int  read_requesthdrs(rio_t *rp);
int  parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, int met, rio_t* rp, int len);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, int met, rio_t* rp, int len);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);
int  find_method(char *method);
void respond_trace();
void respond_options();
void parse_body(char *body, char *args, int len);
void parse_percent(char *body, char *args, int *i, int *j);
void send_redirect(int fd, int host, rio_t *rio, int port);

int main(int argc, char **argv) 
{
    int listenfd, connfd, port, clientlen;
    int on_flag = 0;
    int recv_flag;
    int procID;
    int i;
    struct sockaddr_in clientaddr;
    int redirect = BALANCER+1;
    int *num_requests;
    int comm_size;
    int redirect_flag, request_flag, done_flag;
    int request_handler;
    int request_done;
    MPI_Status stat;
    MPI_Request req, update;
    MPI_Datatype HOST_TYPE;

    printf("Program starting\n");
    fflush(stdout);

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



    /* Initialize the first servers */
    if (procID > BALANCER)
    {
      if (procID <= BALANCER + INIT_SERVERS)
        on_flag = 0;
      else
        on_flag = -1;
    }
    else
      on_flag = -2;

    /* Wait until servers are initialized to begin working */
    if (procID == BALANCER)
      num_requests = (int *)malloc(sizeof(int) * comm_size);

    MPI_Barrier(MPI_COMM_WORLD);
    printf("Initial servers initialized\n");


    /* Initialize the load balancer's array of number of requests per worker */
    MPI_Gather(&on_flag, 1, MPI_INT, num_requests, 1, MPI_INT, BALANCER, MPI_COMM_WORLD);
    printf("Balancer array initialized\n");


    /* Root dispatcher loop */
    if (procID == ROOT)
    {
      rio_t rio;

      listenfd = Open_listenfd(port);
      printf("procID %d is ROOT, waiting for connections\n", procID);

      /* Post receive to change the client to redirect to */
      MPI_Irecv(&redirect, 1, MPI_INT, BALANCER, CHANGE_REDIRECT, MPI_COMM_WORLD, &req);

      while (1) {
        /* Test for reception of redirection message */
        MPI_Test(&req, &redirect_flag, &stat);

        /* If message was received, acknowledge and post new receive */
        while (redirect_flag) 
        {
          MPI_Irecv(&redirect, 1, MPI_INT, BALANCER, CHANGE_REDIRECT, MPI_COMM_WORLD, &req);
          MPI_Test(&req, &redirect_flag, &stat);
        }

        clientlen=sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Rio_readinitb(&rio, connfd);
        send_redirect(connfd, redirect, &rio, port);

        /* Tell load balancer to increment the number of requests being handled */
        MPI_Isend(&redirect, 1, MPI_INT, BALANCER, NEW_REQUEST, MPI_COMM_WORLD, &update);
        Close(connfd);
      }
    }

    if (procID == BALANCER)
    {
      int min_rank = BALANCER+1;
      int reqs;

      printf("procID %d is BALANCER\n", procID);
      
      MPI_Irecv(&request_handler, 1, MPI_INT, ROOT, NEW_REQUEST, MPI_COMM_WORLD, &req);
      MPI_Irecv(&request_done, 1, MPI_INT, MPI_ANY_SOURCE, REQUEST_DONE, MPI_COMM_WORLD, &update);
     
      while (1) {
        /* Test for reception of new request message */
        MPI_Test(&req, &request_flag, &stat);

        /* If message was received, acknowledge, update, and post new receive */
        while (request_flag)
        {
          num_requests[request_handler]++;
          MPI_Irecv(&request_handler, 1, MPI_INT, ROOT, NEW_REQUEST, MPI_COMM_WORLD, &req);  
          MPI_Test(&req, &request_flag, &stat);
        }

        /* Test for reception of a message indicating a request is fulfilled */
        MPI_Test(&update, &done_flag, &stat);

        /* If message was received, acknowledge, update, and post new receive */
        while (done_flag)
        {
          num_requests[request_done]--;
          MPI_Irecv(&request_done, 1, MPI_INT, MPI_ANY_SOURCE, REQUEST_DONE, MPI_COMM_WORLD, &update);
          MPI_Test(&update, &done_flag, &stat);
        }

        min_rank = BALANCER + 1;

        /* Find the server with the fewest pending requests and select it */
        for (i = BALANCER + 1; i < comm_size; i++)
        {
          reqs = num_requests[i];
          if (reqs > 0 && reqs < num_requests[min_rank])
            min_rank = i;
        }

        /* If different from before, tell the root */
        if (min_rank != redirect)
        {
          redirect = min_rank;
          MPI_Isend(&redirect, 1, MPI_INT, ROOT, CHANGE_REDIRECT, MPI_COMM_WORLD, &update);
        }

      }

     free(num_requests);
    }

    /* Worker servers loop */
    if (procID > BALANCER)
    {
      on_flag++;
      listenfd = Open_listenfd(port);

      printf("procID %d is WORKER, waiting for redirects\n", procID);

      /* Post receive to change the on-state of the server */
      MPI_Irecv(&on_flag, 1, MPI_INT, ROOT, FLAG, MPI_COMM_WORLD, &req);

      /* Start server loop */
      while (1) {

        /* Test if server needs to be turned on or off */
        MPI_Test(&req, &recv_flag, &stat);

        /* If message was received, acknowledge and post new receive */
        if (recv_flag)
        {
          recv_flag = 0;
          MPI_Irecv(&on_flag, 1, MPI_INT, ROOT, FLAG, MPI_COMM_WORLD, &req);
        }

        /* Check if server is on or off, work accordingly */
        if (!on_flag)
        {
          sleep(5);
          continue;
        }

        /* Get request and work on it */
	      clientlen = sizeof(clientaddr);
    	  connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    	  doit(connfd);
    	  Close(connfd);
        MPI_Isend(&procID, 1, MPI_INT, BALANCER, REQUEST_DONE, MPI_COMM_WORLD, &update);
      }
    }

    /* Never called, but in the bizarre case this is needed, it should be here */
    MPI_Finalize();
    return 0;
}
/* $end main */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd) 
{
    int is_static;
    int met, len = 0;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;
  
    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
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

    /* Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs);
    if (stat(filename, &sbuf) < 0) {
	    clienterror(fd, filename, "404", "Not found",
		    "Awesome couldn't find this file");
	    return;
    }

    if (is_static) { /* Serve static content */
	    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
	        clienterror(fd, filename, "403", "Forbidden",
			    "Awesome couldn't read the file");
	        return;
	    } 
	    serve_static(fd, filename, sbuf.st_size, met, &rio, len);
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
void send_redirect(int fd, int host, rio_t *rio, int port)
{
  char buf[MAXBUF], uri[MAXLINE], filename[MAXLINE], cgiargs[MAXLINE];
  char method[MAXLINE], version[MAXLINE];
  char retbuf[MAXBUF];

  /* Read request line and headers */
  Rio_readlineb(rio, buf, MAXLINE);
  sscanf(buf, "%s %s %s", method, uri, version);
  parse_uri(uri, filename, cgiargs);

  sprintf(retbuf, "HTTP/1.0 302 Found\r\n");
  sprintf(retbuf, "%sLocation: http://%s:%d/%s\r\n", retbuf, hosts[host], host+port, filename+2);
  Rio_writen(fd, retbuf, strlen(retbuf));
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
    rio_ret = Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf); 
    while(rio_ret > 0 && strcmp(buf, "\r\n")) {
	    printf("%s", buf);
      if ( (p = strstr(buf, "Content-Length: ")) != NULL)
      {
        p += strlen("Content-length: ");
        len = atoi(p);
      }
    rio_ret =  Rio_readlineb(rp, buf, MAXLINE);
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
void serve_static(int fd, char *filename, int filesize, int met, rio_t* rp, int len) 
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];
    char body[len];
    char args[MAXLINE];

    /* Read the message body if this is a POST request */
    if (met == POST)
    {
      Rio_readnb(rp, body, len);
    }

    /* Parse the message body to turn it into arguments */
    parse_body(body, args, len);

    /* Send response headers to client */
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Awesome Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));

    if (met != HEAD) {
      /* Send response body to client */
      srcfd = Open(filename, O_RDONLY, 0);
      srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
      Close(srcfd);
      Rio_writen(fd, srcp, filesize);
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
      /*printf("Reading %d bytes of data\n", len); */
      Rio_readnb(rp, body, len);
      body[len] = '\0';
/*      printf("Read: %s\n", body); */
      parse_body(body, args, len);
/*      printf("Done parsing body\n"); */
    }
    else
    {
      parse_body(body, args, len);
    }

    tok = strtok(args, "\x0b");
    do {
      parsed[i++] = (long)tok;
    } while( (tok = strtok(NULL, "\x0b")) );
    parsed[i] = (long)NULL;


    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Awesome Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
  
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

/*
 * respond_trace - Respond to TRACE requests
 */
/* $begin respond_trace */
void respond_trace(int fd, char *request)
{
  char buf[MAXLINE];

  sprintf(buf, request);
  Rio_writen(fd, buf, strlen(buf));
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
  Rio_writen(fd, buf, strlen(buf));
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
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */
