# To test
## TFTP Server
* Compile.
        
        gcc -o tftp_server tftpserver.c
* Run the executable with port number passed as argument.
        
        ./tftp_server 8089
* On a different terminal, run the tftp client (install it if not already present on your Linux machine).
        
        tftp
  You can set mode to ascii by entering `mode ascii` at the newly opened tftp client prompt. Mode can also be set to octet by entering `mode octet`.<br>
  Once appropriate mode is set, connect to the running tftp server by entering `connect localhost 8089` at the client prompt. <br>
  <i><b>Note:</b> The last argument must match the port number the server is running on.</i>

* To get any file, enter `get <filepath>` on the tftp client prompt. The filepath if relative, must be relative to the location where the tftp server is running. 
* To exit tftp client, type `quit`. To exit tftp server, hit `Ctrl-C`.

## Faster traceroute
* Compile.
        
        gcc -o faster_traceroute fastertraceroute -lpthread
* Run executable as root (to facilitate raw socket creation), passing the host name (ex - zapak.com) as argument.
        
        sudo faster_traceroute <hostname>
