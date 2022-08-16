/**
	* Shell
	* Operating Systems
	* v20.08.28
	*/

/**
	Hint: Control-click on a functionname to go to the definition
	Hint: Ctrl-space to auto complete functions and variables
	*/

// function/class definitions you are going to use
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/param.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>

#include <vector>
#include <array>
// although it is good habit, you don't have to type 'std' before many objects by including this line
using namespace std;

struct Command {
	vector<string> parts = {};
};

struct Expression {
	vector<Command> commands;
	string inputFromFile;
	string outputToFile;
	bool background = false;
};

// Parses a string to form a vector of arguments. The seperator is a space char (' ').
vector<string> splitString(const string& str, char delimiter = ' ') {
	vector<string> retval;
	for (size_t pos = 0; pos < str.length(); ) {
		// look for the next space
		size_t found = str.find(delimiter, pos);
		// if no space was found, this is the last word
		if (found == string::npos) {
			retval.push_back(str.substr(pos));
			break;
		}
		// filter out consequetive spaces
		if (found != pos)
			retval.push_back(str.substr(pos, found-pos));
		pos = found+1;
	}
	return retval;
}

// wrapper around the C execvp so it can be called with C++ strings (easier to work with)
// always start with the command itself
// always terminate with a NULL pointer
// DO NOT CHANGE THIS FUNCTION UNDER ANY CIRCUMSTANCE
int execvp(const vector<string>& args) {
	// build argument list
	const char** c_args = new const char*[args.size()+1];
	for (size_t i = 0; i < args.size(); ++i) {
		c_args[i] = args[i].c_str();
	}
	c_args[args.size()] = nullptr;
	// replace current process with new process as specified
	::execvp(c_args[0], const_cast<char**>(c_args));
	// if we got this far, there must be an error
	int retval = errno;
	// in case of failure, clean up memory
	delete[] c_args;
	return retval;
}

// Executes a command with arguments. In case of failure, returns error code.
int executeCommand(const Command& cmd) {
	auto& parts = cmd.parts;
	if (parts.size() == 0)
		return EINVAL;

	// execute external commands
	int retval = execvp(parts);
	return retval;
}

void displayPrompt() {
	char buffer[512];
	char* dir = getcwd(buffer, sizeof(buffer));
	if (dir) {
		cout << "\e[32m" << dir << "\e[39m"; // the strings starting with '\e' are escape codes, that the terminal application interpets in this case as "set color to green"/"set color to default"
	}
	cout << "$ ";
	flush(cout);
}

string requestCommandLine(bool showPrompt) {
	if (showPrompt) {
		displayPrompt();
	}
	string retval;
	getline(cin, retval);
	return retval;
}

// note: For such a simple shell, there is little need for a full blown parser (as in an LL or LR capable parser).
// Here, the user input can be parsed using the following approach.
// First, divide the input into the distinct commands (as they can be chained, separated by `|`).
// Next, these commands are parsed separately. The first command is checked for the `<` operator, and the last command for the `>` operator.
Expression parseCommandLine(string commandLine) {
	Expression expression;
	vector<string> commands = splitString(commandLine, '|');
	for (size_t i = 0; i < commands.size(); ++i) {
		string& line = commands[i];
		vector<string> args = splitString(line, ' ');
		if (i == commands.size() - 1 && args.size() > 1 && args[args.size()-1] == "&") {
			expression.background = true;
			args.resize(args.size()-1);
		}
		if (i == commands.size() - 1 && args.size() > 2 && args[args.size()-2] == ">") {
			expression.outputToFile = args[args.size()-1];
			args.resize(args.size()-2);
		}
		if (i == 0 && args.size() > 2 && args[args.size()-2] == "<") {
			expression.inputFromFile = args[args.size()-1];
			args.resize(args.size()-2);
		}
		expression.commands.push_back({args});
	}
	return expression;
}
int executeExpression(Expression& expression) {
	// Check for empty expression
	string strEmpty = "..";
	if (expression.commands.size() == 0){
		return EINVAL;
	}
	bool inputFile = !(expression.inputFromFile.empty());
	bool outputFile = !(expression.outputToFile.empty());

	// Handle intern commands (like 'cd' and 'exit')
	for(std::vector<int>::size_type i = 0; i < expression.commands.size(); i++){
    	for (std::vector<int>::size_type j = 0; j < expression.commands[i].parts.size(); j++){
            if (expression.commands[i].parts[j] == "exit"){
                //ckecks for an exit command
                printf("Exit successfull!\n");
                exit(0);
            }
            else if (expression.commands[i].parts[j] == "cd"){
                //change the directory acordingly
                int ch = chdir(expression.commands[i].parts[j+1].c_str());
				//and gives an error if the directory is not found.
                if (ch < 0){
                    printf("Error when changing directory\n");
                }
				if(expression.commands[i].parts[j+1] == strEmpty){
					chdir("..");
                }
            }
        }
    }

	// External commands, executed with fork():

	// create a list of 2 file descriptors. Iterate through number of pipes an create pairs with pipe(). Add them to pipesRead and pipesWrite vectors.
	int ends[2];
	std::vector<int> pipesRead;
   	std::vector<int> pipesWrite;
	for (std::vector<int>::size_type i = 0; i < expression.commands.size() - 1; i++) {
		if (pipe(ends) == -1){
        	printf("error when opening a pipe");
         	return 1;
    	} else {
			//printf("LINE 168");
			pipesRead.push_back(ends[0]);
			pipesWrite.push_back(ends[1]);
		}
	}

	//create list of process ids for each command.
	std::vector<pid_t> children;

	pid_t process;
	if (expression.commands.size() >= 2){
			for (std::vector<int>::size_type i = 0; i < expression.commands.size(); i++) {
				process = fork();
				if (process > 0) { //in parent process, add remaining children to children list.
					children.push_back(process);
				} else if (process == 0) {
					if (i == 0) {
						if (inputFile) {
							int input = open(expression.inputFromFile.c_str(), O_RDWR | O_CREAT);
							if (input == -1) {
								fprintf(stderr, "File could not open.");
							}
							else {
								dup2(input, STDIN_FILENO);
								close(input);
							}
						}
						//printf("line 200");
						close(pipesRead[0]);
						dup2(pipesWrite[0], STDOUT_FILENO);
						close(pipesWrite[0]);
						executeCommand(expression.commands[0]);
					}
					else if (i == expression.commands.size() - 1)  {
						if (outputFile) {
							int output = open(expression.outputToFile.c_str(), O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
							if (output == -1) {
								fprintf(stderr, "File could not open.");
							}
							else {
								dup2(output, STDOUT_FILENO);
								close(output);
							}
						}
						close(pipesWrite[i-1]);
						dup2(pipesRead[i-1], STDIN_FILENO);
						close(pipesRead[i-1]);
						executeCommand(expression.commands[i]);
					}
					else {
						
						dup2(pipesRead[i-1], STDIN_FILENO);
						close(pipesRead[i-1]);
						dup2(pipesWrite[i], STDOUT_FILENO);
						close(pipesWrite[i]);
						executeCommand(expression.commands[i]);
					}
				}
			}
	} else {
			if (inputFile) {
				int input = open(expression.inputFromFile.c_str(), O_RDWR | O_CREAT);
				if (input == -1) {
					fprintf(stderr, "File could not open.");
				}
				else {
					dup2(input, STDIN_FILENO);
					close(input);
					}
			}
			if (outputFile) {
				int output = open(expression.outputToFile.c_str(), O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
				if (output == -1) {
					fprintf(stderr, "File could not open.");
				}
				else {
				    dup2(output, STDOUT_FILENO);
					close(output);
				}
			}
			process = fork();
			if (process == 0) {
				executeCommand(expression.commands[0]);
			}
			else {
				waitpid(process, nullptr, 0);
			}
			return 0; //if only one command, execute it and return
	}

	for (std::vector<int>::size_type i = 0; i < expression.commands.size() - 1; i++) {
		close(pipesRead[i]);
		close(pipesWrite[i]);
	}
	if(!expression.background){
			for(std::vector<int>::size_type i = 0; i < expression.commands.size(); i++){
			waitpid(children[i], NULL, 0);
			}
		}
	return 0;
	// Loop over all commandos, and connect the output and input of the forked processes
	//Iterate through children processes. Only redirect output for first child and only iput for last child.

	/*for (std::vector<int>::size_type i = 0; i < children.size() - 1; i++) {
		pid_t processID;
		processID=getpid();
		if (processID == children[i]) {
			//printf("line 210");
			dup2(pipesRead[i], STDIN_FILENO);
			close(pipesRead[i]);
			dup2(pipesWrite[i], STDOUT_FILENO);
			close(pipesWrite[i]);
			waitpid(children[i-1], NULL, 0);
			executeCommand(expression.commands[i]);
		}

	}

	if (getpid() == children[children.size() - 1]) {
		//printf("line 221 last");
		int n = children.size() - 1; // last child
		dup2(pipesRead[n], STDIN_FILENO);
		close(pipesWrite[n]);
		close(pipesWrite[n]);

		waitpid(children[children.size()-2], NULL, 0);
		executeCommand(expression.commands[expression.commands.size()-1]);
	}
	*/
	

	//for(std::vector<int>::size_type i = 0; i < expression.commands.size(); i++){
    //    for (std::vector<int>::size_type j = 0; j < expression.commands[i].parts.size(); j++){
	//		if(pid == 0){
	//			//child process
	//			dup2(fd[1],STDOUT_FILENO);
	//			close(fd[0]);
	//			close(fd[1]);
	//			executeCommand(expression.commands[i]);
	//		}
	//		else {
	//			//parent process
	//			wait(NULL);
	//			dup2(fd[0], STDIN_FILENO);
	//			close(fd[0]);
	//			close(fd[1]);
	//			executeCommand(expression.commands[j]);
	//		}
	//	}
	//}
	// For now, we just execute the first command in the expression. Disable.
	// executeCommand(expression.commands[0]);

}
int normal(bool showPrompt) {
	while (cin.good()) {
		string commandLine = requestCommandLine(showPrompt);
		Expression expression = parseCommandLine(commandLine);
		int rc = executeExpression(expression);
		if (rc != 0)
			cerr << strerror(rc) << endl;
	}
	return 0;
}



// framework for executing "date | tail -c 5" using raw commands
// two processes are created, and connected to each other
int step1(bool showPrompt) {
	// create communication channel shared between the two processes
	// ...
	int fd[2];
	if (pipe(fd) == -1){
		printf("error when opening the pipe");
		return 1;
	}
	pid_t child1 = fork();
	if(child1 < 0){
		printf("Error when forking child 1");
		return 2;
	}
	if (child1 == 0) {
		// redirect standard output (STDOUT_FILENO) to the input of the shared communication channel
		dup2(fd[1],STDOUT_FILENO);
		close(fd[0]);
		close(fd[1]);
		// free non used resources (why?)
		Command cmd = {{string("date")}};
		executeCommand(cmd);
		// display nice warning that the executable could not be found
		if (executeCommand(cmd) == -1){
			printf("Error,executable not found");
			return 3;
		}
		abort(); // if the executable is not found, we should abort. (why?) -- because the pipe will have no input to take in.
	}

	pid_t child2 = fork();
	if (child2 == 0) {
		// redirect the output of the shared communication channel to the standard input (STDIN_FILENO).
		dup2(fd[0], STDIN_FILENO);
		close(fd[0]);
		close(fd[1]);
		// free non used resources (why?) -- if we don't close the pipes after we're done with them the processes won't know when to stop reading/writring
		Command cmd = {{string("tail"), string("-c"), string("5")}};
		executeCommand(cmd);
		abort(); // if the executable is not found, we should abort. (why?)
	}

	// free non used resources (why?)
	//if we don't close the main pipe, the child2 process will not know when to stop reading, because the pipe from the main process is not closed
	close(fd[0]);
	close(fd[1]);
	// wait on child processes to finish (why both?)
	waitpid(child1, nullptr, 0);
	waitpid(child2, nullptr, 0);
	return 0;

}




int shell(bool showPrompt) {


	return normal(showPrompt);


	//return step1(showPrompt);
		if(step1(showPrompt) !=0){
			printf("Step 1 error");
		}
		
	}
