all: server client
server: server.c
	gcc server.c -o server -lrt -lpthread
client: client.c
	gcc client.c -o client -lrt -lpthread
clean:
	rm -fr server client *~ *.o core*

