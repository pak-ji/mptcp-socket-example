CC=gcc
CFLAGS=-I/usr/include/libnl3 -lnl-genl-3 -lnl-3 -lpthread -Wno-implicit-function-declaration  
TARGET=libnl_netlink_client
OBJS=libnl_netlink_client.o ../token/token.o

$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

../token/token.o : ../token/token.c
	$(CC) $(CFLAGS) -c -o ../token/token.o ../token/token.c

libnl_netlink_client.o : libnl_netlink_client.c
	$(CC) $(CFLAGS) -c libnl_netlink_client.c

clean:
	rm $(OBJS) $(TARGET)
