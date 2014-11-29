// http://www.cs.rpi.edu/~moorthy/Courses/os98/Pgms/socket.html

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/ioctl.h>

void error(char *msg)
{
    perror(msg);
    exit(0);
}

char* nsd_send(int sockfd, char *cmd) {
  char buffer[512];

  bzero(buffer, 512);
  strcpy(buffer, cmd);
  printf("Sending: [%s]", buffer);

  int n;
  n = write(sockfd, buffer,strlen(buffer));
  if (n < 0) 
    error("ERROR writing to socket");

  bzero(buffer, 512);

  n = read(sockfd,buffer,511);
  if (n < 0) 
    error("ERROR reading from socket");

  //printf("resp1: %s\n", buffer);

  char *response;
  strcpy(response, buffer);

  return response;
}

char* nsd_read(int sockfd) {
  char buffer[512];

  printf("sta");

  int len = 0;
  int n;
  ioctl(sockfd, FIONREAD, &len);
  printf("Len: %i", len);

  if (len > 0) {
    n = read(sockfd, buffer, len);
  } else {
    return "";
  }

  char *response;
  strcpy(response, buffer);

  return response;
}

int main(int argc, char *argv[])
{
  int sockfd, portno, n;

  struct sockaddr_in serv_addr;
  struct hostent *server;

  if (argc < 3) {
    fprintf(stderr,"usage %s hostname port\n", argv[0]);
    exit(0);
  }
  portno = atoi(argv[2]);
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");
  server = gethostbyname(argv[1]);
  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host\n");
    exit(0);
  }
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
  serv_addr.sin_port = htons(portno);
  if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) 
  error("ERROR connecting");

  char *response;
  //response = nsd_send(sockfd, "hello\n");
  //printf("hello: %s\n", response);

  response = nsd_send(sockfd, "display\n");
  printf("display1: %s\n", response);

  response = nsd_send(sockfd, "status\n");
  printf("status: %s\n", response);

  response = nsd_send(sockfd, "watch 0\n");
  printf("watch: %s\n", response);

  response = nsd_read(sockfd);
  printf("read1: %s\n", response);


  return 0;
}