#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
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
int sent[10]={0},receive[10]={0};
int sthroughput[10]={0}, rthroughput[10]={0};
clock_t pdelay[10]={0};
int totalSent=0,totalRcv=0;
int s=0;

typedef struct echosendkit{
  int interval;
  int packetSize;
  struct sockaddr *to;
  int tolen;
  int socfd;
  clock_t *sendTimes;
} echoSendKit;

typedef struct echorcvkit{
  int interval;
  int packetSize;
  struct sockaddr *from;
  int fromlen;
  int socfd;
  clock_t *sendTimes;
  clock_t *rcvTimes;
} echoRcvKit;



void delay(int seconds)
{
    int milliSeconds = 50 * seconds;
  
    clock_t startTime = clock();
  
    while (clock() < startTime + milliSeconds)
        ;
}

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
  for(int k=0;k<10;k++)
  {
    clock_t curTime = clock();
    int c=0;
    while(clock()<curTime+1000000)//&&c<=left)
    { 
    // create message
        char *message = (char *)malloc(sizeof(char) * 100);
        int len = sprintf(message, "%d", totalSent);
        int paddingLen = 100 - len;
        char *padding = (char *)malloc(sizeof(char) * paddingLen);
        memset(padding, '$', paddingLen - 1);
        padding[paddingLen-1]='\0';
        strcat(message, padding);

        // send message
        pthread_mutex_lock(&lock);
        sendKit->sendTimes[totalSent] = clock();
        int temp = sendto(sendKit->socfd, message, sendKit->packetSize, 0, sendKit->to, sendKit->tolen);
        c++,totalSent++;
        pthread_mutex_unlock(&lock);
        // printf("send: %d\n", temp);
        // delay
        delay(sendKit->interval);
        if (message != NULL)
          free(message);
        if (padding != NULL)
          free(padding);
    }
    sent[k]=c;
  }
  printf("Sent Packets = %d\n", totalSent);
  s=1;
}

void *receiveEchoMessages(void *arg)
{
  echoRcvKit *rcvKit = (echoRcvKit*)arg;
  for(int i=0;i<10&&s==0;i++)
  {
    int c=0;
    clock_t startTime = clock();
    while(clock()< 1000000 + startTime)
    {
      //initialize message
      char* message = (char*)malloc(sizeof(char)*rcvKit->packetSize);
      
      //receive message
      int rcvlen = recvfrom(rcvKit->socfd,message,rcvKit->packetSize,MSG_DONTWAIT,rcvKit->from,&(rcvKit->fromlen));
      if(rcvlen!=-1)
      {
        c++,totalRcv++;
        clock_t rcvTime = clock();
        int id = parseNum(message, '$');
        pthread_mutex_lock(&lock);
        rcvKit->rcvTimes[id] = rcvTime; 
        pthread_mutex_unlock(&lock);
      }
    }
    receive[i]=c;
  }
  printf("Received packets = %d\n",totalRcv);
}

void initializeEchoSendKit(int interval, int packetSize, struct sockaddr *to, int tolen, int socfd, clock_t* sendTimes, echoSendKit *sendKit)
{
  sendKit->interval = interval;
  sendKit->packetSize = packetSize;
  sendKit->to = to;
  sendKit->tolen = tolen;
  sendKit->socfd = socfd;
  sendKit->sendTimes = sendTimes;
}

void initializeEchoRcvKit(int interval, int packetSize, struct sockaddr *from, int fromlen, int socfd, clock_t* sendTimes, clock_t* rcvTimes, echoRcvKit *rcvKit)
{
  rcvKit->interval = interval;
  rcvKit->packetSize = packetSize;
  rcvKit->from = from;
  rcvKit->fromlen = fromlen;
  rcvKit->socfd = socfd;
  rcvKit->sendTimes = sendTimes;
  rcvKit->rcvTimes = rcvTimes;
}

int main(int argc, char* argv[]){
  //params
  int interval = 1; //in seconds
  int packetSize = 100; //in bytes 
  char* serverAddrString = NULL; 
  int addressLength;
  // make a socket for the client
  int socketDescriptor = socket(AF_INET, SOCK_DGRAM, 0);

  //Parse all the arguments
  for(int i=1;i<argc;i++)
  {
    if(argv[i][0] == '-' && argv[i][1] == 's')
    {
      i++;
      packetSize = parseNum(argv[i],'\0');
    }
    else 
    {
      serverAddrString = (char*)malloc(sizeof(*argv[i]));
      strcpy(serverAddrString, argv[i]);
    }
  }

  printf("Server Address: %s\n", serverAddrString==NULL?"localhost":serverAddrString);
  printf("packet size: %d bytes\n", packetSize);

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
  clock_t *sendTimes = (clock_t*)malloc(sizeof(clock_t)*200000);
  clock_t *rcvTimes = (clock_t*)calloc(200000,sizeof(clock_t));
  for(int i=0;i<200000;i++)
  {
    sendTimes[i] = -1;
  }

  //Allocate and initialize the structures passed to the send and receive functions
  echoSendKit *sendKit = (echoSendKit*)malloc(sizeof(echoSendKit));
  echoRcvKit *rcvKit = (echoRcvKit*)malloc(sizeof(echoRcvKit));
  initializeEchoSendKit(interval, packetSize, servinfo->ai_addr, servinfo->ai_addrlen, socketDescriptor, sendTimes, sendKit);

  initializeEchoRcvKit(interval, packetSize, servinfo->ai_addr, servinfo->ai_addrlen, socketDescriptor, sendTimes, rcvTimes, rcvKit);

  //Declare the threads
  pthread_t sendThread, rcvThread;

  //Initialize a lock
  if (pthread_mutex_init(&lock, NULL) != 0)
  {
      printf("\n mutex init failed\n");
      return 1;
  }

  //Start the clock
  printf("Sending Packets\n");
  clock_t startTime = clock();

  //Create the threads for the send and receive functions
  if(pthread_create(&sendThread, NULL, sendEchoMessages, sendKit)!=0)
  {
    perror("Could not create Send Thread!\n");
    exit(0);
  }

// sleep(1);
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
  for(int i=0;i<10;i++)
  {
    sthroughput[i]=8*packetSize*sent[i];
    rthroughput[i]=8*packetSize*receive[i];
  }
  int savgtpt = (8*packetSize*totalSent)/10;
  int ravgtpt = (8*packetSize*totalRcv)/10;

  int avgpack = totalRcv/10;
  int j=0;
  clock_t avgdelay=0;
  for(int i=0;i<10;i++)
  {
    int c=0;
    for(int k=0;k<avgpack;k++)
    {
      if(rcvTimes[j*avgpack+k]!=0)
      {
        pdelay[i] += (difftime(rcvTimes[j*avgpack+k],sendTimes[j*avgpack+k])/2);
        c++;
      }
    }
    j++;
    avgdelay+=pdelay[i];
    pdelay[i]/=c;
    // printf("%ld\n", pdelay[i]);
  }
  avgdelay/=totalRcv;
  char ip4[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &(((struct sockaddr_in *)servinfo->ai_addr)->sin_addr), ip4, INET_ADDRSTRLEN);
  printf("ThroughPut and Delay Statistics for %s\n", ip4);
  printf("Avg Send Throughput = %d bps Avg Recieve Throughput = %d bps\n", savgtpt, ravgtpt);
  printf("Avg delay = %ld microseconds\n",avgdelay);

  printf("Storing the values in plot.txt\n");
  FILE *fptr = fopen("plot.txt", "w");
  for(int i=0;i<10;i++)
    fprintf(fptr, "%d %d %d %ld\n", i+1,sthroughput[i], rthroughput[i], pdelay[i]);

  fclose(fptr);
  printf("Plotting the Graphs\n");
  int p1 = fork();

  if (p1 == 0)
  {
    /* This is the child process.  Execute the shell command. */
    char *args[3];
    args[0] = strdup("gnuplot");
    args[1] = strdup("plot_throughput.plt");
    args[2] = NULL;
    int p2 = fork();
    if (p2 == 0)
    {
      /* This is the child process.  Execute the shell command. */
      char *args1[3];
      args1[0] = strdup("gnuplot");
      args1[1] = strdup("plot_delay.plt");
      args1[2] = NULL;
      
      execvp(args1[0],args1);
    }
    else if (p2 < 0)
      /* The fork failed.  Report failure.  */
      exit(1);
    else
    {
      /* This is the parent process.  Wait for the child to complete.  */
      execvp(args[0],args);
    }
  }
  else if (p1 < 0)
    /* The fork failed.  Report failure.  */
    exit(1);
  else
    /* This is the parent process.  Wait for the child to complete.  */
    wait(NULL);

  printf("Graphs Plotted. Closing Client!\n");

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
