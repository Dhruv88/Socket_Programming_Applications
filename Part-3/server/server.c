#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <string.h>
#include <netdb.h>

#define MAXLINE 1024  
#define PORT "3495"

// void delay(int seconds)
// {
//     int milliSeconds = 100000 * seconds;
  
//     clock_t startTime = clock();
  
//     while (clock() < startTime + milliSeconds)
//         ;
// }

int parseNum(char *message, char end)
{
  int i=0;
  int id = 0;
  while(message[i] != end)
  {
    id = id*10+(message[i]-'0');
    i++;
  }
  return id;
}

void *cast_ipv(struct sockaddr* sa)
{
  if(sa->sa_family == AF_INET)
  {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char* argv[])
{
  char* serverAddrString = NULL;
  int delay = 2;
  for(int i=1;i<argc;i++)
  {
    if(argv[i][0] == '-' && argv[i][1] == 'd')
    {
      i++;
      delay = parseNum(argv[i],'\0');
    }
    else 
    {
      serverAddrString = (char*)malloc(sizeof(*argv[i]));
      strcpy(serverAddrString, argv[i]);
    }
  }
  int socketDescriptor;
  int number;
  int addressLength;
  char message[MAXLINE];

  struct sockaddr_storage  clientAddress;
  //Get IP from name
  int ev;
  struct addrinfo hints, *servinfo, *it;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;

  if((ev = getaddrinfo(serverAddrString, PORT, &hints, &servinfo))!=0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ev));
    return 1;
  }

  for(it = servinfo; it != NULL; it = it->ai_next)
  {
    if((socketDescriptor = socket(it->ai_family, it->ai_socktype, it->ai_protocol))!=-1)
    {
      if(bind(socketDescriptor,it->ai_addr, it->ai_addrlen)==-1)
      {
        close(socketDescriptor);
        continue;
      }
      else
        break;
    }
  }

  if(it == NULL)
  {
    perror("Failed to acquire a socket!\n");
    return 2;
  }
  char sa[INET6_ADDRSTRLEN];
  inet_ntop(it->ai_family,cast_ipv(it->ai_addr),sa, INET6_ADDRSTRLEN);
  printf("\nServer Started ...%s\n",sa);

  freeaddrinfo(servinfo);

  int c=0;

  while(1){
    // printf("\n");
    addressLength = sizeof(clientAddress);

    number = recvfrom(socketDescriptor,message,MAXLINE,0,(struct sockaddr*)&clientAddress,&addressLength);
    char ip[INET6_ADDRSTRLEN];
    inet_ntop(clientAddress.ss_family,cast_ipv((struct sockaddr*)&clientAddress),ip, INET6_ADDRSTRLEN);
    printf("\n Message from client %s:%d\n ", ip, ntohs(((struct sockaddr_in6*)&clientAddress)->sin6_port)); 
    // printf("\n Client's Message: %s ",message);

    if(number<1)
      perror("send error");

    int n = rand();
    if(c>0 && n%c==0)
    {
      sleep(delay);
    }
    printf("n=%d c=%d\n",n, c);
      // printf("%s\n", message);
      sendto(socketDescriptor,message,number,0,(struct sockaddr*)&clientAddress,addressLength);
      if(c>20)
        c=0;
      else
        c++;
  }
  close(socketDescriptor);
  return 0;
}
