/*
**	This program is a simple shell that only handles
**	commands with multiple arguments, '|' (pipelines), '<', '>', '>>' redirections.
**  Give filename argument to write outputs into backup file.
**	Type "quit" to quit.
*/
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define CMDLEN 80
#define MAX_TOKENS 100

int findToken(char *tokens[], char *t){
	int i=0;
	for ( ; tokens[i]!=NULL; i++){
		if (strcmp(tokens[i], t)==0){
			return i;
		}
	}
	return -1;
}

void runCmd(char **tokens, int inFd, int outFd){

    if (inFd>=0){
        dup2( inFd, STDIN_FILENO);
    }
    if (outFd>=0){
        dup2( outFd, STDOUT_FILENO);
    }

    int inRedirect = findToken(tokens, "<");
	if (inRedirect>=0){
		int inRedFd = open(tokens[inRedirect+1], O_RDONLY);
		if (inRedFd<0){
		    printf("cannot open input redirection file.\n");
		    exit(-1);
		}
		dup2(inRedFd, STDIN_FILENO);
		close(inRedFd);
	}

	int outRedirect = findToken(tokens, ">");
	if (outRedirect>=0){
		int outRedFd = open(tokens[outRedirect+1], O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH ); // reference https://cboard.cprogramming.com/c-programming/95874-open-empty-file.html
		if (outRedFd<0){
            printf("cannot open output redirection file.\n");
            exit(-1);
        }
		dup2(outRedFd, STDOUT_FILENO);
		close(outRedFd);
	}

	int outRedirect2 = findToken(tokens, ">>");
	if (outRedirect2>=0){
		int outRedFd2 = open(tokens[outRedirect2+1], O_CREAT | O_RDWR | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH ); // reference https://ubuntuforums.org/showthread.php?t=1582689
		if (outRedFd2<0){
            printf("cannot open output redirection file.\n");
            exit(-1);
        }
		dup2(outRedFd2, STDOUT_FILENO);
		close(outRedFd2);
	}

	if (inRedirect>=0)
		tokens[inRedirect]=NULL;
	if (outRedirect>=0)
		tokens[outRedirect]=NULL;
	if (outRedirect2>=0)
    	tokens[outRedirect2]=NULL;

    execvp(tokens[0], tokens);
}

int runLine(char **tokens, int tokc){

    int en, st=0;
    int inFd=-1;
    for (en=0; en<tokc; en++){
        if ( en==tokc-1 || strcmp( tokens[en], "|" ) == 0 ){
            int outFd[2]={-1, -1};
            if (en<tokc-1){
                pipe(outFd);
                tokens[en]=NULL;
            }

            int pid=fork();

            if (pid<0){
                printf("error in fork.\n");
                return 0;
            }

            if (pid==0){
                if (en<tokc-1){
                    close(outFd[0]);
                }
                char **cmd=tokens+st;
                runCmd(cmd, inFd, outFd[1]);
            } else {
                close(outFd[1]);
                int status;
                waitpid( pid, &status, 0 );
                inFd=outFd[0];
                if (status!=0){
                    char ch[2];
                    while(inFd>=0 && read(inFd, ch, 1)){
                        printf("%c", ch[0]);
                    }
                    return 0;
                }
                st=en+1;
            }

        }
    }
    return 0;
}

int getTokens(char *tokens[], char *command){
	char *p=strtok(command, " ");
	int tokc=0;
	
	while (p!=NULL){
	    tokens[tokc]=p;
	    tokc++;
	    p = strtok(NULL, " ");
	}
	tokens[tokc]=NULL;
	return tokc;
}

int main(int argc, char **argv)
{
	int pid;
	int status;
	int i, bup=-1;
	char command[CMDLEN];
	char *tokens[MAX_TOKENS+1];
	tokens[100]=NULL;
	_Bool isBup = 0;
	
	if (argc==2){
        bup=open(argv[1],  O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH );
		if (bup<0){
			printf("Cannot open backup file.\n");
		} else {
		    isBup=1;
		}
    }
	
	printf( "Program begins.\n" );

	if (isBup)
		write(bup, "\nProgram begins.\n", 17);
	
	while (1)
	{	
		
		if (isBup){
			lseek(bup, 0, SEEK_END);
			write(bup, "Please enter a command:   ", 26);
		}
		printf( "Please enter a command:   " );
		fgets( command, CMDLEN, stdin );
		command[strlen(command)-1] = '\0';
		if (isBup){
        	write(bup, command, strlen(command));
            write(bup, "\n", 1);
        }

		if ( strcmp(command, "quit") == 0 ){
		    break;
		}

		int tokc = getTokens(tokens, command);
		
		pid = fork();

		if ( pid < 0 )
		{
			printf( "Error in fork.\n" );
			if (isBup)
            		write(bup, "Error in fork.\n", 15);
			exit(-1);
		}

		if ( pid == 0 ) {
		    if ( strcmp(tokens[tokc-1], "&" )==0 ){
		        tokens[tokc-1]=NULL;
		        tokc--;
		    }
			int fd[2];
			pipe(fd); // reference http://tldp.org/LDP/lpg/node11.html
			int childPid = fork();
			
			if (childPid<0){
			    printf( "Error in fork.\n" );
				exit(-1);
			}

			if (childPid==0){
				close(fd[0]);
				dup2(fd[1], STDOUT_FILENO);
                return runLine(tokens, tokc);
			} else {
				close(fd[1]);
				int childStatus;
				waitpid( childPid, &childStatus, 0 );
				dup2(fd[0], STDIN_FILENO);
				char *teeCmds[4]={"tee", "-a", NULL, NULL};
				if (isBup){
				    teeCmds[2]=argv[1];
				}
				execvp( "tee",  teeCmds);	
			}
		} else {	
		    if (strcmp(tokens[tokc-1], "&")==0){
		        continue;
		    }
			waitpid( pid, &status, 0 );
		}
	}
	return 0;
}