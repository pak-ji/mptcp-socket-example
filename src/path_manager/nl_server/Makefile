CC=gcc
CFLAGS=-I/usr/include/libnl3 -lnl-genl-3 -lnl-3 -Wno-implicit-function-declaration  
TARGET=libnl_netlink_server
OBJS=libnl_netlink_server.o ../token/token.o

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

../token/token.o : ../token/token.c
	$(CC) $(CFLAGS) -c -o ../token/token.o ../token/token.c

libnl_netlink_server.o : libnl_netlink_server.c
	$(CC) $(CFLAGS) -c libnl_netlink_server.c

clean:
	rm $(OBJS) $(TARGET)
