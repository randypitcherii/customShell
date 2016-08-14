
#ifndef command_h
#define command_h

// Command Data Structure
struct SimpleCommand {
	// Available space for arguments currently preallocated
	int _numOfAvailableArguments;

	// Number of arguments
	int _numOfArguments;
	char ** _arguments;
	
	SimpleCommand();
	void insertArgument( char * argument );
	char * expandEnvVars(char * argument);
};

struct Command {
	int _numOfAvailableSimpleCommands;
	int _numOfSimpleCommands;
	SimpleCommand ** _simpleCommands;
	char * _outFile;
	char * _inFile;
	char * _errFile;
	int _background;
	int _appendToFile;
	int wasKilled;//used to flag when a process was interrupted with ctrl-c
	int foregroundPID;

	void prompt();
	void print();
	void execute();
	void clear();
	
	//signal handlers
	void interrupt(int sig);
	void killZombies(int sig);
	void sendToBackground(int sig);
	
	Command();
	void insertSimpleCommand( SimpleCommand * simpleCommand );

	static Command _currentCommand;
	static SimpleCommand *_currentSimpleCommand;
};

#endif
