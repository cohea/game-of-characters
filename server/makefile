OBJ=tcp_server.c server_main.c threadpool.c usr_mgt.c
CFLAGS=-lpthread
server:$(OBJ)
	$(CC) $(OBJ) -o $@ $(CFLAGS)
