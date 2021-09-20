#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#define MAX_LINE 80 /* The maximum length command */


static char * cmd_hist;
char commands[MAX_LINE][MAX_LINE];
const char* COMMAND_PIPE = "|";
const char* COMMAND_IN = "<";
const char* COMMAND_OUT = ">";

static void runNoPipeCmd(const char * line)
{
    char * CMD = strdup(line);
    char *parser[10];
    int arg = 0;
    parser[arg++] = strtok(CMD, " ");

    while(parser[arg-1] != NULL)
    {
        parser[arg++] = strtok(NULL, " ");
    }

    arg--;
    int amp = 0;//"&" count

    if(strcmp(parser[arg-1], "&") == 0)//check if "&" input
    {
        amp = 1;
        parser[--arg] = NULL;
    }

    int fd[2] = {-1, -1};
    while(arg >= 3)
    {
        if(strcmp(parser[arg-2], ">") == 0)
        {
            fd[1] = open(parser[arg-1], O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP|S_IWGRP);
            if(fd[1] == -1)
            {
                perror("open");
                free(CMD);
                return;
            }

            parser[arg-2] = NULL;
            arg -= 2;
        }
        else if(strcmp(parser[arg-2], "<") == 0)
        {
            fd[0] = open(parser[arg-1], O_RDONLY);
            if(fd[0] == -1)
            {
                perror("open");
                free(CMD);
                return;
            }

            parser[arg-2] = NULL;
            arg -= 2;
        }
        else
            break;
    }

    int stat;
    pid_t pid = fork();
    switch(pid)
    {
        case -1:
            perror("fork error");
            break;
        case 0:
            if(fd[0] != -1)
            {
                if(dup2(fd[0], STDIN_FILENO) != STDIN_FILENO)
                {
                    perror("dup2 error");
                    exit(1);
                }
            }

            if(fd[1] != -1)
            {
                if(dup2(fd[1], STDOUT_FILENO) != STDOUT_FILENO)
                {
                    perror("dup2 error");
                    exit(1);
                }
            }
            //create an empty child process in the back
            if(amp ==1 ){
                wait(NULL);
            }
            //for cd command
            else if(strcmp(parser[0],"cd")== 0){
                char * address = parser[1];
                chdir(address);

            }
            else{
                execvp(parser[0], parser);
                perror("execvp");

            }
            exit(0);

        default:
            close(fd[0]);close(fd[1]);
            if(!amp)
                waitpid(pid, &stat, 0);
            break;
    }

    free(CMD);
}

static void saveToHist(const char * cmd)
{
    cmd_hist = strdup(cmd);
}

int splitPipe(char command[MAX_LINE]);
int callCommand(int commandNum);
static void run_from_hist(const char * cmd)
{
    //detect !! command
    if(cmd[1] == '!'){
        if(cmd_hist == NULL)
        {
            printf("No history command found\n");
            return ;
        }
        printf("%s\n", cmd_hist);
        int commandNum = splitPipe(cmd_hist);
        int result = callCommand(commandNum);
        printf("%c", result);
    }
    else{
        printf("invalid command !--,should be !!\n");
    }
}

int splitPipe(char command[MAX_LINE]) {
    int num = 0;
    int i, j;
    int len = strlen(command);

    for (i=0, j=0; i<len; ++i) {
        if (command[i] != ' ') {
            commands[num][j++] = command[i];
        } else {
            if (j != 0) {
                commands[num][j] = '\0';
                ++num;
                j = 0;
            }
        }
    }
    if (j != 0) {
        commands[num][j] = '\0';
        ++num;
    }

    return num;
}

int pipeHelper(int left, int right) {


    /* check for redirecting */
    int inNum = 0, outNum = 0;
    char *inFile = NULL, *outFile = NULL;
    int endIdx = right;

    for (int i=left; i<right; ++i) {
        if (strcmp(commands[i], COMMAND_IN) == 0) { // redirect input
            ++inNum;
            if (i+1 < right){
                inFile = commands[i+1];
            }

            else{
                perror("missing command after >");
            }

            if (endIdx == right) endIdx = i;
        } else if (strcmp(commands[i], COMMAND_OUT) == 0) { // redirect output
            ++outNum;
            if (i+1 < right){
                outFile = commands[i+1];
            }

            else {
                perror("missing command after <");
            }

            if (endIdx == right) {
                endIdx = i;
            }
        }
    }

    if (inNum == 1) {
        FILE* fp = fopen(inFile, "r");
        if (fp == NULL){
            perror("file not exist");
        }


        fclose(fp);
    }

    if (inNum > 1) {
        perror("multiput >");
    }
    else if (outNum > 1) {
        perror("multipul <");
    }

    int result = 0;//RESULT_NORMAL;
    pid_t pid = vfork();
    if (pid == -1) {
        perror("fork error");
    } else if (pid == 0) {

        if (inNum == 1)
            freopen(inFile, "r", stdin);
        if (outNum == 1)
            freopen(outFile, "w", stdout);


        char* comm[MAX_LINE];
        for (int i=left; i<endIdx; ++i)
            comm[i] = commands[i];
        comm[endIdx] = NULL;
        execvp(comm[left], comm+left);
        exit(errno);
    } else {
        int status;
        waitpid(pid, &status, 0);
    }


    return result;
}

int runPipe(int left, int right) {
    if (left >= right) {
        return 0;//RESULT_NORMAL;
    }
    /* detecting pipe */
    int pipeIdx = -1;
    for (int i=left; i<right; ++i) {
        if (strcmp(commands[i], COMMAND_PIPE) == 0) {
            pipeIdx = i;
            break;
        }
    }
    if (pipeIdx == -1) { // no pipe
        return pipeHelper(left, right);
    }
    else if (pipeIdx+1 == right) {
        perror("missing command after pipe");
    }


    int fds[2];
    if (pipe(fds) == -1) {
        perror("pipe error");
    }
    int result = 0;//RESULT_NORMAL;
    pid_t pid = vfork();
    if (pid == -1) {
        perror("fork error");
    } else if (pid == 0) {
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        close(fds[1]);

        result = pipeHelper(left, pipeIdx);
        exit(result);
    } else {
        int status;
        waitpid(pid, &status, 0);
        int exitCode = WEXITSTATUS(status);

        if (exitCode != 0) {//RESULT_NORMAL
            char information[4096] = {0};
            char line[MAX_LINE];
            close(fds[1]);
            dup2(fds[0], STDIN_FILENO);
            close(fds[0]);
            while(fgets(line, MAX_LINE, stdin) != NULL) {
                strcat(information, line);
            }
            printf("%s", information);

            result = exitCode;
        } else if (pipeIdx+1 < right){
            close(fds[1]);
            dup2(fds[0], STDIN_FILENO);
            close(fds[0]);
            result = runPipe(pipeIdx+1, right);
        }
    }

    return result;
}


int callCommand(int commandNum) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork error");
    } else if (pid == 0) {

        int inFds = dup(STDIN_FILENO);
        int outFds = dup(STDOUT_FILENO);

        int result = runPipe(0, commandNum);


        dup2(inFds, STDIN_FILENO);
        dup2(outFds, STDOUT_FILENO);
        exit(result);
    } else {
        int status;
        waitpid(pid, &status, 0);
        return WEXITSTATUS(status);
    }
}

int main(void)
{
    size_t line_size = MAX_LINE;
    char * line = (char*) malloc(sizeof(char)*line_size);
    int isPipe = 0;
    int result;
    int should_run = 1;
    if(line == NULL)
    {
        perror("malloc");
        return 1;
    }

    int in = 0;
    while(should_run)
    {
        if(!in)
            printf("mysh:~$ ");
        if(getline(&line, &line_size, stdin) == -1)
        {
            if(errno == EINTR)
            {
                clearerr(stdin);
                in = 1;
                continue;
            }
            perror("getline");
            break;
        }
        in = 0;
        int line_len = strlen(line);
        if(line_len == 1)
            continue;
        line[line_len-1] = '\0';
        int commandNum = splitPipe(line);
        //check command is a pipe or not
        for (int i=0; i<line_len; ++i) {
            if (strcmp(commands[i], COMMAND_PIPE) == 0) {
                isPipe = 1;
                break;
            }
        }
        if(strcmp(line, "exit") == 0){
            break;
        }

        else if(line[0] == '!'){
            run_from_hist(line);
        }

        else
        {
            saveToHist(line);
            if(isPipe == 1){
                result = callCommand(commandNum);
                printf("%c", result);
                isPipe = 0; //pipe check back to original stat
            }
            else{
                runNoPipeCmd(line);
            }
        }
    }
    free(line);
    return 0;
}