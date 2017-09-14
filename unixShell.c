#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

typedef struct st1{
    char *cmdargs[100];
    int numargs;
    char infile[100];
    char outfile[100];
    int append;
}cmd_t;

int main(int argc, char **argv){
    char cmdin[1000];
    char cmdToken[100][100];
    int cmdTokenPipeDepth[100] = {0};
    fgets(cmdin,100,stdin);
    char token[100];
    int tokenEnd = 0;
    int numberOfCommands = 0;
    for(int i = 0;i<strlen(cmdin);){
        if(cmdin[i] == '|' || cmdin[i] == '\n' || cmdin[i] == ','){
            if(tokenEnd == 0){                  //this means that pipe operator is being applied after no command
                fprintf(stderr,"Please enter a valid command\n");
                break;
            }
            token[tokenEnd] = '\n';
            token[++tokenEnd] = '\0';
            tokenEnd = 0;
            strcpy(cmdToken[numberOfCommands],token);
            if(cmdin[i] == '|' && cmdin[i+1] == '|' && cmdin[i+2] == '|'){
                i += 3;
                cmdTokenPipeDepth[numberOfCommands] = 3;
            }
            else if(cmdin[i] == '|' && cmdin[i+1] == '|'){
                i += 2;
                cmdTokenPipeDepth[numberOfCommands] = 2;
            }
            else if(cmdin[i] == '|'){
                i += 1;
                cmdTokenPipeDepth[numberOfCommands] = 1;
            }
            else if(cmdin[i] == ','){
                i += 1;
                cmdTokenPipeDepth[numberOfCommands] = 0;
            }
            else
                i++;
            numberOfCommands++;
        }
        else
            token[tokenEnd++] = cmdin[i++];
    }
    /*
     *for(int i = 0;i<numberOfCommands;i++){
     *    printf("%s\n",cmdToken[i]);
     *    printf("pipedepth is %d\n",cmdTokenPipeDepth[i]);
     *}
     */
    cmd_t commands[numberOfCommands];
    int mode;                                               //this is to tell what currently we are collecting
    int indef,outdef;
    for(int i = 0;i<numberOfCommands;i++){
        tokenEnd = 0;
        mode = 0;
        indef = outdef = 0;
        commands[i].numargs = 0;
        strcpy(commands[i].infile,"none");
        strcpy(commands[i].outfile,"none");
        commands[i].append = 0;
        for(int j = 0;j<strlen(cmdToken[i]);){
            if(mode == 0){
                if(cmdToken[i][j] == ' ' || cmdToken[i][j] == '\n'|| cmdToken[i][j] == '>' || cmdToken[i][j] == '<'){
                    if(tokenEnd != 0){
                        token[tokenEnd] = '\0';
                        tokenEnd = 0;
                        commands[i].cmdargs[commands[i].numargs] = (char*)malloc(100*sizeof(char));
                        strcpy(commands[i].cmdargs[commands[i].numargs++],token);
                    }
                    if(cmdToken[i][j] == '<')
                        mode = 1;
                    else if(cmdToken[i][j] == '>'){
                        mode = 2;
                        if(cmdToken[i][j+1] == '>'){//j+1 would exist because \n hasn't been found
                            commands[i].append = 1;
                            j++;
                        }
                    }
                    j++;
                }
                else
                    token[tokenEnd++] = cmdToken[i][j++];
            }
            else if(mode == 1){
                if(cmdToken[i][j] == ' '|| cmdToken[i][j] == '>' || cmdToken[i][j] == '\n'){
                    if(tokenEnd!=0){
                        token[tokenEnd] = '\0';
                        tokenEnd = 0;
                        if(indef){
                            fprintf(stderr,"you can't specify input file more than once\n");
                            break;
                        }
                        strcpy(commands[i].infile,token);
                        indef = 1;
                    }
                    if(cmdToken[i][j] == '>'){
                        mode = 2;
                        if(!indef){
                            fprintf(stderr,"you didn't specify the input file\n");
                            break;
                        }
                        if(cmdToken[i][j+1] == '>'){
                            commands[i].append  = 1;
                            j++;
                        }
                    }
                    j++;
                }
                else
                    token[tokenEnd++] = cmdToken[i][j++];
            }
            else if(mode == 2){
                if(cmdToken[i][j] == ' '|| cmdToken[i][j] == '<' || cmdToken[i][j] == '\n'){
                    if(tokenEnd!=0){
                        token[tokenEnd] = '\0';
                        tokenEnd = 0;
                        if(outdef){
                            fprintf(stderr,"you can't specify output file more than once\n");
                            break;
                        }
                        strcpy(commands[i].outfile,token);
                        outdef = 1;
                    }
                    if(cmdToken[i][j] == '<'){
                        mode = 1;
                        if(!outdef){
                            fprintf(stderr,"you didn't specify the output file\n");
                            break;
                        }
                    }
                    j++;
                }
                else
                    token[tokenEnd++] = cmdToken[i][j++];
            }
        }
        commands[i].cmdargs[commands[i].numargs] = NULL;//null terminated string array
    }


    for(int i = 0;i<numberOfCommands;i++){
        printf("the number of arguments for subcommand %s are %d\n",cmdToken[i],commands[i].numargs);
        for(int j = 0;j<commands[i].numargs;j++){
            printf("%s\n",commands[i].cmdargs[j]);
        }
        printf("%s\n",commands[i].infile);
        printf("%s\n",commands[i].outfile);
        printf("\n");
    }
    printf("number of commands are %d\n",numberOfCommands);

    return 0;
    int prev = -1;
    int pp[2];

    for(int i = 0;i<numberOfCommands;i++){
        printf("inside the loop\n");
        if(i == 0 && numberOfCommands == 1){
            printf("first and last\n");
            if(!fork()){
                if(strcmp(commands[i].infile,"none")){
                    int rfd = open(commands[i].infile,O_RDONLY);
                    if(rfd < 0){
                        fprintf(stderr,"error opening read file: %s\n",strerror(errno));
                        exit(1);
                    }
                    dup2(rfd,STDIN_FILENO);
                    close(rfd);

                }
                if(strcmp(commands[i].outfile,"none")){
                    int wfd;
                    if(commands[i].append)
                        wfd = open(commands[i].outfile,O_WRONLY|O_APPEND);
                    else
                        wfd = open(commands[i].outfile,O_CREAT|O_WRONLY);
                    if(wfd < 0){
                        fprintf(stderr,"error opening write file: %s\n",strerror(errno));
                        exit(1);
                    }
                    dup2(wfd,STDOUT_FILENO);
                    close(wfd);
                }
                //this needs to be converted to execv and path search should be performed by us only
                execvp(commands[i].cmdargs[0],commands[i].cmdargs);
                fprintf(stderr,"error executing command: %s\n",strerror(errno));
                exit(1);
            }
            else
                waitpid(-1,0,0);
        }
        else if(i == 0){
            printf("first only\n");
            printf("command to exe %s\n",commands[i].cmdargs[0]);
            pipe(pp);
            if(!fork()){
                 close(pp[0]);
                dup2(pp[1],STDOUT_FILENO);
                if(strcmp(commands[i].infile,"none")){
                    int rfd = open(commands[i].infile,O_RDONLY);
                    if(rfd < 0){
                        fprintf(stderr,"error opening read file: %s\n",strerror(errno));
                        exit(1);
                    }
                    dup2(rfd,STDIN_FILENO);
                    close(rfd);

                }
                if(strcmp(commands[i].outfile,"none")){
                    int wfd;
                    if(commands[i].append)
                        wfd = open(commands[i].outfile,O_WRONLY|O_APPEND);
                    else
                        wfd = open(commands[i].outfile,O_CREAT|O_WRONLY);
                    if(wfd < 0){
                        fprintf(stderr,"error opening write file: %s\n",strerror(errno));
                        exit(1);
                    }
                    dup2(wfd,STDOUT_FILENO);
                    close(wfd);
                }
                //this needs to be converted to execv and path search should be performed by us only
                execvp(commands[i].cmdargs[0],commands[i].cmdargs);
                fprintf(stderr,"error executing command: %s\n",strerror(errno));
                exit(1);
            }
            else{
                close(pp[1]);
                waitpid(-1,0,0);
                prev = pp[0];
                int temp;
                char buf[100];
                temp = read(prev,buf,100);
                printf("temp is %d \n",temp);
                if(temp == -1)
                    printf("error reading from pipe\n");
                buf[temp] = '\0';
                printf("I read from pipe %s\n",buf);
            }
            /*printf("the first command is executed\n");*/
            /*break;*/
        }
        else if(i == numberOfCommands-1){
            printf("last only\n");
            if(!fork()){
                dup2(prev,STDIN_FILENO);
                if(strcmp(commands[i].infile,"none")){
                    int rfd = open(commands[i].infile,O_RDONLY);
                    if(rfd < 0){
                        fprintf(stderr,"error opening read file: %s\n",strerror(errno));
                        exit(1);
                    }
                    dup2(rfd,STDIN_FILENO);
                    close(rfd);
                }
                if(strcmp(commands[i].outfile,"none")){
                    int wfd;
                    if(commands[i].append)
                        wfd = open(commands[i].outfile,O_WRONLY|O_APPEND);
                    else
                        wfd = open(commands[i].outfile,O_CREAT|O_WRONLY);
                    if(wfd < 0){
                        fprintf(stderr,"error opening write file: %s\n",strerror(errno));
                        exit(1);
                    }
                    dup2(wfd,STDOUT_FILENO);
                    close(wfd);
                }
                //this needs to be converted to execv and path search should be performed by us only
                execvp(commands[i].cmdargs[0],commands[i].cmdargs);
                fprintf(stderr,"error executing command: %s\n",strerror(errno));
                exit(1);
            }
            else{
                waitpid(-1,0,0);
            }
        }
        else{

        }
    }
    return 0;
}
