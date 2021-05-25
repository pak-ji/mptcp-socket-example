## MPTCP Socket API test app
This is echo and file send program between server and client.<br>
To run this, the Linux kernel must first have MPTCP built into it.<br>
Refer to ref 1) below to build MPTCP in the Linux kernel.<br>
And I used `tcpdump` for packet capture. <br>
And I used `ifstat` for NIC statistics.

* ref 1) : www.multipath-tcp.org
* ref 2) : https://irtf.org/anrw/2016/anrw16-final16.pdf

<br>

## Environment
* &geq; Linux Kernel 4.4.x
* &geq; MPTCP v0.92.x

<br>

## How to installations?
##### in server
	gcc -o server server.c
##### in client
	gcc -o client client.c

<br>

## How to run?
##### in server (do first, echo & file & p_scheduler)
	./server [port number]
##### in client (echo)
	./client [server address] [port number]
##### in client (file & p_scheduler)
	./client [server address] [port number] [file path]

<br>

## Precautions for Testing
It does not work as an MPTCP stack when proceeding to the loopback address("localhost", "127.0.0.1"). <br>
Make sure you send it to physical address.
