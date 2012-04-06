int main(){
	int listenfd, connfd, port, clentlen;
	struct sockaddr_in clientaddr;

	/* Check command line args */
	if (argc!=2){
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	port = atoi(argv[1]);

	/* customize open_listenfd */
	listenfd = Open_listenfd(port);

	while(1){
		clientlen = sizeof(clientaddr);

		/* customize accept */
		connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
		doit(connfd);

		/* customize close */
		Close(connfd);
	}

	exit(0);
}
