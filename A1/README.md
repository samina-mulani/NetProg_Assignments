# To test
* First compile.
``` 
gcc -o d_server d_server.c
gcc -o m_server m_server.c
gcc -o client client.c
```

* Open 3 terminals and on the first, run meta data server with the number of data servers required (a multiple of 3) passed as argument.
```
./m_server 6
```
* On the second terminal, run data server with the same argument passed. <br>
(Meta data server has to be run first so that the message queues used for communication are created correctly).
```
./d_server 6
```
* Run the client on third terminal. A menu would pop-up, using which you can add a file, copy a file, move a file, remove a file, send command to data server, or exit. 
```
./client
```
<br>
<i><b>Note:</b> No new folders for the data servers need to be created - it will be done automatically at the location where the d_server executable is run.<i>
