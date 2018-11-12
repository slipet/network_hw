all:server




server: server.c
	gcc -o server server.c



.PHONY:
	clean:
		@rm -f *.o
		@rm -f server
