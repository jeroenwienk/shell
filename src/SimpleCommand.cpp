#include <iostream>
#include <unistd.h>
#include "SimpleCommand.h"
#include "Sequence.h"

void SimpleCommand::execute(Sequence *pSequence) {
    if (command == "cd") {
        std::cerr << "command cd was used in pipeline this will do nothing" << std::endl;
        exit(0);
    }

    std::string commandPath = findCommand(pSequence);
    std::cout << "CommandPath: " << commandPath << std::endl;

    std::vector<std::string> vector = this->getArguments();
    const char *programname = command.c_str();
    const char **argv = new const char *[vector.size() + 2];
    argv[0] = programname;
    for (int j = 0; j < vector.size() + 1; ++j) {

        argv[j + 1] = vector[j].c_str();
    }

    argv[vector.size() + 1] = NULL;


    int ret = execvp(commandPath.c_str(), (char **) argv);
    std::cerr << "Ret Was: " << commandPath << std::endl;
}

std::string SimpleCommand::findCommand(Sequence *pSequence) {
    // First try to find the command in PATH
    for (auto temp : pSequence->getPaths()) {

        temp = temp.append("/").append(this->command);
        if (access(temp.c_str(), F_OK) == 0)
            return temp;

    }

    // command started with ./ check if it exists
    if (this->command.find("./") == 0 && access(this->command.c_str(), F_OK) == 0) {
        // return the relative command
        return this->command;
    }

    return "";
}

void SimpleCommand::changeDirectory(Sequence *pSequence, std::string *pPath) {

    std::string pathToGoTo;

    if (!pSequence->getHomeString().empty() && (pPath->empty() || pPath->find('~') != std::string::npos)) {

        if (pPath->size() > 1) {
            pathToGoTo = pathToGoTo.append(pSequence->getHomeString());
            pathToGoTo = pathToGoTo.append(pPath->substr(pPath->find('~') + 1));
        } else {
            pathToGoTo = pathToGoTo.append(pSequence->getHomeString());
        }


    } else if (!pPath->empty()) {
        pathToGoTo = pathToGoTo.append(*pPath);
    }

    chdir(pathToGoTo.c_str());
}

const std::string &SimpleCommand::getCommand() const {
    return command;
}

const std::vector<std::string> &SimpleCommand::getArguments() const {
    return arguments;
}
