#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>

#define MAXLINE 1024
#define PORT "3495"
pthread_mutex_t lock;

typedef struct echosendkit{
  int interval;
  int packetSize;
  int numberOfMessages;
  struct sockaddr *to;
  int tolen;
  int socfd;
  clock_t *sendTimes;
} echoSendKit;

typedef struct echorcvkit{
  int interval;
  int packetSize;
  int numberOfMessages;
  struct sockaddr *from;
  int fromlen;
  int socfd;
  clock_t *sendTimes;
  clock_t *rcvTimes;
  int timeOut;
} echoRcvKit;



// void delay(int seconds)
// {
//     int milliSeconds = 1000000 * seconds;
  
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

void *sendEchoMessages(void *arg)
{
  echoSendKit *sendKit = (echoSendKit*)arg;
  for(int i=0;i<sendKit->numberOfMessages;i++)
  {
    //create message
    char* message = (char*)malloc(sizeof(char)*sendKit->packetSize);
    int len = sprintf(message, "%d", i);
    int paddingLen = sendKit->packetSize-len;
    char *padding = (char*)malloc(sizeof(char)*paddingLen);
    memset(padding, '$', paddingLen-1);
    strcat(message, padding);
   
    //send message
    pthread_mutex_lock(&lock);
    sendKit->sendTimes[i] = clock();
    int temp = sendto(sendKit->socfd,message,sendKit->packetSize,0,sendKit->to,sendKit->tolen);
    pthread_mutex_unlock(&lock);
    // printf("send: %d\n", temp);
    //delay
    sleep(sendKit->interval);
    if(message!=NULL)
      free(message);
    if(padding!=NULL)
      free(padding);
  }
}

void *receiveEchoMessages(void *arg)
{
  echoRcvKit *rcvKit = (echoRcvKit*)arg;
  int expecting = rcvKit->numberOfMessages;
  while(expecting>0)
  {
    //check for timeout for each message
    pthread_mutex_lock(&lock);
    for(int i=0;i<rcvKit->numberOfMessages;i++)
    {
      if(rcvKit->sendTimes[i]!=-1)
      {
        if(rcvKit->rcvTimes[i]!=-2 && (rcvKit->rcvTimes[i]==-1 || (rcvKit->rcvTimes[i]==0 && clock()>rcvKit->sendTimes[i]+(1000000*rcvKit->timeOut))))
        {
          // printf("rcv: %ld ", rcvKit->rcvTimes[i]);
          rcvKit->rcvTimes[i] = -2;
          expecting--;
          printf("Request Timed Out.\n");
        }
      }
      
    }
    // printf("end2\n");
    pthread_mutex_unlock(&lock);
    // printf("d%d\n", expecting);
    //initialize message
    char* message = (char*)malloc(sizeof(char)*rcvKit->packetSize);
    
    //receive message
    int rcvlen = recvfrom(rcvKit->socfd,message,rcvKit->packetSize,MSG_DONTWAIT,rcvKit->from,&(rcvKit->fromlen));
    if(rcvlen!=-1)
    {
      clock_t rcvTime = clock();
      int id = parseNum(message, '$');
      // printf("outside\n");
      pthread_mutex_lock(&lock);
      // printf("inside\n");
      if(rcvKit->rcvTimes[id]!=-2)
      {
        rcvKit->rcvTimes[id] = rcvTime;
        if(rcvTime>rcvKit->sendTimes[id]+(1000000*rcvKit->timeOut))
          rcvKit->rcvTimes[id]=-1;
        else
        {
          clock_t rtt = difftime(rcvKit->rcvTimes[id],rcvKit->sendTimes[id]); 
          char ip[INET6_ADDRSTRLEN];
          inet_ntop(rcvKit->from->sa_family, cast_ipv(rcvKit->from), ip,INET6_ADDRSTRLEN);
          printf("Reply from %s : bytes=%d rtt=%ld\n", ip, rcvlen, rtt);
          expecting--;
        }
      }
      // printf("end\n");
      pthread_mutex_unlock(&lock);
    }
    // printf("a%d\n", expecting);
  }
}

void initializeEchoSendKit(int interval, int packetSize, int numberOfMessages, struct sockaddr *to, int tolen, int socfd, clock_t* sendTimes, echoSendKit *sendKit)
{
  sendKit->interval = interval;
  sendKit->packetSize = packetSize;
  sendKit->numberOfMessages = numberOfMessages;
  sendKit->to = to;
  sendKit->tolen = tolen;
  sendKit->socfd = socfd;
  sendKit->sendTimes = sendTimes;
}

void initializeEchoRcvKit(int interval, int packetSize, int numberOfMessages, struct sockaddr *from, int fromlen, int socfd, clock_t* sendTimes, clock_t* rcvTimes, int timeOut, echoRcvKit *rcvKit)
{
  rcvKit->interval = interval;
  rcvKit->packetSize = packetSize;
  rcvKit->numberOfMessages = numberOfMessages;
  rcvKit->from = from;
  rcvKit->fromlen = fromlen;
  rcvKit->socfd = socfd;
  rcvKit->sendTimes = sendTimes;
  rcvKit->rcvTimes = rcvTimes;
  rcvKit->timeOut = timeOut;
}


int main(int argc, char* argv[]){
  //params
  int interval = 2; //in seconds
  int packetSize = 8; //in bytes
  int numberOfMessages = 6;
  int timeOut = 4; //in seconds
  char* serverAddrString = NULL; 
  int addressLength;
  // make a socket for the client
  int socketDescriptor;

  //Parse all the arguments
  for(int i=1;i<argc;i++)
  {
    if(argv[i][0] == '-' && argv[i][1] == 'i')
    {
      i++;
      interval = parseNum(argv[i],'\0');
    }
    else if(argv[i][0] == '-' && argv[i][1] == 's')
    {
      i++;
      packetSize = parseNum(argv[i],'\0');
    }
    else if(argv[i][0] == '-' && argv[i][1] == 'n')
    {
      i++;
      numberOfMessages = parseNum(argv[i],'\0');
    }
    else if(argv[i][0] == '-' && argv[i][1] == 't')
    {
      i++;
      timeOut = parseNum(argv[i],'\0');
    }
    else 
    {
      serverAddrString = (char*)malloc(sizeof(*argv[i]));
      strcpy(serverAddrString, argv[i]);
    }
  }

  printf("Server Address: %s\n", serverAddrString==NULL?"ip6-localhost":serverAddrString);
  printf("interval: %d s\n", interval);
  printf("packet size: %d bytes\n", packetSize);
  printf("number of messages: %d\n", numberOfMessages);
  printf("timeout: %d s\n", timeOut);

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
      break;
    }
  }

  if(it == NULL)
  {
    perror("Failed to acquire a socket!\n");
    return 2;
  }
  
  //Allocate and initialize the time arrays
  clock_t *sendTimes = (clock_t*)malloc(sizeof(clock_t)*numberOfMessages);
  clock_t *rcvTimes = (clock_t*)calloc(numberOfMessages,sizeof(clock_t));
  for(int i=0;i<numberOfMessages;i++)
  {
    sendTimes[i] = -1;
  }
  
  // set the server info
  // struct sockaddr_in serverAddress;
  // serverAddress.sin_family = AF_INET;
  // serverAddress.sin_addr.s_addr = servinfo[0].; //localhost
  // serverAddress.sin_port = htons(PORT); 
 
  // addressLength = sizeof(serverAddress);

  //Allocate and initialize the structures passed to the send and receive functions
  echoSendKit *sendKit = (echoSendKit*)malloc(sizeof(echoSendKit));
  echoRcvKit *rcvKit = (echoRcvKit*)malloc(sizeof(echoRcvKit));
  initializeEchoSendKit(interval, packetSize, numberOfMessages, it->ai_addr, it->ai_addrlen, socketDescriptor, sendTimes, sendKit);

  initializeEchoRcvKit(interval, packetSize, numberOfMessages, it->ai_addr, it->ai_addrlen, socketDescriptor, sendTimes, rcvTimes, timeOut, rcvKit);

  //Declare the threads
  pthread_t sendThread, rcvThread;

  //Initialize a lock
  if (pthread_mutex_init(&lock, NULL) != 0)
  {
      printf("\n mutex init failed\n");
      return 1;
  }

  //Start the clock
  clock_t startTime = clock();

  //Create the threads for the send and receive functions
  if(pthread_create(&sendThread, NULL, sendEchoMessages, sendKit)!=0)
  {
    perror("Could not create Send Thread!\n");
    exit(0);
  }

  
  if(pthread_create(&rcvThread, NULL, receiveEchoMessages, rcvKit))
  {
    perror("Could not create Receive Thread!\n");
    exit(0);
  }

  //join the threads after completion
  if(pthread_join(sendThread, NULL)!=0)
  {
    perror("Could not join Send Thread!\n");
    exit(0);
  }
  if(pthread_join(rcvThread, NULL)!=0)
  {
    perror("Could not join Receive Thread!\n");
    exit(0);
  }

  //Get the Statistics
  char ip[INET6_ADDRSTRLEN];
  inet_ntop(it->ai_family, it->ai_addr, ip, INET6_ADDRSTRLEN);
  clock_t maxrtt = 0, avgrtt = 0;
  clock_t minrtt = INT_MAX;
  int lost = 0, rcved = 0, percentloss = 0;
  for(int i=0;i<numberOfMessages;i++)
  {
    clock_t rtt = difftime(rcvKit->rcvTimes[i], rcvKit->sendTimes[i]);
    if(rtt>=0)
    {
      if (rtt > maxrtt)
        maxrtt = rtt;
      if (rtt < minrtt)
        minrtt = rtt;
      avgrtt+=rtt;
    }
    else
      lost++;
  }
  if(minrtt == INT_MAX)
    minrtt = 0;
  avgrtt/=numberOfMessages;
  rcved = numberOfMessages - lost;
  percentloss = (lost*100)/numberOfMessages;
  printf("Ping statistics for %s\n", ip);
  printf("\tPackets: Sent = %d, Recieved = %d, Lost=%d (%d%% loss)\n", numberOfMessages, rcved, lost, percentloss);
  printf("Approximate round trip times\n");
  printf("\tMinimum = %ld, Maximum = %ld, Average = %ld\n", minrtt, maxrtt, avgrtt);

  printf("Closing Client!\n");
  // sendto(socketDescriptor,sendMessage,MAXLINE,0,(struct sockaddr*)&serverAddress,addressLength);
  // recvfrom(socketDescriptor,recvMessage,MAXLINE,0,NULL,NULL);
 
  // printf("\nServer's Echo : %s\n",recvMessage);

  freeaddrinfo(servinfo);
  if(serverAddrString!=NULL)
    free(serverAddrString);
  if(sendKit!=NULL)
    free(sendKit);
  if(rcvKit!=NULL)
    free(rcvKit);
  if(sendTimes!=NULL)
    free(sendTimes);
  if(rcvTimes!=NULL)
    free(rcvTimes);


  return 0;
}
