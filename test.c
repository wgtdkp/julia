#define _XOPEN_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <strings.h>

int cmd_quem(void) {
  int result;
  int pipefd[2];
  FILE *cmd_output;
  char buf[1024];
  int status;

  result = pipe(pipefd);
  if (result < 0) {
    perror("pipe");
    exit(-1);
  }

  result = fork();
  if(result < 0) {
    exit(-1);
  }

  if (result == 0) {
    dup2(pipefd[1], STDOUT_FILENO); /* Duplicate writing end to stdout */
    close(pipefd[0]);
    close(pipefd[1]);

    char* argv[] = {"php5-cgi", "/home/wgtdkp/julia/www/index.php", NULL};
    execv("/usr/bin/php5-cgi", argv);
    _exit(1);
  }

  /* Parent process */
  close(pipefd[1]); /* Close writing end of pipe */

  cmd_output = fdopen(pipefd[0], "r");

  if (fgets(buf, sizeof buf, cmd_output)) {
    printf("%s", buf);
  } else {
    printf("No data received.\n");
  }

  wait(&status);
  //printf("Child exit status = %d\n", status);

  return 0;
}


int foo(void)
{
    int fd = open("test.txt", O_RDWR);
    int output[2];
    int input[2];
    pipe(output);
    pipe(input);
    int pid = fork();
    if (pid == 0) {
        //printf("child\n");
        close(output[0]);
        close(input[1]);
        
        //dup2(fd, STDOUT_FILENO);
        dup2(output[1], STDOUT_FILENO);
        dup2(input[0], STDIN_FILENO);
        //putenv("REQUEST_METHOD=POST");
        //putenv("CONTENT_LENGTH=13");
        //char* argv[] = {"php5-cgi", "/home/wgtdkp/julia/www/index.php", NULL};
        //execv("/usr/bin/php5-cgi", argv);
        char* argv[] = {"print", NULL};
        execv("print", argv);
        printf("end of child\n");
        
    } else {
        int status;
        //printf("parent\n");
        close(output[1]);
        close(input[0]);
        char ch = 0;
        for (int i = 0; i < 5; i++) {
            int err = scanf("%c", &ch);
            write(input[1], &ch, 1);
        }
        //printf("%c", ch);
        //int fd = open("test.txt", O_RDWR);
        while (1 == read(output[0], &ch, 1))
            printf("%c", ch);
        //close(fd);
        waitpid(pid, &status, 0);
        close(fd);
    }
    return 0;
}

int main(void)
{
    //foo();
    //cmd_quem();
    char buf[255];
    putenv("GATEWAY_INTERFACE=CGI/1.1");
    //putenv("REQUEST_METHOD=GET");
    //putenv("QUERY_STRING=user=kangping");
    putenv("REQUEST_METHOD=POST");
    putenv("CONTENT_LENGTH=13");
    putenv("REDIRECT_STATUS=200");
    putenv("CONTENT_TYPE=application/x-www-form-urlencoded");
    sprintf(buf, "SCRIPT_FILENAME=/home/wgtdkp/julia/www/index.php");
    //putenv("SCRIPT_FILENAME=/home/wgtdkp/julia/www/index.php");
    putenv(buf);
    char* argv[] = {NULL};
    execv("/usr/bin/php5-cgi", argv);


    return 0;
}