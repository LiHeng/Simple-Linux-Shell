#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string>
#include <iostream>

using namespace std;

#define MAX_LINE 4096
#define MAX_WORDS MAX_LINE/2
#define MAX_VAR_LEN 100

/* break line into words separated by whitespace, placing them in the
 array words, and setting the count to nwords */
void tokenize(char *line, char **words, int *nwords);

int find(char*s, char c){
    for (size_t i = 0; i < strlen(s); ++i) {
        if (s[i]==c){
            return 1;
        }
    }
    return 0;
}

/* create a new variable or set the variable's value */
void set_variable(char *name, char* value,char p_variable[MAX_VAR_LEN][2][MAX_LINE],int* len){
    for (int i = 0; i < *len; ++i) {
        if (strcmp(p_variable[i][0],name)==0){
            strcpy(p_variable[i][1],value);
            return;
        }
    }
    strcpy(p_variable[*len][0],name);
    strcpy(p_variable[*len][1],value);
    *len =*len+1;
}

/* get the value of a variable */
char* get_variable(char *name,char p_variable[MAX_VAR_LEN][2][MAX_LINE],int len){
    for (int i = 0; i < len; ++i) {
        if (strcmp(p_variable[i][0],name)==0){
            return p_variable[i][1];
        }
    }
    return NULL;
}

/*realize ' '*/
char *my_strsep(char** s,const char* delimiter){
    char *p = strsep(s,delimiter);
    while (p!=NULL && strcmp(p,"")==0){
        p = strsep(s,delimiter);
    }
    return p;
}

/*copy the string to realize '/ '*/
char *elimit(char *s){
    size_t i=0;
    char *r=strdup(s);
    int index=0;
    while(i<strlen(s)){
        if (s[i]=='\\'){
            if (s[i+1]==' '){
                r[index] = '~';
                i+=2;
            } else{
                r[index]=s[i];
                i+=1;
            }
        } else{
            r[index]=s[i];
            i+=1;
        }
        index+=1;
    }
    r[index]='\0';
    // cout<<r<<endl;
    return r;
}

void replace(char* s){
    for (size_t i = 0; i < strlen(s); ++i) {
        if (s[i]=='~'){
            s[i] = ' ';
        }
    }
}

int main(int argc, char** argv, char** env)
{
    char line[MAX_LINE], *words[MAX_WORDS];
    char current_path[MAX_LINE];
    // all the variables
    char p_variable[MAX_VAR_LEN][2][MAX_LINE];
    // count of variables
    int var_count = 0;
    int nwords=0;
    //
    pid_t pid;
    // exit status of child process
    int status;

    signal(SIGINT,SIG_IGN);

    while(1)
    {
        char tmp_line[MAX_LINE];
        getcwd(current_path,MAX_LINE);
        cout <<"myShell:"<<current_path<<" $ "<<flush;

        /* read a line of text here */
        fgets(line,MAX_LINE,stdin);

        strcpy(line,elimit(line));

        strcpy(tmp_line,line);
        tmp_line[strlen(tmp_line)-1]='\0';

        if(strcmp(tmp_line,"")==0){
            continue;
        }

        /* Check for EOF */
        if(feof(stdin)){
            break;
        }

        //just split the input line by spaces
        tokenize(line,words,&nwords);

        /* built-in commands */
        if(nwords==1&&strcmp(words[0],"exit")==0){
            break;
        }

        if(nwords==2&&strcmp(words[0],"cd")==0){
            if(chdir(words[1])<0){
                cout<<words[1]<<": No such file or directory"<<endl;
            }
            continue;
        }

        if(strcmp(words[0],"set")==0||strcmp(words[0],"export")==0){
            char *p;
            char variable_name[30];
            char *li = tmp_line;
            my_strsep(&li," ");
            p = my_strsep(&li," ");
            strcpy(variable_name,p);
            if(strcmp(words[0],"set")==0){
                set_variable(variable_name,my_strsep(&li,"\0"),p_variable,&var_count);
            } else if (strcmp(words[0],"export")==0){
                char *value = get_variable(variable_name,p_variable,var_count);
                if (value!=NULL){
                    setenv(variable_name,value,1);
                }
            } else{

            }
            continue;
        }

        if(strcmp(words[0],"echo")==0){
            for (int i = 1; i < nwords; ++i) {
                if (words[i][0]=='$'){
                    char *p=words[i]+1;
                    char *value = get_variable(p,p_variable,var_count);
                    if (value!=NULL){
                        printf("%s",value);
                    } else{
                        char *env_value = getenv(p);
                        if (env_value!=NULL){
                            printf("%s",env_value);
                        } else {
                            continue;
                        }
                    }
                } else{
                    printf("%s",words[i]);
                }
                if (i<nwords-1){
                    printf(" ");
                }
            }
            printf("\n");
            continue;
        }

        char program[MAX_LINE];
        // Search PATH environment variable for program if it does not contain a /
        if (!find(words[0],'/')){
            char *path_all;
            int found = 0;
            path_all = getenv("PATH");
            // Make copy of environment path so strsep does not modify original
            char *path_all_tmp = strdup(path_all);
            if (path_all_tmp != NULL){
                char *path;
                path = strsep(&path_all_tmp,":");
                while (path != NULL){
                    char command[MAX_LINE];
                    strcpy(command,path);
                    strcat(command,"/");
                    strcat(command,words[0]);

                    // Check to see if it exists in specified directory
                    if (access(command,0)==0){
                        strcpy(program,command);
                        found = 1;
                        break;
                    }
                    path = strsep(&path_all_tmp,":");
                }
            }
            //free(path_all_tmp);
            if (!found){
                cout<<"Command "<<words[0]<<" not found"<<endl;
                continue;
            }
        } else{   // Otherwise see if exists in absolute path
            if (access(words[0],0)==-1){
                cout<<"Command "<<words[0]<<" not found"<<endl;
                continue;
            }
            strcpy(program,words[0]);
        }

        if((pid=fork())<0){
            perror("fork error");
        } else if (pid==0){     //child process
            //check the input redirection and output redireciton
            signal(SIGINT,SIG_DFL);
            if (nwords>=3){
                if(strcmp(words[nwords-2],"<")==0){
                    int fd=open(words[nwords-1],O_RDONLY);
                    if (fd==-1)
                    {
                        cout<<"file open failed"<<endl;
                        return -1;
                    }
                    close(0);
                    dup2(fd,0);
                    close(fd);
                    //clear the redirection parameters
                    words[nwords-2]=NULL;
                    words[nwords-1]=NULL;
                }else if(strcmp(words[nwords-2],">")==0){
                    int fd=open(words[nwords-1],O_CREAT|O_RDWR,0666); //0666 is the permission
                    if (fd==-1)
                    {
                        cout<<"file open failed"<<endl;
                        return -1;
                    }
                    close(1);
                    dup2(fd,1);
                    close(fd);
                    //clear the redirection parameters
                    words[nwords-2]=NULL;
                    words[nwords-1]=NULL;
                } else if(strcmp(words[nwords-2],"2>")==0){
                    int fd=open(words[nwords-1],O_CREAT|O_RDWR,0666); //0666 is the permission
                    if (fd==-1)
                    {
                        cout<<"file open failed"<<endl;
                        return -1;
                    }
                    close(2);
                    dup2(fd,2);
                    close(fd);
                    //clear the redirection parameters
                    words[nwords-2]=NULL;
                    words[nwords-1]=NULL;
                }
            }
            execvp(program,words);
            cout<<"Command "<<words[0]<<" not found"<<endl;
            exit(1);
        }

        //wait the child process to exit
        if (waitpid(pid,&status,0)!=pid){
            perror("wait error");
        }else{
            if(WIFEXITED(status)){    //child process normal termination
                cout<<"\nProgram exited with status "<<WEXITSTATUS(status)<<endl;
            } else if(WIFSIGNALED(status)){  //child process abnormal termination
                cout<<"\nProgram killed by signal "<<WTERMSIG(status)<<endl;
            }else if(WIFSTOPPED(status)){   //child process was stopped by a signal
                cout<<"\nProgram killed by signal "<<WSTOPSIG(status)<<endl;
            }
        }
    }

    return 0;
}

void tokenize(char *line, char **words, int *nwords)
{
    *nwords=1;

    for(words[0]=my_strsep(&line," \t\r\n");
        (*nwords<MAX_WORDS)&&(words[*nwords]=my_strsep(&line, " \t\r\n"));
        *nwords=*nwords+1
            ){
        replace(words[*nwords]);
    }
    return;
}
