
/*
 * CS252: Shell project
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <regex.h>

#include "command.h"

SimpleCommand::SimpleCommand()
{
	// Create available space for 5 arguments
	_numOfAvailableArguments = 5;
	_numOfArguments = 0;
	_arguments = (char **) malloc( _numOfAvailableArguments * sizeof( char * ) );
}

//expands wildcards and environmental variables
char * SimpleCommand::expandEnvVars(char * argument) {
	//start by counting number of '${' in argument. This gives us 
	//the maximum number of possible environmental variables (useful later) and
	//if there are none, we can assume there are no environment variables (efficient)
	int maxPossibleEnvVars = 0;
	char * tmp = argument;
	
	while (true) {
		tmp = strstr(tmp, "${");
		if (tmp != NULL) {
			maxPossibleEnvVars++;
			tmp++;
		} else {
			break;//no more matches found in tmp
		}
	}
	
	if (maxPossibleEnvVars == 0) {
		return strdup(argument);//no env vars to expand. return now
	}
	
	//regex for finding environment variables needing to be expanded
	char expression[1024] = "[$][{][^}][^}]*[}]";
	regex_t regex;	
	int result = regcomp(&regex, expression, REG_EXTENDED);
	
	//check that regcomp worked
	if( result != 0 ) {
		perror("Deadly error occurred in expandArgument.\n");
		printf("errorno: %d\n", result);
		exit( -1 );
	}
	
	
	regmatch_t matches;//stores location of matches
	result = regexec(&regex, argument, (size_t) 1, &matches, 0);
	
	//instantiate values for expanding the argument
	char * expandedArg = strdup(argument);//this will point to the fully expanded arguemnt
	char * varStart;//points to start of the match
	char * varEnd;//points to 1 char after the end of the match
	char * foundVar;//points to a copy of just the variable match between "${" and "}"
	char * freeable;//points to the beginning of the initial space allocated for foundVar
	char * foundValue;//points to the value associated with the environment variable in foundVar.
	size_t nextArgSize;//used to allocate space for the next argument expansion iteration string.
	
	//handle each match bellow
	while (result == 0) {
		varStart = expandedArg + matches.rm_so;
		varEnd = expandedArg + matches.rm_eo;
		
		//get the variable within "${" and "}"
		foundVar = (char *) malloc(sizeof(char) * (varEnd - varStart));
		freeable = foundVar;//will let us free this memory block later.
		strncpy(foundVar, expandedArg + matches.rm_so, matches.rm_eo - matches.rm_so - 1);//get match value
		*(foundVar + (varEnd - varStart) - 1) = '\0';//append null terminator where "}" normally goes
		foundVar += 2;//go past the "${" of the var so foundVar points just to the variable name
		
		//get the value associated with foundVar, if any	
		foundValue = getenv(foundVar);
		//foundValue = NULL means there is no matching env var in the env var list.
		if (foundValue == NULL) {
			printf("No matching value found for environment variable '%s'\n", foundVar);
			//for now, just set the value to the variable name. Maybe this will change in future editions
			foundValue = foundVar;
		}	
		
		//debugging print statements
		//printf("foundvar: %s\n", foundVar);
		//printf("foundValue: %s\n", foundValue);
		
		//allocate space for the newly expanded iteration of expandedArg
		//newSize = oldsize - sizeofVar + sizeofValue + 1(space for '\0')
		nextArgSize = sizeof(char) * (strlen(expandedArg) - (varEnd - varStart) + strlen(foundValue) + 1);
		tmp = expandedArg;//used to free this space when necessary.
		expandedArg = (char *) malloc(nextArgSize);
		strncpy(expandedArg, tmp, matches.rm_so);//copy old expandedArg up to the start of the match ("${")
		strcpy(expandedArg + matches.rm_so, foundValue);//copy the foundValue where the foundVar used to be
		strcpy(expandedArg + matches.rm_so + strlen(foundValue), tmp + matches.rm_eo);//get the rest of old expandedArg
		
		//free space made for foundVar and the previous argument expansion iteration here
		free(freeable);
		freeable = NULL;
		foundVar = NULL;
		free(tmp);
		tmp = NULL;
		
		//debugging print statement
		//printf("expandedArg: %s\n", expandedArg);
		
		result = regexec(&regex, expandedArg, (size_t) 1, &matches, 0);
	}
	
	return expandedArg;
}

void
SimpleCommand::insertArgument( char * argument )
{
	if ( _numOfAvailableArguments == _numOfArguments  + 1 ) {
		// Double the available space
		_numOfAvailableArguments *= 2;
		_arguments = (char **) realloc( _arguments,
				  _numOfAvailableArguments * sizeof( char * ) );
	}
	
	_arguments[ _numOfArguments ] = expandEnvVars(argument);

	// Add NULL argument at the end
	_arguments[ _numOfArguments + 1] = NULL;
	
	_numOfArguments++;
}

Command::Command()
{
	// Create available space for one simple command
	_numOfAvailableSimpleCommands = 1;
	_simpleCommands = (SimpleCommand **)
		malloc( _numOfSimpleCommands * sizeof( SimpleCommand * ) );

	_numOfSimpleCommands = 0;
	_outFile = 0;
	_appendToFile = 0;
	_inFile = 0;
	_errFile = 0;
	_background = 0;
	wasKilled = 0;
}

void
Command::insertSimpleCommand( SimpleCommand * simpleCommand )
{
	if ( _numOfAvailableSimpleCommands == _numOfSimpleCommands ) {
		_numOfAvailableSimpleCommands *= 2;
		_simpleCommands = (SimpleCommand **) realloc( _simpleCommands,
			 _numOfAvailableSimpleCommands * sizeof( SimpleCommand * ) );
	}
	
	_simpleCommands[ _numOfSimpleCommands ] = simpleCommand;
	_numOfSimpleCommands++;
}

void
Command:: clear()
{
	for ( int i = 0; i < _numOfSimpleCommands; i++ ) {
		for ( int j = 0; j < _simpleCommands[ i ]->_numOfArguments; j ++ ) {
			free ( _simpleCommands[ i ]->_arguments[ j ] );
		}
		
		free ( _simpleCommands[ i ]->_arguments );
		free ( _simpleCommands[ i ] );
	}

	if ( _outFile ) {
		free( _outFile );
	}

	if ( _inFile ) {
		free( _inFile );
	}

	if ( _errFile ) {
		free( _errFile );
	}

	_numOfSimpleCommands = 0;
	_outFile = 0;
	_inFile = 0;
	_errFile = 0;
	_background = 0;
	foregroundPID = -5;
}

void
Command::print()
{
	printf("\n\n");
	printf("              COMMAND TABLE                \n");
	printf("\n");
	printf("  #   Simple Commands\n");
	printf("  --- ----------------------------------------------------------\n");
	
	for ( int i = 0; i < _numOfSimpleCommands; i++ ) {
		printf("  %-3d ", i );
		for ( int j = 0; j < _simpleCommands[i]->_numOfArguments; j++ ) {
			printf("\"%s\" \t", _simpleCommands[i]->_arguments[ j ] );
		}
		printf("\n");
	}

	printf( "\n\n" );
	printf( "  Output       Input        Error        Background\n" );
	printf( "  ------------ ------------ ------------ ------------\n" );
	printf( "  %-12s %-12s %-12s %-12s\n", _outFile?_outFile:"default",
		_inFile?_inFile:"default", _errFile?_errFile:"default",
		_background?"YES":"NO");
	printf( "\n\n" );
	
}

void
Command::execute()
{
	// Don't do anything if there are no simple commands
	if ( _numOfSimpleCommands == 0 ) {
		prompt();
		return;
	}
	
	//handle the exit command
	if(strcmp("exit", _simpleCommands[0]->_arguments[0]) == 0) {
		printf("\tDueces ✌️✌️\n");
		exit(1);
	}
	
	//initially set the wasKilled flag to false
	//the sig handler for an interrupt will change this if there is a ctrl-c
	//before the end of execute. This helps to determine wether to clear and prompt
	//within execute or not
	wasKilled = 0;
		
	//save in/out
	int tmpin = dup(0);
	int tmpout = dup(1);
	int tmperr = dup(2);
	
	//set initial input
	int fdin;
	if (_inFile) {
		fdin = open(_inFile, O_RDONLY);
	} else {
		//default input
		fdin = dup(tmpin);
	}
	
	int ret;
	int fdout;
	foregroundPID = -5;
	for (int i = 0; i < _numOfSimpleCommands; i++) {
		//redirect input
		dup2(fdin, 0);
		close(fdin);
		
		//setup output
		if (i == _numOfSimpleCommands - 1) {
			// ----------------- setup stdout -------------------------
			if (_outFile) {
				if(_appendToFile) {
					//open output in append mode
					fdout = open(_outFile, O_CREAT | O_APPEND | O_WRONLY, 0666);
				} else {
					//create file if it doesn't exist, erase it if it does.
					fdout = open(_outFile, O_CREAT | O_TRUNC | O_WRONLY, 0666);
				}
			} else {
				fdout = dup(tmpout);
			}//--------------------------------------------------------
			
			
			//------------------- setup stderr out----------------------
			if (_errFile) {
				//redirect stderr to fdout
				dup2(fdout, 2);
			}//---------------------------------------------------------
			
		//if not the last simple command, handle in the else block
		} else {
			//create pipe
			int fdpipe[2];
			pipe(fdpipe);
			fdout = fdpipe[1];
			fdin = fdpipe[0];
		}
		
		//redirect output
		dup2(fdout, 1);
		close(fdout);
		
		//handle the setenv command here
		if (!strcmp(_simpleCommands[i]->_arguments[0], "setenv")) {
			if(_simpleCommands[i]->_numOfArguments  != 3) {
				//improper usage. 
				printf("invalid setenv input. Usage: setenv varName varValue\n");
				continue;//do nothing. Skip the fork and continue for loop
			}
			
			//name-friendly pointer to the environment variable.
			char * envVar = _simpleCommands[i]->_arguments[1];
			//name-friendly pointer to the variable value
			char * envValue = _simpleCommands[i]->_arguments[2];
			setenv(envVar, envValue, 1);		
			continue;//do not fork. Restart for loop from here
		
		//handle the unsetenv command below
		} else if(!strcmp(_simpleCommands[i]->_arguments[0], "unsetenv")) {
			if(_simpleCommands[i]->_numOfArguments  != 2) {
				//improper usage. 
				printf("invalid unsetenv input. Usage: setenv varName\n");
				continue;//do nothing. Skip the fork and continue for loop
			}
			
			unsetenv(_simpleCommands[i]->_arguments[1]);
			continue;//do not fork. Restart for loop from here
			
		//handle cd
		} else if(!strcmp(_simpleCommands[i]->_arguments[0], "cd")) {
			if(_simpleCommands[i]->_numOfArguments > 2) {
				//improper usage. 
				printf("invalid cd input. Usage: cd directoryPath\n");
				continue;//do nothing. Skip the fork and continue for loop
			}
			
			if (_simpleCommands[i]->_numOfArguments == 1) {
				//cd to home directory
				chdir(getenv("HOME"));
				continue;
			}
			
			//handle an explicitly specified path
			char * path = _simpleCommands[i]->_arguments[1];
			char * freeable = NULL;
			
			if (path[0] != '/') {
				path = (char *) malloc(sizeof(char) * (strlen(path) + 2));
				strcpy(path, "./");
				strcpy(path + 2, _simpleCommands[i]->_arguments[1]);
				freeable = path;				
			}
			
			if (chdir(path)!= 0) {
				//there was an error. For example, no directory found
				perror("cd");
			}
			
			//freeable won't point to null if malloc was called to create path.
			if (freeable != NULL) {
				free(freeable);
				freeable = NULL;
			}
			continue;//do not fork. Restart for loop from here
		}
		
		//create child process.
		ret = fork();
		
		//update latest foregroundPID if not background
		if (!_background) {
			foregroundPID = ret;
		}
		if (ret == 0) {
			//child
			execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);
			perror("execvp");
			_exit(1);
		} else if (ret < 0) {
			perror("fork");
			return;
		}
	}//end of for loop
	
	//restore in/out defaults
	dup2(tmpin, 0);
	dup2(tmpout, 1);
	dup2(tmperr, 2);
	
	if (!_background) {
		//wait for last process
		waitpid(ret, NULL, 0);
	}

	if (wasKilled) {
		//if the process was killed, the sig handler will clear and prompt.
		wasKilled = 0;//reset wasKilled flag
	} else {
		// Clear to prepare for next command
		clear();	
		// Print new prompt
		prompt();
	}
}

// Shell implementation

void
Command::prompt()
{
	if(isatty(0)) {
		printf("myshell>");
		fflush(stdout);
	}
}

void interrupt(int sig) {
	Command::_currentCommand.wasKilled = 1;//flag that the process was killed
	printf("\n");
	Command::_currentCommand.clear();
	Command::_currentCommand.prompt();
}

void killZombies(int sig) {	 
	while(waitpid(-1, NULL, WNOHANG) > 0); 
}

void sendToBackground(int sig) {
	if (Command::_currentCommand.foregroundPID < 1) {
		//do nothing if there is currently no foreground process
		return;
	}
	
	//suspend the foreground process and send to background
	kill(Command::_currentCommand.foregroundPID, SIGTSTP);
	printf("\n");
	Command::_currentCommand.clear();
	Command::_currentCommand.prompt();
}

Command Command::_currentCommand;
SimpleCommand * Command::_currentSimpleCommand;

int yyparse(void);

main() {
	//get the environ string array
	extern char ** environ;
	
	//establish structs for signal handlers
	struct sigaction zombieSA, interruptSA, backgroundSA;
	
	//handle zombies
	zombieSA.sa_handler = killZombies;
	sigemptyset(&zombieSA.sa_mask);
	interruptSA.sa_flags = SA_RESTART;
	
	if (sigaction(SIGCHLD, &zombieSA, NULL)) {
		perror("zombie sigaction error");
		exit(2);
	}
	
	//handle interrupt signal
    interruptSA.sa_handler = interrupt;
    sigemptyset(&interruptSA.sa_mask);
    interruptSA.sa_flags = SA_RESTART;
    
    if(sigaction(SIGINT, &interruptSA, NULL)){
        perror("interrupt sigaction error");
        exit(2);
    }
    
    //handle send to background (CTRL-Z) signal
    backgroundSA.sa_handler = sendToBackground;
    sigemptyset(&backgroundSA.sa_mask);
    backgroundSA.sa_flags = 0;
    
    if(sigaction(SIGTSTP, &backgroundSA, NULL)) {
    	perror("background error");
    	exit(2);
    }
    
	Command::_currentCommand.prompt();
	yyparse();
}

