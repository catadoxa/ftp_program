#!/usr/bin/env python3

"""
Author: Joshua Kluthe
Date: 2017.03.10
Description: This connects to a server and then sets up a second connection on which to receive 
either a file or the current working directory contents from the server.
"""

from sys import argv
import socket


def listen_for_data_conn(this_address):
    """
    listen_for_data_conn() sets up a server on which to receive data and waits a few seconds for a
    connection. After this it assumes the connection failed and times out. On success, returns the
    connction object.
    """
    data_server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    data_server.bind(this_address)
    data_server.settimeout(3)
    try:
        data_server.listen(1)
        return data_server.accept()
    except:
        print("CLIENT: Data connection failed.")
        exit(1)


def parse_args():
    """
    parse_args() simply parses and validates the command line arguments and returns them in nice
    packages.
    """
    if len(argv) == 5:
        command = argv[3]
    elif len(argv) == 6:
        command = "{} {}".format(argv[3], argv[4])
    else:
        print("CLIENT: Usage error. Proper usage is\n'ftclient [HOST] [CONTROL PORT] -g [FILENAME] [DATA PORT]' or\n'ftclient [HOST] [CONTROL PORT] -l [DATA PORT]'")
        exit(1)
    server_address = (argv[1], int(argv[2]))
    this_address = (socket.gethostname(), int(argv[len(argv) - 1]))
    if server_address[1] is False or this_address[1] is False:
        print("CLIENT: Usage error. CONTROL PORT and DATA PORT must be integers")
        exit(1)
    return server_address, this_address, command


def get_data(data_conn):
    """
    get_data() receives either a header containing the size of the data it is to receive or an
    error message. If not an error, it loops until it has received all the data indicated in the
    header and then returns it.
    """
    recvd_data = data_conn.recv(127).decode()
    try:
        total_data = int(recvd_data)
        data_conn.send("ACK\0".encode())
    except:
        print("ERROR. Server says \"{}\".".format(recvd_data))
        exit()
    data = ""
    while len(data) < total_data:
        data += data_conn.recv(1024).decode()
    return data


def get_dir_list(data_conn):
    """
    get_dir_list() gets the contents of the current working directory from the server.
    """
    print("Receiving directory contents.")
    dir_list = get_data(data_conn)
    print dir_list


def get_file(data_conn, filename):
    """
    get_file gets the contents of the file indicated by filename from the server, then writes it to 
    'recvd_filename'.
    """
    file_contents = get_data(data_conn)
    fd = open("recvd_{}".format(filename), "w")
    print("Receiving \"{}\". Saving file to \"recvd_{}\"".format(filename, filename))
    fd.write(file_contents)
    fd.close()
    print("File transfer complete.")


def main():
    """
    Usage: ftclient [HOST] [CONTROL PORT] [COMMAND] [FILENAME] [DATA PORT]
    
    arg HOST: hostname of the server to connect to
    arg CONTROL PORT: port on the server to connect to
    arg COMMAND: -l to list directory contents, -g to get file
    arg FILENAME: name of file to receive, if COMMAND == -g
    arg DATA PORT: port on which to set up data connection
    """
    #parse the args and connect to server
    server_address, this_address, command = parse_args()
    control_conn = socket.create_connection(server_address)
    #send command and optional filename
    control_conn.sendall("{}\0".format(command).encode())
    recvd = control_conn.recv(1024).decode()
    #check for an ACK to proceed, otherwise print error message and exit
    if recvd == "ACK":
        #send address info for the data connection and set up connection
        control_conn.sendall("{}|{}\0".format(this_address[0], this_address[1]).encode())
        data_conn = listen_for_data_conn(this_address)[0]
        #get file or directory contents as appropriate
        if command[1] == 'l':
            get_dir_list(data_conn)
        else:
            get_file(data_conn, command[3:])
        data_conn.close()
    else:
        print("{}:{} says {}".format(server_address[0], server_address[1], recvd))
    control_conn.close()
    

if __name__ == "__main__":
    main()

