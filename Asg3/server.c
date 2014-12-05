#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
 
int main()
{
    int status;
    char sessions[10000];
    strcpy(sessions, "");
    //Adding default sessions
    FILE * fp = fopen("pdfs.txt", "r");
    char pdf[100];
    while(fgets(pdf, 100, fp))
    {
        strcat(sessions, pdf);
    }
    fclose(fp);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0)
    {
        perror("Socket Failed\n");
        abort();
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(5000);

    //Bind to a port
    if(bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
      perror("Bind Failed\n");
      abort();
    }

    //Listen for connections
    if(listen(sock, 10) < 0)
    {
      perror("Listen Failed\n");
      abort();
    }

    signal(SIGCLD, SIG_IGN);
  
    while(1)
    {
        struct sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        int client_sock = accept(sock, (struct sockaddr *)&client_addr, &addr_len);//Accept new connection

        //Send available sessions
        write(client_sock, sessions, strlen(sessions));

        //Wait for session chosen
        char pdf_name[100];
        read(client_sock, pdf_name, sizeof(pdf_name));
        
        if(client_sock < 0)
        {
            perror("Accept Failed\n");
            abort();
        }

        FILE * fp = fopen(pdf_name,"r");
        char file[1000000];
        strcpy(file, "");
        fseek(fp, 0L, SEEK_END);
        int size = ftell(fp);
        fseek(fp, 0L, SEEK_SET);
        fclose(fp);

        char file_size[10];
        sprintf(file_size,"%d",size);
        write(client_sock, file_size, strlen(file_size));
        
        fp = fopen(pdf_name,"rb");
        int total = 0;
        while(total<size)
        {
            int read = fread(file,1,1,fp);
            int sent = send(client_sock, file, 1, 0);
            total+=1;
        }        
        fclose(fp);

         
        int clientpid = fork();
        if(clientpid == 0)//Handle client, It has it's own client_sock
        {
            close(sock);//Close it's listen socket

            while(1)
            {
                int cpid = fork();
                if(cpid == 0)//Child to exec bash script
                {
                    char command[100] = "sh";
                    char * arguments[100] = {"sh", "xdo_server.sh", pdf_name, NULL};
                    execvp(command, arguments);
                }
                //Client for this process waits for bash script
                waitpid(cpid, &status, 0);//wait for bash script to execute

                //Parse zoom_page.txt
                fp = fopen("zoom_page.txt", "r");
                FILE * fp1 = fopen("check.txt", "r");
                char state[10000], line[1000], line1[1000], line2[1000], temp[1000];
                int flag = 0;
                strcpy(state, "");
                strcpy(temp, "");
                int count = 1;
                fgets(line1, 1000, fp1);
                fgets(line2, 1000, fp1);

                if(strcmp(line1, line2) != 0)
                    continue;

                while(fgets(line, 1000, fp))//Read entire file
                {
                    strcat(state, line);
                    count++;
                }
                
                //Send state to correct client
                write(client_sock, state, strlen(state));
        
                sleep(2);
            }
            close(client_sock);
        }//Client over
        
    }
    //exit(1); 
 
    return 0;
}