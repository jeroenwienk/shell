#include <iostream>
#include <unistd.h>
#include "SimpleCommand.h"
#include "Sequence.h"
#include <cerrno>

/**
 * Execute this command
 * @param pSequence pointer to the sequence of this command
 */
void SimpleCommand::execute(Sequence *pSequence) {
    // Both cd and pwd are special cases
    // cd is handled in the parent process since changing it in child will have no effect
    // on the parent
    if (command == "cd") {
        std::cerr << "command cd does not work inside a pipeline" << std::endl;
        exit(0);
    } else if (command == "pwd") {
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        std::cout << cwd << std::endl;
        exit(0);
    }

    // e.g /bin/ls
    std::string commandPath = findCommand(pSequence);

    if (commandPath.empty()) {
        std::cerr << command << ": command not found" << std::endl;
        exit(0);
    }

    // the arguments can not be provided to execvp as a vector
    std::vector<std::string> args = this->getArguments();
    const char *commandCString = command.c_str();
    const auto **argv = new const char *[args.size() + 2];
    argv[0] = commandCString;

    // FIXME if an argument is ~ it probably means the home path of the user
    // FIXME so replace ~ with home path
    // FIXME doing this will somehow prevent any output being logged
    // FIXME no clue why

    for (int j = 0; j < args.size() + 1; ++j) {
        argv[j + 1] = args[j].c_str();
    }

    argv[args.size() + 1] = NULL;

    int ret = execvp(commandPath.c_str(), (char **) argv);
}

/**
 * Find the command in either the paths or in a relative directory
 * @param pSequence pointer to the sequence of this command
 * @return if it exists the path to the command else empty string
 */
std::string SimpleCommand::findCommand(Sequence *pSequence) {
    // First try to find the command in PATH
    for (auto temp : pSequence->getPaths()) {

        temp = temp.append("/").append(this->command);
        // On success zero is returned.  On error , -1 is returned and errno is set appropriately.
        if (access(temp.c_str(), F_OK) == 0) {
            return temp;
        }

    }

    // command started with ./ check if it exists
    if (this->command.find("./") == 0 && access(this->command.c_str(), F_OK) == 0) {
        // return the relative command
        return this->command;
    }

    return "";
}

/**
 * Change to the directory provided by path
 * @param pSequence pSequence pointer to the sequence of this command
 * @param pPath the path to change to
 */
void SimpleCommand::changeDirectory(Sequence *pSequence, std::string *pPath) {

    std::string pathToGoTo;

    // If the path is ~ it means the user wants to go their home directory or relative to it
    if (!pSequence->getHomeString().empty() && (pPath->empty() || pPath->find('~') != std::string::npos)) {


        if (pPath->size() > 1) {
            // case for ~/example
            pathToGoTo = pathToGoTo.append(pSequence->getHomeString());
            pathToGoTo = pathToGoTo.append(pPath->substr(pPath->find('~') + 1));
        } else {
            // case for ~
            pathToGoTo = pathToGoTo.append(pSequence->getHomeString());
        }


    } else if (!pPath->empty()) {
        pathToGoTo = pathToGoTo.append(*pPath);
    }

    int returnValue = chdir(pathToGoTo.c_str());
    if (returnValue != 0) {
        switch (errno) {
            case ENOENT:
                std::cerr << "No such file or directory or missing" << std::endl;
                break;
            case EACCES:
                std::cerr << "Permission denied" << std::endl;
                break;
            case ENOTDIR:
                std::cerr << "Path is not a directory" << std::endl;
                break;
            default:
                break;
        }
    }
}

const std::string &SimpleCommand::getCommand() const {
    return command;
}

const std::vector<std::string> &SimpleCommand::getArguments() const {
    return arguments;
}
