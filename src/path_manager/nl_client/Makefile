CC=gcc
CFLAGS=-I/usr/include/libnl3 -lnl-genl-3 -lnl-3 -lpthread -Wno-implicit-function-declaration  
TARGET=libnl_netlink_client_cp
OBJS=libnl_netlink_client_cp.o

$(TARGET) : $(OBJS)
	$(CC) -o $@ $(OBJS) $(CFLAGS)

libnl_netlink_client_cp.o : libnl_netlink_client_cp.c
	$(CC) -c libnl_netlink_client_cp.c $(CFLAGS)

clean:
	rm $(OBJS) $(TARGET)
