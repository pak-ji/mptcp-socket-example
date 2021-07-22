CC=gcc
CFLAGS=-I/usr/include/libnl3 -lnl-genl-3 -lnl-3 -lpthread -Wno-implicit-function-declaration  
TARGET=libnl_netlink_client
OBJS=libnl_netlink_client.o ../token/token.o

$(TARGET) : $(OBJS)
	$(CC) -o $@ $(OBJS) $(CFLAGS)

../token/token.o : ../token/token.c
	$(CC) -c -o ../token/token.o ../token/token.c $(CFLAGS)

libnl_netlink_client.o : libnl_netlink_client.c
	$(CC) -c libnl_netlink_client.c $(CFLAGS)

clean:
	rm $(OBJS) $(TARGET)
