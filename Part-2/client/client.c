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
#define PORT "3490"
pthread_mutex_t lock;
char order[MAXLINE]; 

typedef struct echosendkit{
  struct sockaddr *to;
  int tolen;
  int socfd;
  clock_t* sendTime;
} echoSendKit;

typedef struct echorcvkit{
  struct sockaddr *from;
  int fromlen;
  int socfd;
  clock_t* sendTime;
  clock_t* rcvTime;
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

void *sendEchoMessages(void *arg)
{
  echoSendKit *sendKit = (echoSendKit*)arg;
  char cont = 'y';
  while(cont == 'y')
  {
    //Take order
    int orderLen = 0;
    printf("Place your order. Enter the dish table number number-quantity separated by comma(eg:1,1-2,2-3. here first 1 is the table number followed by the items ordered)\n");
    do{
    scanf("%s", order);
    orderLen = strlen(order);
    if(orderLen == 0)
      printf("Order cannot be empty!Please place a valid order\n");
    }while(orderLen == 0);
    order[orderLen++] = ',';
    order[orderLen] = '\0';
    printf("%s\n", order);
    
    //send message
    pthread_mutex_lock(&lock);
    *(sendKit->sendTime) = clock();
    int temp = sendto(sendKit->socfd,order,orderLen+1,0,sendKit->to,sendKit->tolen);
    pthread_mutex_unlock(&lock);
    // printf("send: %d\n", temp);
    //delay
    // sleep(sendKit->interval);
    cont = 'n';
  }
}

void *receiveEchoMessages(void *arg)
{
  echoRcvKit *rcvKit = (echoRcvKit*)arg;
  int expecting = 1;
  while(expecting)
  {
    //check for timeout for each message
    pthread_mutex_lock(&lock);
    if(*(rcvKit->sendTime)!=-1)
    {
      if(*(rcvKit->rcvTime)==0 && clock()>*(rcvKit->sendTime)+(1000000*rcvKit->timeOut))
      {
        // printf("rcv: %ld ", rcvKit->rcvTime);
        printf("Resending order\n");
        *(rcvKit->sendTime) = clock();
        int temp = sendto(rcvKit->socfd,order,strlen(order)+1,0,rcvKit->from,rcvKit->fromlen);
      }
    }
    pthread_mutex_unlock(&lock);
    // printf("d%d\n", expecting);
    //initialize message
    char message[1000];
    
    //receive message
    int rcvlen = recvfrom(rcvKit->socfd,message,10,MSG_DONTWAIT,rcvKit->from,&(rcvKit->fromlen));
    if(rcvlen!=-1)
    {
      clock_t rcvTime = clock();
      // int id = parseNum(message, '$');
      // printf("outside\n");
      pthread_mutex_lock(&lock);
      // printf("inside\n");
      *(rcvKit->rcvTime) = rcvTime; 
      printf("Please wait for %s s\n", message);
      int timer = parseNum(message, '\0');
      do{
      rcvlen = recvfrom(rcvKit->socfd,message,4,0,rcvKit->from,&(rcvKit->fromlen));
      }while(message[rcvlen-2]>='0' && message[rcvlen-2]<='9');
      if(rcvlen!=-1)
      {
        printf("Order is being processed\n");
      }
      do{
      rcvlen = recvfrom(rcvKit->socfd,message,4,0,rcvKit->from,&(rcvKit->fromlen));
      }while(message[rcvlen-2]>='0'&&message[rcvlen-2]<='9');
      if(rcvlen!=-1)
      {
        printf("Order is completed\n");
      }
      expecting=0;
      // printf("end\n");
      pthread_mutex_unlock(&lock);
    }
    // printf("a%d\n", expecting);
  }
}

void initializeEchoSendKit(struct sockaddr *to, int tolen, int socfd, clock_t* sendTime, echoSendKit *sendKit)
{
  sendKit->to = to;
  sendKit->tolen = tolen;
  sendKit->socfd = socfd;
  sendKit->sendTime = sendTime;
}

void initializeEchoRcvKit(struct sockaddr *from, int fromlen, int socfd, clock_t* sendTime, clock_t* rcvTime, int timeOut, echoRcvKit *rcvKit)
{
  rcvKit->from = from;
  rcvKit->fromlen = fromlen;
  rcvKit->socfd = socfd;
  rcvKit->sendTime = sendTime;
  rcvKit->rcvTime = rcvTime;
  rcvKit->timeOut = timeOut;
}

int main(int argc, char* argv[]){
  //params
  int timeOut = 2; //in seconds
  char* serverAddrString = NULL; 
  int addressLength;
  // make a socket for the client
  int socketDescriptor = socket(AF_INET, SOCK_DGRAM, 0);

  //Parse all the arguments
  for(int i=1;i<argc;i++)
  {
    if(argv[i][0] == '-' && argv[i][1] == 't')
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

  printf("Server Address: %s\n", serverAddrString==NULL?"localhost":serverAddrString);
  printf("timeout: %d s\n", timeOut);

  //Get IP from name
  int ev;
  struct addrinfo hints, *servinfo;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  if((ev = getaddrinfo(serverAddrString, PORT, &hints, &servinfo))!=0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ev));
    return 1;
  }
  
  //Allocate and initialize the time arrays
  clock_t sendTime = -1;
  clock_t rcvTime = 0;

  // set the server info
  // struct sockaddr_in serverAddress;
  // serverAddress.sin_family = AF_INET;
  // serverAddress.sin_addr.s_addr = servinfo[0].; //localhost
  // serverAddress.sin_port = htons(PORT); 
 
  // addressLength = sizeof(serverAddress);

  //Allocate and initialize the structures passed to the send and receive functions
  echoSendKit *sendKit = (echoSendKit*)malloc(sizeof(echoSendKit));
  echoRcvKit *rcvKit = (echoRcvKit*)malloc(sizeof(echoRcvKit));
  initializeEchoSendKit(servinfo->ai_addr, servinfo->ai_addrlen, socketDescriptor, &sendTime, sendKit);

  initializeEchoRcvKit(servinfo->ai_addr, servinfo->ai_addrlen, socketDescriptor, &sendTime, &rcvTime, timeOut, rcvKit);

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
  // char ip4[INET_ADDRSTRLEN];
  // inet_ntop(AF_INET, &(((struct sockaddr_in *)servinfo->ai_addr)->sin_addr), ip4, INET_ADDRSTRLEN);
  // clock_t maxrtt = 0, avgrtt = 0;
  // clock_t minrtt = INT_MAX;
  // int lost = 0, rcved = 0, percentloss = 0;
  // for(int i=0;i<numberOfMessages;i++)
  // {
  //   clock_t rtt = difftime(rcvKit->rcvTimes[i], rcvKit->sendTimes[i]);
  //   if(rtt>=0)
  //   {
  //     if (rtt > maxrtt)
  //       maxrtt = rtt;
  //     if (rtt < minrtt)
  //       minrtt = rtt;
  //     avgrtt+=rtt;
  //   }
  //   else
  //     lost++;
  // }
  // if(minrtt == INT_MAX)
  //   minrtt = 0;
  // avgrtt/=numberOfMessages;
  // rcved = numberOfMessages - lost;
  // percentloss = (lost*100)/numberOfMessages;
  // printf("Ping statistics for %s\n", ip4);
  // printf("\tPackets: Sent = %d, Recieved = %d, Lost=%d (%d%% loss)\n", numberOfMessages, rcved, lost, percentloss);
  // printf("Approximate round trip times\n");
  // printf("\tMinimum = %ld, Maximum = %ld, Average = %ld\n", minrtt, maxrtt, avgrtt);

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

  return 0;
}
