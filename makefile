all:server server_select




server: server.c
	gcc -o server server.c
server_select: server_select.c
	gcc -o server_select server_select.c


#.PHONY:
clean:
		@rm -f *.o
		@rm -f server
		@rm -f server_select
