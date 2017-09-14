#include <stdio.h>
#include <glob.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define INVALINPUT -5
#define EXECERROR -7
#define SUCCESS 1
#define WRITEERROR -9
#define OUTPUTCANTOPEN -8
#define INPUTCANTOPEN -6
#define INVALOUTPUT -4
#define INVALPIPEMORE -3
#define INVALPIPELESS -2
#define INVALCOM -1
extern char **environ;
char *fullpath(char *exname){
    for(int i = 0;i<strlen(exname);i++)
        if(exname[i] == '/')
            return exname;
    char *path;
    for(int i = 0;environ[i] != NULL;i++){
        /*fprintf(stderr,"%s\n",environ[i]);*/
        if(environ[i][0]=='P'&&environ[i][1]=='A'&&environ[i][2]=='T'&&environ[i][3]=='H'&&environ[i][4]=='='){
            path = (char *)malloc((strlen(exname)+strlen(environ[i]))*sizeof(char));
            int ind = 0;
            glob_t alt;
            for(int j = 5;j<strlen(environ[i]);j++){
                if(environ[i][j] == ':'){
                    path[ind++] = '/';
                    path[ind++] = '\0';
                    ind = 0;
                    strcat(path,exname);
                    /*fprintf(stderr,"after concatinating %s\n",path);*/
                    glob(path,GLOB_MARK,NULL,&alt);
                    if(!alt.gl_pathc)
                        continue;
                    for(int k = 0;k<alt.gl_pathc;k++){
                        int len = strlen(alt.gl_pathv[k]);
                        if(alt.gl_pathv[k][len-1] == '/')
                            continue;
                        return path;
                    }
                }
                else{
                    path[ind++] = environ[i][j];
                }
            }
            return exname;
        }
    }
    return exname;
}
typedef struct st1{
    char *cmdargs[100];
    char *infile[100];//though you can specify multiple input files, only the final one is used
    char *outfile[100];
    int numargs;
    int numinfl;
    int numoutfl;
    int append[100];
}cmd_t;
int Write(int *fds,int n,char *buf,int lim){
    int stat;
    for(int i = 0;i<n;i++){
        stat = write(fds[i],buf,lim);
        if(stat < 0)
            break;
    }
    return stat;
}
void Close(int *fds,int n){
    for(int i = 0;i<n;i++){
        if(fds[i] == STDOUT_FILENO)
            continue;
        close(fds[i]);
    }
}
int* Open(cmd_t *command){
    int n = command->numoutfl;
    int *fds = (int *)malloc(sizeof(int)*(n+1));
    int i = 0;
    for(;i<n;i++){
        if(command->append[i] == 1){
            fds[i] = open(command->outfile[i],O_WRONLY|O_APPEND);
            if(fds[i]<0){
                fprintf(stderr,"the append files does not exists\n");
                return NULL;
            }
        }
        else{
            fds[i] = open(command->outfile[i],O_WRONLY|O_CREAT,0664);
            if(fds[i]<0)
                return NULL;
        }
    }
    return fds;
}
int copyPipe(int p,int *pipe,int *pipe2){
    /*printf("copying pipe\n");*/
    int a,b;
    char buf[100];
    while((a = read(p,buf,100)) > 0){
        /*printf("read %d characters\n",a);*/
        b = write(pipe[1],buf,a);
        /*printf("wrote %d characters in 1\n",b);*/
        b = write(pipe2[1],buf,a);
        /*printf("wrote %d characters in 2\n",b);*/
    }
    if(a<0)
        fprintf(stderr,"error reading pipe during copy\n");
    else{
        close(pipe[1]);
        close(pipe2[1]);
        close(p);
    }
    return a;
}

typedef struct st3{
    char com[1000];
    int status;
}log_t;

int incre(int a){
    a = a+1;
    if(a==10)
        a = 0;
    return a;
}
int decre(int a){
    a = a-1;
    if(a == -1)
        a = 9;
    return a;
}
int increcnt(int a){
    if(a == 10)
        return a;
    a = a+1;
    return a;
}
void prntlastTen(log_t *log,int ct,int pt){
    if(ct == 0){
        printf("no command given by user\n");
        return;
    }
    for(int i = 0;i<ct;i++){
        pt = decre(pt);
        printf("command: %s",log[pt].com);
        printf("status: ");
        switch(log[pt].status){
            case INVALINPUT:
                printf("Invalid input file");
                break;
            case EXECERROR:
                printf("error executing command");
                break;
            case SUCCESS:
                printf("command successful");
                break;
            case WRITEERROR:
                printf("error writing to file");
                break;
            case OUTPUTCANTOPEN:
                printf("error opening output file");
                break;
            case INVALOUTPUT:
                printf("Invalid output file");
                break;
            case INVALPIPEMORE:
                printf("error piping to more commands than specified");
                break;
            case INVALPIPELESS:
                printf("error piping to less commands than specified");
                break;
            case INVALCOM:
                printf("invalid command");
                break;
            default:
                printf("command not completed");
        }
        printf("\n");
    }
}
int caughtint,caughtquit;
void sighand_int(int signo){
    printf("last ten commands executed by user:\n");
    caughtint = 1;
}
void sighand_quit(int signo){
    printf("Do you really want to exit?(y/n)");
    caughtquit = 1;
}
int main(int argc, char **argv){
    caughtquit = caughtint = 0;
    log_t prev[10];
    int ct = 0;
    int pt = 0;
    struct sigaction sas;
    sas.sa_flags = 0;
    sas.sa_handler = sighand_int;
    sigaction(SIGINT,&sas,0);
    sas.sa_handler = sighand_quit;
    sigaction(SIGQUIT,&sas,0);
    char cmdin[1000];
    char cmdToken[100][100];
    int restart = 0,tokenEnd,numberOfCommands;
    int cmdTokenPipeDepth[100] = {0};//this should be all zero, there is some problem here
    char token[100];
    char usrres[10];
    while(1){
        /*printf("at the start of it all\n");*/
        if(caughtint){
            printf("inside caught int\n");
            caughtint = 0;
            prntlastTen(prev,ct,pt);
        }
        if(caughtquit){
            scanf("%s",usrres);
            if(!strcmp(usrres,"y"))
                return 0;
            caughtquit = 0;
        }
    if(fgets(cmdin,100,stdin) == NULL){
        if(errno == EINTR)
            continue;
        break;
    }
    /*printf("received %s\n",cmdin);*/
    strcpy(prev[pt].com,cmdin);
    ct = increcnt(ct);
    prev[pt].status = 0;
    tokenEnd = 0;
    numberOfCommands = 0;
    for(int i = 0;i<strlen(cmdin);){
        if(cmdin[i] == '|' || cmdin[i] == '\n' || cmdin[i] == ','){
            if(tokenEnd == 0){                  //this means that pipe operator is being applied after no command
                fprintf(stderr,"Please enter a valid command\n");
                restart = 1;
                prev[pt].status = INVALCOM;
                pt = incre(pt);
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
            else{
                i += 1;
                cmdTokenPipeDepth[numberOfCommands] = 0;
            }
            numberOfCommands++;
        }
        else
            token[tokenEnd++] = cmdin[i++];
    }
    if(restart){
        restart = 0;
        continue;
    }
    /*for(int i = 0;i<numberOfCommands;i++){*/
        /*printf("%s\n",cmdToken[i]);*/
        /*printf("pipedepth is %d\n",cmdTokenPipeDepth[i]);*/
    /*}*/
    cmd_t commands[numberOfCommands];
    int mode;                                               //this is to tell what currently we are collecting
    for(int i = 0;i<numberOfCommands;i++){
        tokenEnd = 0;
        mode = 0;
        commands[i].numargs = commands[i].numoutfl = commands[i].numinfl = 0;
        for(int j = 0;j<strlen(cmdToken[i]);){
            if(mode == 0){
                if(cmdToken[i][j] == ' ' || cmdToken[i][j] == '\n'|| cmdToken[i][j] == '>' || cmdToken[i][j] == '<'){
                    if(tokenEnd != 0){
                        token[tokenEnd] = '\0';
                        commands[i].cmdargs[commands[i].numargs] = (char*)malloc((1+tokenEnd)*sizeof(char));
                        tokenEnd = 0;
                        strcpy(commands[i].cmdargs[commands[i].numargs++],token);
                    }
                    if(cmdToken[i][j] == '<'){
                        mode = 1;
                    }
                    else if(cmdToken[i][j] == '>'){
                        mode = 2;
                        if(cmdToken[i][j+1] == '>'){//j+1 would exist because \n hasn't been found
                            commands[i].append[commands[i].numoutfl] = 1;
                            j++;
                        }
                        else
                            commands[i].append[commands[i].numoutfl] = 0;
                    }
                    j++;
                }
                else
                    token[tokenEnd++] = cmdToken[i][j++];
            }
            else if(mode == 1){
                if(cmdToken[i][j] == ' '|| cmdToken[i][j] == '<' || cmdToken[i][j] == '>' || cmdToken[i][j] == '\n'){
                    if(tokenEnd!=0){
                        token[tokenEnd] = '\0';
                        commands[i].infile[commands[i].numinfl] = (char*)malloc(sizeof(char)*(1+tokenEnd));
                        tokenEnd = 0;
                        strcpy(commands[i].infile[commands[i].numinfl++],token);
                        mode = 0;
                    }
                    else{
                        if(cmdToken[i][j] == ' '){
                            j++;
                        }
                        else{
                            fprintf(stderr,"please specify the input file in pipe token %d\n",i+1);
                            prev[pt].status = INVALINPUT;
                            pt = incre(pt);
                            restart = 1;
                            break;
                        }
                    }
                }
                else
                    token[tokenEnd++] = cmdToken[i][j++];
            }
            else if(mode == 2){
                if(cmdToken[i][j] == ' '|| cmdToken[i][j] == '<' || cmdToken[i][j] == '>' || cmdToken[i][j] == '\n'){
                    if(tokenEnd!=0){
                        token[tokenEnd] = '\0';
                        commands[i].outfile[commands[i].numoutfl]=(char*)malloc(sizeof(char)*(1+tokenEnd));
                        tokenEnd = 0;
                        strcpy(commands[i].outfile[commands[i].numoutfl++],token);
                        mode = 0;
                    }
                    else{
                        if(cmdToken[i][j] == ' '){
                            j++;
                        }
                        else{
                            fprintf(stderr,"please specify the output file in pipe token %d\n",i+1);
                            prev[pt].status = INVALOUTPUT;
                            pt = incre(pt);
                            restart = 1;
                            break;
                        }
                    }
                }
                else
                    token[tokenEnd++] = cmdToken[i][j++];
            }
        }
        if(restart)
            break;
        commands[i].cmdargs[commands[i].numargs] = NULL;//null terminated string array
    }
    if(restart){
        restart = 0;
        continue;
    }

    /*printf("\nnumber of commands are %d\n",numberOfCommands);*/
    /*for(int i = 0;i<numberOfCommands;i++){*/
        /*printf("the number of arguments for subcommand %s are %d\n",cmdToken[i],commands[i].numargs);*/
        /*for(int j = 0;j<commands[i].numargs;j++){*/
            /*printf("%s\n",commands[i].cmdargs[j]);*/
        /*}*/
        /*printf("infiles are \n");*/
        /*for(int j = 0;j<commands[i].numinfl;j++){*/
            /*printf("%s\n",commands[i].infile[j]);*/
        /*}*/
        /*printf("outfiles are \n");*/
        /*for(int j = 0;j<commands[i].numoutfl;j++){*/
            /*printf("%s\n",commands[i].outfile[j]);*/
        /*}*/
    /*}*/

    /*return 0;*/
    int in,out,status,pp[2],pin = -1,pout = -1,ttl = 0;//ttl = 0 means you must take input from somebody and set the next pipe as well, ttl > 0 means you just take the input from before, ttl<0 means you take input from nobody and also set the pipe
    /*continue;*/
    for(int i = 0;i<numberOfCommands;i++){
        /*printf("in the loop\n");*/
        /*printf("i is %d, numcom is %d\n",i,numberOfCommands);*/
        ttl--;
        pipe(pp);
        out = pp[1];
        if(ttl <= 0){//this means that either you are starting or you are the command just before a pipe
            if(ttl < 0)
                in = STDIN_FILENO;
            else
                in = pin;
            ttl = cmdTokenPipeDepth[i];//the fd in pin now should be closed at the end of fork in parent as it will be no longer required
            /*printf("for ttl <=0 ttl is %d\n",cmdTokenPipeDepth[i]);*/
            pin = pp[0];
            if(ttl == 0){
                if(i!=(numberOfCommands-1)){
                    fprintf(stderr,"you are piping input to more commands than the pipe specifies\n");
                    prev[pt].status = INVALPIPEMORE;
                    pt = incre(pt);
                    restart = 1;
                    break;
                }
            }
        }
        else{
            in = pin;
            if(cmdTokenPipeDepth[i]>0){
                fprintf(stderr,"you are piping output of more than one commands to same input\n");
                prev[pt].status = INVALPIPELESS;
                pt = incre(pt);
                restart = 1;
                break;
            }
            int ptemp1[2],ptemp2[2];
            pipe(ptemp1);pipe(ptemp2);
            copyPipe(in,ptemp1,ptemp2);
            in = ptemp1[0];
            pin = ptemp2[0];
        }
        //check for any input files
        if(commands[i].numinfl){
            //only use the latest input file
            in = open(commands[i].infile[commands[i].numinfl-1],O_RDONLY);
            if(in<0){
                fprintf(stderr,"failed to open input file\n");
                prev[pt].status = INPUTCANTOPEN;
                pt = incre(pt);
                restart = 1;
                break;
            }
        }
        if(!fork()){
            close(pp[0]);
            dup2(out,STDOUT_FILENO);
            if(in!=STDIN_FILENO)
                dup2(in,STDIN_FILENO);
            char *fname = fullpath(commands[i].cmdargs[0]);
            execv(fname,commands[i].cmdargs);
            perror("error executing command:");
            prev[pt].status = EXECERROR;
            pt = incre(pt);
            exit(1);
        }
        close(pp[1]);
        /*if(in != STDIN_FILENO)*/
            /*close(in);*/
        int a,tnum;
        tnum = waitpid(-1,&status,0);//will see what to do with status
        if(tnum <-1)
            if(errno == EINTR)
                continue;
        int *fds;
        char buf[100];
        int pwrt[2];
        pipe(pwrt);
        if(commands[i].numoutfl > 0){
            fds = Open(&(commands[i]));
            if(fds == NULL){
                fprintf(stderr,"error opening files for writing\n");
                prev[pt].status = OUTPUTCANTOPEN;
                pt = incre(pt);
                restart = 1;
                break;
            }
            tnum = commands[i].numoutfl;
            if(cmdTokenPipeDepth[i]>0){
                pipe(pwrt);
                fds[commands[i].numoutfl] = pwrt[1];
                tnum++;
                pin = pwrt[0];
            }
        }
        else if(cmdTokenPipeDepth[i]>0)//we have our own pipe, then we need to duplicate data
        {
            pipe(pwrt);
            fds = (int*)malloc(sizeof(int));
            *fds = pwrt[1];
            tnum = 1;
            pin = pwrt[0];
        }
        else{
            fds = (int*)malloc(sizeof(int));
            *fds = STDOUT_FILENO;
            tnum = 1;
        }
        while((a=read(pp[0],buf,100)) > 0){
            if(Write(fds,tnum,buf,a)<0){
                fprintf(stderr,"error writing\n");
                prev[pt].status = WRITEERROR;
                pt = incre(pt);
                restart = 1;
                break;
            }
        }
        if(restart)
            break;
        Close(fds,tnum);
        //close the open file descriptors in fds, don't close stdin
    }
    if(restart){
        restart = 0;
        continue;
    }
    prev[pt].status = SUCCESS;
    pt = incre(pt);
    /*printf("at the end it all\n");*/
    }
    return 0;
}
