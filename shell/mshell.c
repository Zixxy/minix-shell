#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include "config.h"
#include "siparse.h"
#include "utils.h"
#include "sys/stat.h"
#include "builtins.h"
typedef int bool;


#define true 1
#define false 0

#define DEBUG_DEFINE 8

void execute_command(command* command){
	if(command == NULL)
		exit(1);
	int res = execvp(command -> argv[0], command -> argv);
	if(errno == ENOENT)
		fprintf(stderr, "%s: no such file or directory\n",
			command -> argv[0]);
	else if(errno == EACCES)
		fprintf(stderr, "%s: permission denied\n",
			command -> argv[0]);
	else
		fprintf(stderr, "%s: exec error\n",
			command -> argv[0]);
	fflush(NULL);
	exit(EXEC_FAILURE);
}

command* prepare_command(int length, char* bufor){
	char* currentline;
	if(length == 0)
			exit(1);
	
	if(length == MAX_LINE_LENGTH+1){
		fprintf(stderr, "Syntax error.\n");
		char cur = {0};
		while(cur != '\n'){
			char a[1] = {0};
			read(0, a, 1);
			cur = a[0];
		}
		return NULL;
	}
	
	line * line = parseline(bufor);
	command* command = pickfirstcommand(line);
	if(command == NULL 
		|| command -> argv == NULL
		|| command -> argv[0] == NULL)
			return NULL;
	return command;
}

void buffor_copy(char* tab1, char* tab2, int len1){
	if(tab1 == NULL || len1 == 0)
		return;
	for(int i = 0; i < len1; ++i)
		tab2[i] = tab1[i];
}

void clear_bufor(char* bufor, int length){
	for(int i = 0; i < length; ++i)
		bufor[i] = 0;
}
void read_and_order_executing(bool terminal_mode){
	char* bufor = (char*) calloc(2 * MAX_LINE_LENGTH + 2, sizeof(char));
	char* first_part = NULL;

	int first_part_length = 0;

	while(true){
		
		if(terminal_mode){
			printf(PROMPT_STR);
			fflush(NULL);
		}

		int length = read(0, bufor+first_part_length, 
			MAX_LINE_LENGTH + 1 - first_part_length);

		if(length == 0){
			realloc(bufor, ((2 * MAX_LINE_LENGTH+2)*sizeof(char)));
			exit(0);
		}

		int prev = 0;
		length += first_part_length;
		for(int i = 0; i < length; ++i){		
			if(bufor[i] == '\n' || bufor[i] == 0){
				bufor[i] = 0;
				fflush(stdout);
				if(i == prev){
					++prev;
					continue;
				}
				command* command = prepare_command(i - prev, bufor + prev);

				int u = 0;
				bool executed = false;
				while(1){
					if(builtins_table[u].name == NULL)
						break;
					//printf("%s \n",*(command -> argv));
					//printf("%s \n", builtins_table[u].name);
					if(strcmp(builtins_table[u].name, *(command -> argv)) == 0){
						//printcommand(command, 1);
						builtins_table[u].fun(command -> argv);
						executed = true;
						break;
					}

					fflush(stdout);
					++u;
				}
				if(!executed){
					int child_pid = fork();
					if(child_pid == -1){
						printf(" fork didnt work\n");
						fflush(NULL);
					}
					else if(child_pid == 0){
						execute_command(command);
					}
					else{
						int status = waitpid(child_pid, NULL, 0);
						if(status == -1){
							printf("shell process couldnt do waitpid() for executor.");
							fflush(NULL);
						}
					}
				}
				
				prev = i+1;
			}
		}

		if(prev == 0 && length == MAX_LINE_LENGTH+1){
			//printf("WAIT");
			//fflush(stdout);
			fprintf(stderr, "Syntax error.\n");
			char cur = {0};
			while(cur != '\n'){
				char a[1] = {0};
				read(0, a, 1);
				cur = a[0];
			}
			first_part_length = 0;
			clear_bufor(bufor, 2*MAX_LINE_LENGTH+2);
			continue;
		}

		if(prev != MAX_LINE_LENGTH+1){
			
			char* new_bufor = (char*) calloc(2 * MAX_LINE_LENGTH+2, sizeof(char));
			first_part_length = length - prev;
			
			buffor_copy(bufor+prev, new_bufor, first_part_length);
			realloc(bufor, ((2 * MAX_LINE_LENGTH+2)*sizeof(char)));
			bufor = new_bufor;
			clear_bufor(bufor+first_part_length+1, 
			2 * MAX_LINE_LENGTH + 2 - first_part_length);
		}
		else{
			first_part_length = 0;
			clear_bufor(bufor, 2 * MAX_LINE_LENGTH+2);
		}

	}
}



int	main(int argc, char *argv[])
{
	bool terminal_mode = true;

	struct stat status;
	fstat(0, &status);
		
	if(S_ISCHR(status.st_mode) == 0){
		terminal_mode = false;
	}
	
	read_and_order_executing(terminal_mode);
}
