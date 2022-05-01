CC=gcc
OBJ=client_main.c tcp_client.c
CFLAGS=-g `pkg-config --libs --cflags gtk+-3.0` -export-dynamic -lpthread
LIBS=`pkg-config --libs --cflags gtk+-3.0`
client:$(OBJ)
	$(CC) $(OBJ) -o $@ $(CFLAGS)
