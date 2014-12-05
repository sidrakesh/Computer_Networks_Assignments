#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
 
int main()
{
  int status;
  char zoom[10],page[10],x[10],y[10],w[10],h[10],sessions[10000];
  char pdfs[10][1000];
  char* pch;
  
  int n;
  char state[10000];
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if(sock < 0)
  {
    perror("Socket Failed\n");
    abort();
  }

  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr("10.134.11.119");
  serv_addr.sin_port = htons(5000);

  if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    perror("Connect Failed\n");
    abort();
  }

  read(sock, sessions, sizeof(sessions));
  pch = strtok(sessions, "\r\n");
  int k = 1;
  while(pch!=NULL)
  {
    strcpy(pdfs[k++], pch);
    pch = strtok(NULL, "\r\n");
  }

  int i;
  printf("Options available are : \n");
  for(i=1;i<k;++i)
  {
    printf("%d : %s\n", i, pdfs[i]);
  }
  printf("Enter the option chosen : ");
  int choice;
  scanf("%d", &choice);

  write(sock, pdfs[choice], sizeof(pdfs[choice]));

  char file_size[10];
  read(sock, file_size, sizeof(file_size));
  int size = atoi(file_size);

  char file[1000000];

  int sid_ka_pid = fork();
  if(sid_ka_pid == 0)
  {
    FILE* fp = fopen(pdfs[choice],"w");
    int total = 0;
    while(total<size)
    {
      int rec = recv(sock, file, 1, 0);
      int write = fwrite(file,1,1,fp);
        
      total+=1;
    }        
    fclose(fp);
    exit(0);
  }

  waitpid(sid_ka_pid, &status, 0);
  
  int pdfid = fork();

  if(pdfid==0)
  {
    char commands[100] = "okular";
    char * arguments[100] = {"okular", pdfs[choice], NULL};
    execvp(commands,arguments);
  }
  else
  {
    
    int status1;
    printf("Yewsddddddddd\n");
    //waitpid(pdfid, &status1, 0);
    
    while((n = read(sock, state, sizeof(state)-1)) > 0)
    {
      pch = strtok(state," \r\n");

      char* arguments[10];

      arguments[0] = "sh";
      arguments[1] = "xdoclient.sh";

      if(strcmp(pch,"page")==0)
      {
        pch = strtok(NULL," \r\n");
        arguments[2] = pch;

        pch = strtok(NULL," \r\n");
        pch = strtok(NULL," \r\n");
        arguments[3] = pch;
      }
      else
      {
        pch = strtok(NULL," \r\n");
        arguments[3] = pch;

        pch = strtok(NULL," \r\n");
        pch = strtok(NULL," \r\n");
        arguments[2] = pch;
      }

      pch = strtok(NULL," \r\n");
      arguments[4] = pch;
      pch = strtok(NULL," \r\n");
      arguments[5] = pch;
      pch = strtok(NULL," \r\n");
      arguments[6] = pch;
      pch = strtok(NULL," \r\n");
      arguments[7] = pch;

      arguments[8] = "NULL";      
      

      int cpid = fork();
      if(cpid==0)
      {
        char command[10] = "sh";
        execvp(command,arguments);
      }
      waitpid(cpid, &status, 0);//wait for bash script to execute
    }
    printf("bytes read = %d\n", n);
  }
  return 0;
}