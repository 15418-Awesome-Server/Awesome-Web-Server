void doit(int fd);
void read_requestedrs(rio_t *rp);
int pare_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filenam, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

struct in_addr{
	unsigned int 	s_addr; 		//Network byte order (Big-endian)
};

struct sockaddr{	//generic
	unsigned short 	sa_family;		//Protocol family
	char			sa_data[14];	//Addr data
};

struct sockaddr_in{	//internet
	unsigned short 	sin_family; 	//Address Family
	unsigned short 	sin_port;		//Port number in network byte order
	struct in_addr 	sin_addr;		//IP addr in network byte order
	unsigned char 	sin_zero[8];	//pad to sizeof(struct socckaddr)
};

typedef struct sockaddr SA; //For casting purposes

int main(){
	int listenfd, connfd, port, clentlen;
	struct sockaddr_in clientaddr;

	/* Check command line args */
	if (argc!=2){
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	port = atoi(argv[1]);

	/* TODO customize open_listenfd? */
	listenfd = open_listenfd(port);

	while(1){
		clientlen = sizeof(clientaddr);

		/* TODO customize accept */
		connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		doit(connfd);

		/* TODO customize close */
		Close(connfd);
	}

	exit(0);
}

int open_listenfd(int port){
	int listenfd, optval = 1;
	struct sockaddr_in serveraddr;

	/* create socket descriptor */
	if((listenfd = socket(AF_INET, SOCKSTREAM, 0)) < 0)
		return -1;

	/* Eliminate "Address already in use" error from bind */
	if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)))
		return -1;

	/* listenfd will be an endpoint for all requests to port on any IP address for this host */
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)port);
	if(bind(listenfd, (SA*)&serveraddr, sizeof(serveraddr)) < 0)
		return -1;

	/* Make it a listening socket ready to accept connection requests */
	if(listen(listenfd, LISTENQ) < 0)
		return -1;

	return listenfd;
}

void doit(int fd){
	int is_static;
	struct stat sbuf;
	char buff[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char filename[MAXLINE], cgiargs[MAXLINE];
	/* TODO customize rio_t? */
	rio_t rio;

	/* Read request line and headers */
	/* TODO customize Rio_read... */
	Rio_readinitb(&rio, fd);
	Rio_readlineb(&rio, buf, MAXLINE);
	sscanf(buf, "%s %s %s", method, uri, version);
	if(strcasecomp(method, "GET")){
		clienterror(fd, filename, "404", "Not Found", "Awesome couldn't find this file");
		return;
	}

	if(is_static){ 	// serve static content
		if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)){
			clienterror(fd, filename, "403", "Forbidden", "Awesome couldn't read the file");
			return;
		}
		serve_static(fd, filename, sbuf.st_size);
	}else{ 			// serve dynamic content
		if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)){
			clienterror(fd, filename, "403", "Forbidden", "Awesome couldn't run the CGI program");
			return;
		}
		serve_dynamic(fd, filename, cgiargs);
	}
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg){
	char buf[MAXLINE], body[MAXLINE];

	/* build HTTP response body (html) */
	sprintf(body, "<html><title>Awesome Error</title>");
	sprintf(body, "%s<body bgcolor=""ffffff"">\f\n", body);
	sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
	sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<hr><em>The Awesome Web server</em>\r\n", body);

	/* Print the HTTP response */
	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	/* TODO customize Rio_writen */
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-type: text/html\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
	Rio_writen(fd, buf, strlen(buf));
	Rio_writen(fd, body, strlen(body));
}
