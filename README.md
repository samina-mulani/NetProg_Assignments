# NetProg_Assignments
Codes for projects done as a part of the course Network Programming.
<br>Following are the problem statements-

## Assignment 1

Implement a client-server system within an operating systems using IPC mechanisms such
as message queues. The requirements are as follows.
* The system is file storage and data processing system. It has metadata server (M), data
servers (D) and a client (C).
* A client submits request to add a file at a given path to the M server. M server makes
changes to file hierarchy and replies. 
* Then client divides the file into blocks of CHUNK_SIZE (taken as input). For each chunk,
C will send ADD_CHUNK request to M server. M returns list of addresses of three D
servers. C will send chunk of data directly to D. D will reply with status. D stores each
chunk as a file within its directory.
* To copy file from one path to another, C sends CP request to M. M will change metadata
and reply. M marks the chunks to be copied. In the next communication to D servers, M
instructs them where to copy the chunk.
* To move file, C sends MV request. M changes file hierarchy and reply.
* To delete file, C sends RM request. M changes file hierarchy and marks chunks to be
deleted. In the next communication between M and D, M will instruct Ds to remove the
chunks.
* C can send a command to Ds to be executed on the chunks stored and send the result
back to C. For e.g. C sends wc –w/m/l/c to D servers. D servers execute this command
on their local chunks and send the results back. Other e.g. may be cat, od, ls, sort …

## Assignment 2

### TFTP server
TFTP (Trivial File Transfer Protocol) is used to transfer files between two machines. It
doesn’t need any authentication like FTP. Use UDP socket programming to implement a TFTP
server that can handle Read Requests (RRQs). Write requests (WRQs) need not be implemented.
TFTP protocol is defined in RFC 1350 [http://www.ietf.org/rfc/rfc1350.txt]. Use any TFTP
client [command-line program ‘tftp’ installed on Linux machines] to test your TFTP server. The
server tftpserver.c needs to achieve the following requirements:
* tftp server should accept a port number as command line argument over which it waits
for incoming connections. It is a concurrent server using IO multiplexing i.e. using select
() system call.
[shell$ ./tftp 8069]
* Your server should print out information about every message received and every
message sent with client identity. It uses RRQ, DATA, ACK, and ERROR messages
wherever they are required. The sequence of steps required for downloading a file
  1. The client C sends an RRQ (read request) to Server S (at the specified port),
containing the filename and transfer mode.
  2. S replies with a DATA message to C. Message is sent from a port other than the one
it used for receiving RRQ and client needs to send further messages to S at this port.
In each DATA, 512 bytes of data is sent.
  3. All DATA messages are sequentially numbered. Once S sends DATA message, it will
not send another DATA message unless it receives ACK for this DATA message. C
responds to each DATA with corresponding ACK. Once S receives ACK for DATA
number n, it sends DATA number n+1. If the ACK is not received within a specified
time, S resends DATA.
  4. C detects termination of data transfer if it receives DATA with <512 bytes size. If the
total size of the file is exactly a multiple of 512, then a zero-size DATA is sent to
indicate the termination of transfer.
* Your server should support binary mode (octet) of transferring data. Time-out and
retransmission mechanisms should be used to ensure delivery of messages. You can use
a timeout of 5 seconds for receiving acknowledgements. No of retries for retransmitting can be utmost 3. Server should respond with appropriate ERROR message wherever it
is required either when it receives RRQ or while transferring the file. See RFC for list of
error options. 

### Faster trace route
The program operates by sending UDP datagrams at increasing TTL and receiving the
ICMP replies to determine the router’s address at the respective TTL.
* Let us implement a fastertraceroute.c which finds out the addresses of the intermediate
routers in parallel. There is one thread per one TTL i.e. one thread for TTL=1, another
thread for TTL=2, another thread for TTL=3 etc. There is one single thread ‘icmp’ which
reads ICMP replies over the raw socket. ‘icmp’ thread doesn’t process the replies itself but
delivers the received reply to the respective thread which has sent the datagram. Then
individual threads process the reply and display the result. Design and implement this
multithreaded program 
