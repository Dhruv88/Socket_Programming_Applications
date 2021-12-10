# Socket_Programming_Applications
Consists of Three socket programming applications including applications similar to ping, iperf and an online ordering and notification system

# Part-1a: Ping like Application
The application takes mimics the ping in windows and senda and receives echo messages and replies. Both server and client have are there

# Part-1b: Iperf like Application
The application is similar the iperf and calculates throughput and average delay

# Part-2: An online ordering and notification system for restaurants
This application allows the users to place an order in an restaurant online and gives them an accurate time estimate of time required to process the order.

# Part-3: It is same as Part-1a except that it IPv4/IPv6 independent

The link of folders for demo videos showing how to run them and brief explanation of code

# Instructions regarding compiling the code and the command line options

Make sure your current working directory is the respective part directory. For example if you want to run part-1a code then your current working directory should be Part-1a. Then run the following commands.

For server go into the server folder
To compile the server: gcc -pthread -g server.c
To run the server: ./a.out <server-address> <options>

For <server-address> in case of Part-1a,1b,2 the server-address has to be ipv4 and if left empty then default is localhost. For part-3 it can be either ipv4 or ipv6 and if left empty the default is ip6-localhost.

For <options> there is:
-d for delay in seconds to be used in loss mechanism implemented in server. The value has to be number. Default is 2s. 

Eg: ./a.out 127.0.0.2 -d 4
Here the server address is 12.0.0.2 and delay is 4s.

For client go into the client folder
To compile the client: gcc -pthread -g client.c
To run the client: ./a.out <server-address> <options>

For <server-address> in case of Part-1a,1b,2 the server-address has to be ipv4 and if left empty then default is localhost. For part-3 it can be either ipv4 or ipv6 and if left empty the default is ip6-localhost.

For <options>
**Options available in Part-1a and 3:**
-i the interval in seconds between sending of two consecutive packets. Min is 1sec and default is 2sec.
-s the size of packet to be transmitted in bytes. Default is 8 and keep size >= 8 to ensure proper working always.
-n the number of echo messages to be sent. Default is 6.
-t the time to wait in seconds. If packet not received within this time then it is considered as timed out.

**Options available in Part-1b:**
-s the size of packet to be transmitted in bytes. Default is 8 and keep size >= 8 to ensure proper working always.

**Options available in Part-2:**
-t the time to wait in seconds. If packet not received within this time then it is considered as timed out.

General Eg: ./a.out 127.0.0.2 -i 4 -s 10 -n 8 -t 2
Here server-address is 1227.0.0.2. The interval is 4s, size of packet is 10 bytes, number of packets to be sent is 8 and timeout is 2s.

**Note:** When closing the client use Ctrl+C.

**Note:** In Part 1a and 3 I made a small update now it prints the port number as well in server for the client. This was not there in videos and examples but the code files have this. So when you run the code the port number of client will be printed by the server.
 
