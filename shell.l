
/*
 *
 * CS-252 Fall 2013
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */

%{

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "y.tab.h"
#include "command.h"

static  void yyunput (int c,char *buf_ptr  );

void myunputc(int c) {
    unput(c);
}

%}

%%

\n 	{
		return NEWLINE;
	}
	
"\\[^ \t\n]" {
		myunputc(yytext[1]);
	}

[ \t] 	{
		/* Discard spaces and tabs */
     	}	

">" 	{
		return GREAT;
	}

"|" 	{
		return PIPE;
	}
	
"<" 	{
		return LESS;
	}

">&" 	{
		return GREATAMPERSAND;
	}
	
">>" 	{
		return GREATGREAT;
	}

">>&" 	{
		return GREATGREATAMPERSAND;
	}

"&" 	{
		return AMPERSAND;
	}
	
\"[^\n\"]*\" {
		//use strdup(yytext+1) to get a copy of yytext without the first quote (")
 		yylval.string_val = strdup(yytext+1);
 		
 		//set the last quote to \0
 		yylval.string_val[strlen(yylval.string_val) - 1] = '\0';
 		
		//return the word
		return WORD;
	}

\`[^\n\"]*\` {
		//use strdup(yytext+1) to get a copy of yytext without the first backtick (`)
 		char * subshellCommand = strdup(yytext+1);
 		subshellCommand[strlen(subshellCommand) - 1] = '\0';
 		
 		//set the last backtick to \0
 		  
 		
 		//save in/out
		int tmpin = dup(0);
		int tmpout = dup(1);
 		
		//Setup the pipes for communicating with the subshell
		int shellInput[2], shellOutput[2];
		pipe(shellInput);
		pipe(shellOutput);
		
		//write to the shell		
		write(shellInput[1], subshellCommand, strlen(subshellCommand));
		char * check = strdup("\n");
		write(shellInput[1], check, strlen(check));
		
		//redirect stdout to point to the shellOutput pipe
		dup2(shellOutput[1], 1);
		
		//redirect stdIn to read from the shellInput pipe
		dup2(shellInput[0], 0);
		
		//close the stdin of the shellInput pipe
		close(shellInput[1]);	
		
		//pipes are all set up. Time to fork and call the subshell.
		int ret = fork();
		char * shellCommand[2];
		shellCommand[0] = getenv("_");
		shellCommand[1] = NULL;
		if (ret == 0) {
			//child
			execvp(shellCommand[0], shellCommand);
			perror("execvp");
			_exit(1);
		} else if (ret < 0) {
			perror("fork");
			exit(1);
		}
		
		//restore inputs/outputs
		dup2(tmpin, 0);
		dup2(tmpout, 1);
		
		//close tmp fd's
		close(tmpin);
		close(tmpout);
		
		//close shellOutput pipe input
		close(shellOutput[1]);
		
		waitpid(ret, NULL, 0);
		
		//close the input pipe
		close(shellInput[0]);
		
		//read output from subshell
		char * buffer = (char *) malloc(sizeof(char));
		int responseSize = 1024;
		char * response = (char *) malloc(responseSize * sizeof(char));		
		int i = 0;		
		while (0 < read(shellOutput[0], buffer, sizeof(char))) {
			if (i == responseSize) {
				responseSize = responseSize * 2;
				response = (char *) realloc(response, responseSize);
			}
			response[i] = *buffer;
			i++;
		}
		response[i] = '\0';
		
		//reset i and unput char by char
		i = 1;
		responseSize = strlen(response);
		while (i < responseSize) {
			if (response[responseSize - i - 1] == '\n') {
				//remove newlines from the shell response
				myunputc(' ');
			} else {
				myunputc(response[responseSize - i - 1]);
			}
			i++;
		}
		
		//close shell output
		close(shellOutput[0]);
		
		free(response);
		free(buffer);
	}
		
[^ \t\n>&<|][^ \t\n><&|]*  {
		/* Assume that file names have only alpha chars */
		char * tmp = strdup(yytext);
		char * result = strdup(yytext);
		
		int i = 0;
		int j = 0;
		
		while (tmp[i] != '\0') {
			if (tmp[i] == '\\') {
				i++;
				continue;
			}
			
			result[j] = tmp[i];
			j++;
			i++;
		}
		
		result[j] = '\0';
		
		free(tmp);
		
		yylval.string_val = result;
		return WORD;
	}

