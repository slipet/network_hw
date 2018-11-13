#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include<unistd.h>
#include<netdb.h>
#define BUFSIZE 8096

void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET)
  {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


struct {
    char *ext;
    char *filetype;
} extensions [] = {
    {"gif", "image/gif" },
    {"jpg", "image/jpeg"},
    {"jpeg","image/jpeg"},
    {"png", "image/png" },
    {"zip", "image/zip" },
    {"gz",  "image/gz"  },
    {"tar", "image/tar" },
    {"htm", "text/html" },
    {"html","text/html" },
    {"exe","text/plain" },
    {0,0} };


int main(int argc, char **argv)
{
    ////////////////////////////
    fd_set main_set;
    fd_set tmp_set;
    FD_ZERO(&main_set);
    FD_ZERO(&tmp_set);
    ///////////////////////////
    int i, pid, listenfd, socketfd;
    socklen_t  length;
    static struct sockaddr_in cli_addr;
    static struct sockaddr_in serv_addr;

    char cwd[1024];
    getcwd(cwd,sizeof(cwd));
    printf("%s\n",cwd);
    /* 使用 /tmp 當網站根目錄 */
    if(chdir(cwd) == -1){
        printf("ERROR: Can't Change to directory %s\n",argv[2]);
        exit(4);
    }


    /* 開啟網路 Socket */
    if ((listenfd=socket(AF_INET, SOCK_STREAM,0))<0)
        exit(3);
    bzero(&serv_addr,sizeof(serv_addr));
    /* 網路連線設定 */
    serv_addr.sin_family = AF_INET;
    /* 使用任何在本機的對外 IP */
    //serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* 使用 80 Port */
    serv_addr.sin_port = htons(80);

    /* 開啟網路監聽器 */
    if (bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr))<0)
        exit(3);

    /* 開始監聽網路 */
    if (listen(listenfd,64)<0)
        exit(3);
    else
        printf("Listening\n");

    FD_SET(listenfd,&main_set);
    int fdmax=listenfd;
    char cliIP[INET_ADDRSTRLEN];
    for(;;)
    {
      char buffer[BUFSIZE+1]  ;
      tmp_set=main_set;
      if(select(fdmax+1,&tmp_set,NULL,NULL,NULL)==-1){
        perror("select");
        exit(4);
      }

      for(int i=0;i<=fdmax;i++)
      {
        if(FD_ISSET(i,&tmp_set))
        {
          if(i == listenfd)
          {
            length = sizeof(cli_addr);
            if ((socketfd = accept(listenfd,(struct sockaddr *)&cli_addr, &length))<0)
              exit(3);
            FD_SET(socketfd,&main_set);
            if(fdmax < socketfd)
              fdmax = socketfd;

            printf("selectserver: new connection from %s on socket %d\n",inet_ntop(cli_addr.sin_family,get_in_addr((struct sockaddr *)&cli_addr),cliIP,INET_ADDRSTRLEN),socketfd);


          }else {
              int fd=i;
              int j, file_fd, buflen, len;
              long k, ret;
              char * fstr;
              //static char buffer[BUFSIZE+1]  ;

              ret = read(fd,buffer,BUFSIZE);   /* 讀取瀏覽器要求 */
              if (ret<=0) {
               /* 網路連線有問題，所以結束行程 */
                  if(ret==0)
                    printf("selectserver socket %d hung up\n",fd);
                  else
                    perror("select");
                close(fd);
                FD_CLR(fd,&main_set);
              }
              /* 程式技巧：在讀取到的字串結尾補空字元，方便後續程式判斷結尾 */
              if (ret>0&&ret<BUFSIZE)
                  buffer[ret] = 0;
              else
                  buffer[0] = 0;

              /* 移除換行字元 */
              for (k=0;k<ret;k++)
                  if (buffer[k]=='\r'||buffer[k]=='\n')
                      buffer[k] = 0;

              /* 只接受 GET 命令要求 */
              if (strncmp(buffer,"GET ",4)&&strncmp(buffer,"get ",4))
              {
                close(fd);
                FD_CLR(fd,&main_set);
              }    

              /* 我們要把 GET /index.html HTTP/1.0 後面的 HTTP/1.0 用空字元隔開 */
              for(k=4;k<BUFSIZE;k++) {

                  if(buffer[k] == ' ') {
                      buffer[k] = 0;
                      break;
                  }
              }

              /* 檔掉回上層目錄的路徑『..』 */
              for (j=0;j<k-1;j++)
                  if (buffer[j]=='.'&&buffer[j+1]=='.')
              {
                close(fd);
                FD_CLR(fd,&main_set);
              }  

              /* 當客戶端要求根目錄時讀取 index.html */
              if (!strncmp(&buffer[0],"GET /\0",6)||!strncmp(&buffer[0],"get /\0",6) )
                  strcpy(buffer,"GET /index.html\0");

              /* 檢查客戶端所要求的檔案格式 */
              buflen = strlen(buffer);
              fstr = (char *)0;

              for(k=0;extensions[k].ext!=0;k++) {
                  len = strlen(extensions[k].ext);
                  if(!strncmp(&buffer[buflen-len], extensions[k].ext, len)) {
                      fstr = extensions[k].filetype;
                      break;
                  }
              }

              /* 檔案格式不支援 */
              if(fstr == 0) {
                  fstr = extensions[i-1].filetype;
              }
              /* 開啟檔案 */
              if((file_fd=open(&buffer[5],O_RDONLY))==-1)
              {
                write(fd, "Failed to open file", 19);
              }
              /* 傳回瀏覽器成功碼 200 和內容的格式 */
              sprintf(buffer,"HTTP/1.0 200 OK\r\nContent-Type: %s\r\n\r\n", fstr);
              write(fd,buffer,strlen(buffer));
              

              /* 讀取檔案內容輸出到客戶端瀏覽器 */
              while ((ret=read(file_fd, buffer, BUFSIZE))>0) {
                  printf("open %ld\n",ret);
                  write(fd,buffer,ret);
              }
                close(fd);
                FD_CLR(fd,&main_set);
          }
        }
      }

    }


}
