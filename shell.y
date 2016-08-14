/*
 * CS-252
 * shell.y: parser for shell
 *
 * This parser compiles the following grammar:
 *
 *	cmd [arg]* [> filename]
 *
 * you must extend it to understand the complete shell grammar
 *--------------------------------------------------------------------------
 * Randy Pitcher 
 * PUID: 0025254113
 *
 * Extended to include:
 *	cmd [arg]* [ | cmd [arg]* ]* [ [> filename] [< filename] 
 *	[ >& filename] [>> filename] [>>& filename] ]* [&]
 *
 */
%error-verbose
%token	<string_val> WORD

%token 	NOTOKEN GREAT NEWLINE PIPE LESS GREATAMPERSAND GREATGREAT GREATGREATAMPERSAND AMPERSAND

%union	{
		char   *string_val;
	}

%{
//#define yylex yylex
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <dirent.h>
#include "command.h"
void yyerror(const char * s);
int yylex();
void expandWildcardsIfNecessary(char * arg);
void expandWildcards(char * prefix, char * suffix);

%}

%%

start:	
	commands
	| NEWLINE
	;

commands: 
	command
	| commands command 
	;

command: simple_command
	;

simple_command:	
	command_and_args io_modifier_list background_optional NEWLINE {
		//printf("   Yacc: Execute command\n");
		Command::_currentCommand.execute();
	}
	| command_and_args PIPE command_and_args {
		//printf("   Yacc: Piping Commands\n");
	}
	| simple_command PIPE command_and_args {
		//printf("   Yacc: Piping Commands\n");	
	}
	| simple_command io_modifier_list background_optional NEWLINE {
		//printf("   Yacc: Execute command\n");
		Command::_currentCommand.execute();
	}
	| error NEWLINE { 
		yyerrok;
	}
	;

command_and_args:
	command_word argument_list {
		Command::_currentCommand.
		insertSimpleCommand( Command::_currentSimpleCommand );
	}
	;

argument_list:
	argument_list argument
	| /* can be empty */
	;

argument:
	WORD {
		//printf("   Yacc: insert argument \"%s\"\n", $1);
		expandWildcardsIfNecessary($1);
	}
	;

command_word:
	WORD {
		//printf("   Yacc: insert command \"%s\"\n", $1);	       
		Command::_currentSimpleCommand = new SimpleCommand();
		Command::_currentSimpleCommand->insertArgument(strdup($1));
	}
	;
	
io_modifier_list:
	io_modifier_list io_modifier
	| /*Can be empty */
	;

io_modifier:
	GREATGREAT WORD {
		//printf("   Yacc: append output \"%s\"\n", $2);
		if ( Command::_currentCommand._outFile != 0) {
			printf("Ambiguous output redirect\n");
		}
		Command::_currentCommand._outFile = strdup($2);
		Command::_currentCommand._appendToFile = 1;
	}
	| GREAT WORD {
		//printf("   Yacc: insert output \"%s\"\n", $2);
		if ( Command::_currentCommand._outFile != 0) {
			printf("Ambiguous output redirect\n");
		}
		Command::_currentCommand._outFile = strdup($2);
	}
	| GREATGREATAMPERSAND WORD {
		//printf("   Yacc: append stdout and stderr output \"%s\".\n", $2);
		if ( Command::_currentCommand._outFile != 0) {
			printf("Ambiguous output redirect.\n");
		}
		Command::_currentCommand._outFile = strdup($2);
		Command::_currentCommand._errFile = strdup($2);
		Command::_currentCommand._appendToFile = 1;
	}
	| GREATAMPERSAND WORD {
		//printf("   Yacc: insert stdout and stderr output \"%s\"\n", $2);
		if ( Command::_currentCommand._outFile != 0) {
			printf("Ambiguous output redirect\n");
		}
		Command::_currentCommand._outFile = strdup($2);
		Command::_currentCommand._errFile = strdup($2);
	}
	| LESS WORD {
		//printf("   Yacc: insert input \"%s\"\n", $2);
		if ( Command::_currentCommand._inFile != 0) {
			printf("ambiguous input redirections\n");
		}
		Command::_currentCommand._inFile = strdup($2);
	}
	;
	
background_optional:
	AMPERSAND {
		Command::_currentCommand._background = 1;
	}
	| /*can be empty*/
	;

%%

void yyerror(const char * s) {
	fprintf(stderr,"%s\n", s);
	Command::_currentCommand.clear();
	Command::_currentCommand.prompt();
}

//global array variable for sorting arguments
char ** array;
int maxEntries;
int nEntries;

void mySortArray() {
	int comp = 0;
	char * tmp;
	
	for (int i = 0; i < nEntries; i++) {
		for (int j = i+1; j < nEntries; j++) {
			comp = strcmp(array[i], array[j]);
			if(strcmp(array[i], array[j]) > 0) {
				tmp = array[i];
				array[i] = array[j];
				array[j] = tmp;
			}
		}
	}	
}

void expandWildcardsIfNecessary(char * arg) {
	//Return if arg does not contain '*' or '?'
	if (strchr(arg, '*') == NULL && strchr(arg, '?') == NULL) {
		Command::_currentSimpleCommand->insertArgument(strdup(arg));
		return;
	}
	
	//initialize values for the sortable array
	maxEntries = 20;
	nEntries = 0;
	array = (char **) malloc(maxEntries * sizeof(char*));
	
	//make call to recursive function expandWildcards that will handle
	//all wildcard expansion and insert arguemnts into array
	char c[2] = "\0";
	expandWildcards(c, arg);
	
	//sort the array
	mySortArray();
	
	//add array elements in sorted order to the currentSimpleCommand
	for (int i = 0; i < nEntries; i++) {
		Command::_currentSimpleCommand->insertArgument(strdup(array[i]));
	}
	
	//free the array
	free(array);
	array = NULL;
}

void insertIntoSortableArray(char * arg) {
	if (nEntries == maxEntries) {
		maxEntries *= 2;
		array = (char **) realloc(array, maxEntries*sizeof(char*));
	}
	array[nEntries] = strdup(arg);
	nEntries++;
}

#define MAXFILENAME 1024
void expandWildcards(char * prefix, char * suffix) {
	if (suffix[0] == 0) {
		//suffix is empty. put prefix in argument
		insertIntoSortableArray(prefix + 1);
		return;
	}
	
	//obtain the next component in the suffix
	//also advance suffix.
	char * s = strchr(suffix, '/');
	char component[MAXFILENAME];
	if (s!=NULL) {
		strncpy(component, suffix, s - suffix);
		suffix = s+1;
	} else {
		strcpy(component, suffix);
		suffix = suffix + strlen(suffix);
	}
	
	//now we need to expand the componenet
	char newPrefix[MAXFILENAME];
	if (strchr(component, '*') == NULL && strchr(component, '?') == NULL) {
		//component has no wildcards
		sprintf(newPrefix, "%s/%s", prefix, component);
		expandWildcards(newPrefix, suffix);
		return;
	}
	
	//component has wildcards
	//convert component to regex
	char * reg = (char *) malloc(2*strlen(component) + 10);
	char * a = component;
	char * r = reg;
	
	//match beginning of line
	*r = '^';
	r++;
	
	//match content of a
	while (*a) {
		if (*a == '*') { *r='.'; r++; *r='*'; r++; }
		else if (*a == '?') { *r='.'; r++;}
		else if (*a == '.') { *r='\\'; r++; *r='.'; r++;}
		else { *r=*a; r++;}
		a++;
	}
	
	//match end of line
	*r = '$';
	r++;
	*r = '\0';
	
	// 2. compile regex
	char regExpComplete[1024];
	sprintf(regExpComplete, "%s", reg);
	regex_t regex;
	int result = regcomp( &regex, regExpComplete,  REG_EXTENDED|REG_NOSUB);
	
	if (result != 0) {
		perror("Wildcard regex compilation error");
		return;
	}
	
	char * dir;
	//if prefix is empty, then list current directory
	if (strlen(prefix) == 0){
		dir = strdup(".");
	} else {
		dir = prefix;
	}
	
	DIR * d = opendir(dir);
	
	if (d == NULL) {
		return;
	}
	
	struct dirent* ent;
	int maxEntries = 20;
	int nEntries = 0;
	char ** array = (char **) malloc(maxEntries * sizeof(char*));
	
	while ((ent = readdir(d)) != NULL) {
		//check if name matches
		if (regexec(&regex, ent->d_name, 0,NULL, 0) == 0) {
			if (ent->d_name[0] == '.' && component[0] != '.') {
				continue;//don't add dot files unless component starts with a '.'
			}
			
			sprintf(newPrefix, "%s/%s", prefix, ent->d_name);
			expandWildcards(newPrefix, suffix);
		}
	}
	closedir(d);
}

#if 0
main()
{
	yyparse();
}
#endif
