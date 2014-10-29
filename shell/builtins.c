#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <dirent.h> 
#include "builtins.h"

int echo(char*[]);
int undefined(char *[]);
int endProcess(char *[]);
int lcd(char* []);
int lkill(char* []);
void printError(char* []);
int lls(char* []);

builtin_pair builtins_table[]={
	{"exit",	&endProcess},
	{"lecho",	&echo},
	{"lcd",		&lcd},
	{"lkill",	&lkill},
	{"lls",		&lls},
	{NULL,NULL}
};

int echo( char * argv[]){
	int i =1;
	if (argv[i]) printf("%s", argv[i++]);
	while  (argv[i])
		printf(" %s", argv[i++]);

	printf("\n");
	fflush(stdout);
	return 0;
}

int endProcess(char * argv[]){ // need task about every children.(SIGCHILD) No to leave trash
	exit(1);
}

int lcd(char* argv[]){
	int res;
	if(argv[1] == NULL)
		res = chdir(getenv("HOME"));
	else
		res = chdir(argv[1]);
	if(res == -1){
		printError(argv);
		return BUILTIN_ERROR;
	}
	return 0;
}

int convertToInt(char tab[]){
	int i = 0;
	int sign = 1;
	int dec = 1;
	int res = 0;
	while((48 <= tab[i] && 57 >= tab[i]) || (tab[i] == '-' && i == 0)){ // przerobic na pojedyncze wywolanie funkcji
		if(tab[i] == '-')
			sign *= -1;
		else{
			res += (int)(tab[i] - 48);
			res *= 10; 
		}
		i++;
	}
	res /= 10;
	res *= sign;
	return res;
}

int lkill(char* argv[]){
	int res;
	if(argv[2] == NULL){ // wysylamy sigterm
		res = kill(convertToInt(argv[1]), SIGTERM);
	}
	else{ 
		res = kill(convertToInt(argv[1]), convertToInt(argv[2]));
	}
	if(res == -1){
		printError(argv);
		return BUILTIN_ERROR;
	}
	return 0;
}

void printError(char* argv[]){
	fprintf(stderr, "Builtin '%s' error.\n",
			argv[0]);
	fflush(stderr);
}

void
print_directory(struct dirent * directory) {
	if (directory -> d_name[0] != '.') 
		printf("%s\n", entry->d_name);
}

int lls(char* argv[]){
	char* additionalArgument = argv[1];
	if(additional_argument != NULL)
		return BUILTIN_ERROR;
	
	char current_path[PATH_MAX];
	
	if(getcwd(current_path, PATH_MAX) == NULL)
		return BUILTIN_ERROR;
	
	DIR* files = opendir(current_path);
	
	if(files == NULL)
		return BUILTIN_ERROR;

	struct dirent* directories = readdir(files);
	
	while(directories != NULL){
		directories = print_directory(directories);
	}
	
	closedir(files);
	return 0;
}

int undefined(char * argv[]){
	fprintf(stderr, "Command %s undefined.\n", argv[0]);
	return BUILTIN_ERROR;
}
