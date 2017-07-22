Author: Joshua Kluthe
Date: 2017.03.10

To compile: Use command "gcc ftserver.c -o ftserver"

To run: Use command "ftserver [PORT]", wehere [PORT] is the integer port number to bind the
server socket to. Then use command 
"python ftclient [HOST] [CONTROL PORT] [COMMAND] [FILENAME] [DATA PORT]", where [HOST] is the 
hostname of the server, [CONTROL PORT] is the port number of the server, [COMMAND] is either 
-g to get a file or -l to list the directory contents, [FILENAME] is the name of the file to get and
is only used if the [COMMAND] is -g, and [DATA PORT] is the port number to set up the secondary data
connection on.

