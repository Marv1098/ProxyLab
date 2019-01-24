/*
*   Name: Marvin Tailor
*   PSU ID: mvt5393
*/
#include <string.h>
#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 16777216 
#define MAX_OBJECT_SIZE 8388608 

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static const char *AcceptHdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";

void doit(int fd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *hostname, char *pathname, int *port);

int main(int argc, char **argv)
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientLen;
    struct sockaddr_in clientaddr;

    // CHECKS IF TWO ARGUMENTS ARE GIVEN AND IF NOT THEN OUTPUT ERROR MESSAGE.
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    Signal(SIGPIPE, SIG_IGN);
    listenfd = Open_listenfd(argv[1]);  /* LISTENS TO THE GIVEN PORT NUMBER. */
    while(1) {
        clientLen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientLen);  /* ACCEPTS THE REQUEST FROM THE CLIENT. */
            Getnameinfo((SA *) &clientaddr, clientLen, hostname, MAXLINE, port, MAXLINE, 0);
            printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(connfd);
        Close(connfd);
    }
    printf("%s", user_agent_hdr);
    return 0;
}

/*
*   doit handles one proxy request/response transaction.
*/
void doit(int fd)
{
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], pathname[MAXLINE]; 
    char ReqBuf[MAXBUF];
    int port, Client_fd, Response;
    char clientPort[MAXLINE];
    rio_t client_rio, server_rio;

    /* Read request line and headers */
    Rio_readinitb(&client_rio, fd);
    if(!Rio_readlineb(&client_rio, buf, MAXLINE)) {
        return;
    }
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    if(strcasecmp(method, "GET")) {
        clienterror(fd, hostname, "501", "Not Implemented", "Proxy does not implement this method.");
        return;
    }
    read_requesthdrs(&client_rio);

    if((parse_uri(uri, hostname, pathname, &port) < 0)) {
        clienterror(fd, uri, "404", "Not Found", "Proxy couldn't find the file");
        return;
    }

    /* Forwarding the request to the server */
    sprintf(clientPort, "%d", port);
    Client_fd = Open_clientfd(hostname, clientPort);
    Rio_readinitb(&server_rio, Client_fd);

    sprintf(ReqBuf, "GET %s HTTP/1.0\r\n", pathname);
    Rio_writen(Client_fd, ReqBuf, strlen(ReqBuf));
    sprintf(ReqBuf, "Host: %s\r\n", hostname);
    Rio_writen(Client_fd, ReqBuf, strlen(ReqBuf));
    sprintf(ReqBuf, "%s\r\n", user_agent_hdr);
    Rio_writen(Client_fd, ReqBuf, strlen(ReqBuf));
    sprintf(ReqBuf, "%s\r\n", AcceptHdr);
    Rio_writen(Client_fd, ReqBuf, strlen(ReqBuf));
    sprintf(ReqBuf, "Connection: close\r\n");
    Rio_writen(Client_fd, ReqBuf, strlen(ReqBuf));
    sprintf(ReqBuf, "Proxy-Connection: close\r\n\r\n");
    Rio_writen(Client_fd, ReqBuf, strlen(ReqBuf));

    /* Forwarding the response back to the browser */
    while((Response = Rio_readlineb(&server_rio, ReqBuf, sizeof(ReqBuf))) != 0) {
        Rio_writen(fd, ReqBuf, Response);
    }
    Close(Client_fd);

}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXLINE];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Proxy Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Proxy server</em>\r\n", body);

    /* Prints the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

/*
*   read_requesthdrs - read HTTP request headers
*/
void read_requesthdrs(rio_t *rp) {
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while(strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}

int parse_uri(char *uri, char *hostname, char *pathname, int *port) {
    
    char *HostEnd;
    char *Path;
    int HostLen;
    *port = 80;

    char *temp = strstr(uri, "http://");
    if(temp == NULL) {
        return 0;
    }
    /* Extracting the Hostname, Port, Pathname */
    else {
        HostEnd = strpbrk(uri + 7, " :/\r\n\0");    // Find the instance in Host and return a pointer to that instance.
        HostLen = HostEnd - (uri + 7);              // Then we get the length from the beginning of the hostname till the end of the hostname.
        strncpy(hostname, uri + 7, HostLen);        // We copy the the host into hostname.

        /* Extracting the Port */
        char *Port = strchr(uri + 7, ':');
        if(Port != NULL){
            *port = atoi(Port + 1);
        }

        /* Extracting the Pathname */
        Path = strchr(uri + 7, '/');                // We look for the instance "/" in the uri and that tells us that where the path begins from.
        if(Path != NULL){                           // Once that instance is found go to the next index and then copy the path into the pathname.
            strcpy(pathname, Path);
        }
    }
    return 0;
}
