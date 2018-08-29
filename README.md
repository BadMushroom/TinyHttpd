为了使TinyHttpd能够在Linux系统顺利编译并运行，这里做了如下修改：

httpd.c 的第33 行
```
void accept_request(int);
```
修改为
```
void *accept_request(void *);
```
httpd.c 的第51行
```
void accept_request(int client)
{
  char buf[1024];
  ...
}
```
修改为
```
void *accept_request(void* tclient)
{
  int client = *(int *)tclient;
  char buf[1024];
  ...
```
httpd.c 的第76行
```
return;
```
修改为
```
return NULL;
```
httpd.c 的第124行，添加
```
return NULL;
```
httpd.c 的第438 行
```
int namelen = sizeof(name);
```
修改为
```
socklen_t namelen = sizeof(name);
```
httpd.c 的第458行
```
int client_name_len = sizeof(client_name);
```
修改为
```
socklen_t client_name_len = sizeof(client_name);
```
httpd.c 的第471行修改为
```
if (pthread_create(&newthread , NULL, accept_request, (void*)&client_sock) != 0)
```
MakeFile中
```
gcc -W -Wall -lsocket -lpthread -o httpd httpd.c
```
修改为
```
gcc -W -Wall -o httpd httpd.c -lpthread
```


####################################################################


  This software is copyright 1999 by J. David Blackstone.  Permission
is granted to redistribute and modify this software under the terms of
the GNU General Public License, available at http://www.gnu.org/ .

  If you use this software or examine the code, I would appreciate
knowing and would be overjoyed to hear about it at
jdavidb@sourceforge.net .

  This software is not production quality.  It comes with no warranty
of any kind, not even an implied warranty of fitness for a particular
purpose.  I am not responsible for the damage that will likely result
if you use this software on your computer system.

  I wrote this webserver for an assignment in my networking class in
1999.  We were told that at a bare minimum the server had to serve
pages, and told that we would get extra credit for doing "extras."
Perl had introduced me to a whole lot of UNIX functionality (I learned
sockets and fork from Perl!), and O'Reilly's lion book on UNIX system
calls plus O'Reilly's books on CGI and writing web clients in Perl got
me thinking and I realized I could make my webserver support CGI with
little trouble.

  Now, if you're a member of the Apache core group, you might not be
impressed.  But my professor was blown over.  Try the color.cgi sample
script and type in "chartreuse."  Made me seem smarter than I am, at
any rate. :)

  Apache it's not.  But I do hope that this program is a good
educational tool for those interested in http/socket programming, as
well as UNIX system calls.  (There's some textbook uses of pipes,
environment variables, forks, and so on.)

  One last thing: if you look at my webserver or (are you out of
mind?!?) use it, I would just be overjoyed to hear about it.  Please
email me.  I probably won't really be releasing major updates, but if
I help you learn something, I'd love to know!

  Happy hacking!

                                   J. David Blackstone
