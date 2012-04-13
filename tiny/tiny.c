/* $begin Awesomemain */
/*
 * Awesome.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *     GET method to serve static and dynamic content.
 */
#include "csapp.h"
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
int read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, int met, rio_t* rp, int len);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, int met, rio_t* rp, int len);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);
int find_method(char *method);
void respond_trace();
void respond_options();
void parse_body(char *body, char *args, int len);
void parse_percent(char *body, char *args, int *i, int *j);

int main(int argc, char **argv) 
{
    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;


    /* Check command line args */
    if (argc != 2) {
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    	exit(1);
    }
    port = atoi(argv[1]);

    listenfd = Open_listenfd(port);
    while (1) {
	    clientlen = sizeof(clientaddr);
    	connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    	doit(connfd);
    	Close(connfd);
    }
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

    printf("Reading request headers\n");
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while(strcmp(buf, "\r\n")) {
	    printf("%s", buf);
      if ( (p = strstr(buf, "Content-Length: ")) != NULL)
      {
        p += strlen("Content-length: ");
        len = atoi(p);
        printf("len = %d\n", len);
      }
	    Rio_readlineb(rp, buf, MAXLINE);
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
      case '+': args[j] = ' '; break;
      case '=': args[j] = '=';
                args[++j] = '\"';
                break;
      case '&':
                args[j] = '\"';
                args[++j] = ' ';
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
    char *(args[MAXLINE]);

    /* Read the message body if this is a POST request */
    if (met == POST)
    {
      printf("Reading %d bytes of data\n", len);
      Rio_readnb(rp, body, len);
      body[len] = '\0';
      printf("Read: %s\n", body);
      parse_body(body, args, len);
      printf("Done parsing body\n");
    }
    else
    {
      parse_body(body, args, len);
    }

    /* TODO: The args array does not play nicely with execve right
     * now. Need to parse each individual arg as a token and add
     * the NULL ender to finish this up. */


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
    	Execve(filename, args, environ); /* Run CGI program */
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
