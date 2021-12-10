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
#include <pthread.h>

#define MAXLINE 1024  
#define PORT "3490"
int socketDescriptor;
struct sockaddr_in  clientAddress;
int delay=2;
clock_t processingTimeTable[5] = {2, 5, 10, 15, 20}; //seconds
int tables[10] = {0};
pthread_mutex_t lock;
struct order{
  int tableNum;
  clock_t arrived;
  clock_t timeToProcess;
  clock_t timeToComplete;
  struct sockaddr_in  clientAddress;
  int sent;
  int contents[5];
}orderQueue[10];
int front = -1;
int rear = -1;
clock_t queuingDelay = 0;

// void delay(int seconds)
// {
//     int milliSeconds = 100000 * seconds;
  
//     clock_t startTime = clock();
  
//     while (clock() < startTime + milliSeconds)
//         ;
// }

int parseNum(char *message, int* it, char end)
{
  int i=*it;
  int id = 0;
  while(message[i] != end)
  {
    id = id*10+(message[i]-'0');
    i++;
  }
  *it=i;
  return id;
}

void *cast_ipv(struct sockaddr* sa)
{
    return &(((struct sockaddr_in*)sa)->sin_addr);
}

void* rcvOrders(void *arg)
{
  int c=0;
  while(1){
    // printf("\n");
    int addressLength = sizeof(clientAddress);
    char receipt[1024], message[1024];
    int number;
    number = recvfrom(socketDescriptor,message,MAXLINE,0,(struct sockaddr*)&clientAddress,&addressLength);
    clock_t arrived = clock();
    char ip[INET_ADDRSTRLEN];
    inet_ntop(clientAddress.sin_family,&(clientAddress.sin_addr),ip, INET_ADDRSTRLEN);
    printf("\n Order received. %s\n ", message); 
    int i = 0;
    int tableNum = parseNum(message, &i, ',');
    i++;
    if(tables[tableNum]==0)
    {
      if((rear-front+1)%1000!=0)
      {
        if(front==-1)
          front++;
        rear=(rear+1)%1000;
        printf("Printing order\n%d\n",tableNum);
        orderQueue[rear].clientAddress = clientAddress;
        orderQueue[rear].tableNum = tableNum;
        orderQueue[rear].arrived = arrived;
        orderQueue[rear].timeToProcess = 0;
        // printf("sent %d %d\n",rear,orderQueue[rear].sent);
        while(message[i]!='\0')
        {
          // printf("%d\n",i);
          int item = parseNum(message, &i, '-');
          // printf("%d\n",i);
          i++;
          // printf("%d\n",i);
          int qty = parseNum(message, &i, ',');
          i++;
          // break;
          orderQueue[rear].contents[item] = qty;
          orderQueue[rear].timeToProcess += qty*processingTimeTable[item];
          printf("%d %d\n",item, qty);
        }
        pthread_mutex_lock(&lock);
        orderQueue[rear].timeToComplete = orderQueue[rear].timeToProcess + queuingDelay;
        tables[tableNum] = orderQueue[rear].timeToComplete;
        queuingDelay += orderQueue[rear].timeToProcess;
        pthread_mutex_unlock(&lock);
      }

    }
    
    sprintf(receipt,"%d", tables[tableNum]);
    // printf("\n Client's Message: %s ",message);

    if(number<1)
      perror("send error");

    int n = rand();
    if(c==0 || n%c==0)
    {
      sleep(delay);
    }
    // printf("n=%d c=%d\n",n, c);
      // printf("%s\n", message);
    pthread_mutex_lock(&lock);
    sendto(socketDescriptor,receipt,strlen(receipt)+1,0,(struct sockaddr*)&(orderQueue[rear].clientAddress),addressLength);
    orderQueue[rear].sent = 1;
    pthread_mutex_unlock(&lock);
    // printf("Hello");
    if(c>20)
      c=0;
    else
      c++;
  }
}

void* processOrders(void *arg)
{
  while(1)
  {
    while(front !=-1 && rear !=-1)
    {
      char reply[1024];
      pthread_mutex_lock(&lock);
      int sent = orderQueue[front].sent;
      if(sent == 1) orderQueue[front].sent = 0;
      // printf("sent1 %d %d\n",front,sent);
      pthread_mutex_unlock(&lock);
      if(sent==1)
      {
        struct order curOrder = orderQueue[front];
        sprintf(reply,"%d",curOrder.tableNum);
        printf("Starting to process order for table %d\n", curOrder.tableNum);
        strcat(reply, "OIP");
        int len = strlen(reply);
        sendto(socketDescriptor,reply,len+1,0,(struct sockaddr*)&(curOrder.clientAddress),sizeof(clientAddress));
        if(front == rear)
        {
          front=-1;
          rear=-1;
        }
        else
        {
          front = (front+1)%1000;
        }
        for(int i=0;i<curOrder.timeToProcess;i++)
        {
          sleep(1);
          pthread_mutex_lock(&lock);
          queuingDelay--;
          pthread_mutex_unlock(&lock);
          // printf("%d\n",i);
        }
        reply[len-1]='C';
        printf("Order Complete for table %d\n", curOrder.tableNum);
        sendto(socketDescriptor,reply,len+1,0,(struct sockaddr*)&(curOrder.clientAddress),sizeof(clientAddress));
        tables[curOrder.tableNum] = 0;
        
      }
    }
  }
}

void initializeOrderQueue()
{
  for(int i=0;i<1000;i++)
  {
    orderQueue[i].tableNum=0;
    orderQueue[i].arrived = -1;
    orderQueue[i].timeToProcess = 0;
    orderQueue[i].timeToComplete = 0;
    orderQueue[i].sent=0;
    for(int j=0;j<5;j++)
    {
      orderQueue[i].contents[j] = 0;
    }
  }
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
      delay = parseNum(argv[i], NULL,'\0');
    }
    else 
    {
      serverAddrString = (char*)malloc(sizeof(*argv[i]));
      strcpy(serverAddrString, argv[i]);
    }
  }

  printf("%s\n", serverAddrString==NULL?"localhost":serverAddrString);
  int number;
  char message[MAXLINE];

  //Get IP from name
  int ev;
  struct addrinfo hints, *servinfo, *it;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  if((ev = getaddrinfo(serverAddrString, PORT, &hints, &servinfo))!=0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ev));
    return 1;
  }

  for(it = servinfo; it != NULL; it = it->ai_next)
  {
    if((socketDescriptor = socket(AF_INET, it->ai_socktype, it->ai_protocol))!=-1)
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

  initializeOrderQueue();

  char sa[INET_ADDRSTRLEN];
  inet_ntop(it->ai_family,cast_ipv(it->ai_addr),sa, INET_ADDRSTRLEN);
  printf("\nServer Started ...%s\n",sa);

  freeaddrinfo(servinfo);
  pthread_t processThread, rcvThread;

  if (pthread_mutex_init(&lock, NULL) != 0)
  {
    printf("\n mutex init failed\n");
    return 1;
  }

  if(pthread_create(&rcvThread, NULL, rcvOrders, NULL)!=0)
  {
    perror("Could not create Send Thread!\n");
    exit(0);
  }

  
  if(pthread_create(&processThread, NULL,processOrders, NULL))
  {
    perror("Could not create Receive Thread!\n");
    exit(0);
  }

  //join the threads after completion
  if(pthread_join(rcvThread, NULL)!=0)
  {
    perror("Could not join Send Thread!\n");
    exit(0);
  }
  if(pthread_join(processThread, NULL)!=0)
  {
    perror("Could not join Receive Thread!\n");
    exit(0);
  }
  close(socketDescriptor);
  return 0;
}
