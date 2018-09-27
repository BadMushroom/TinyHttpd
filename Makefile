all:httpd


httpd: httpd.c
	gcc httpd.c -lpthread -W -Wall -o httpd

clean: 
	rm httpd
