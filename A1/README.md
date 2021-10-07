# Problem statement

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
# To test
* First compile.
        
        gcc -o d_server d_server.c
        gcc -o m_server m_server.c
        gcc -o client client.c

* Open 3 terminals and on the first, run meta data server with the number of data servers required (a multiple of 3) passed as argument.
        
        ./m_server 6
* On the second terminal, run data server with the same argument passed. <br>
(Meta data server has to be run first so that the message queues used for communication are created correctly).
        
        ./d_server 6
* Run the client on third terminal. A menu would pop-up, using which you can add a file, copy a file, move a file, remove a file, send command to data server, or exit. 
        
        ./client
<br>
<i><b>Note:</b> No new folders for the data servers need to be created - it will be done automatically at the location where the d_server executable is run.<i>
