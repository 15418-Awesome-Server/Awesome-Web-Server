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

	/* TODO customize open_listenfd */
	listenfd = Open_listenfd(port);

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
