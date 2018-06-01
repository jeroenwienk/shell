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
    char **cStringArguments = getArgumentsAsCStrings(pSequence, &command, &arguments);


    std::cerr << "cStringArguments Was: " << cStringArguments << std::endl;
    int ret = execvp(commandPath.c_str(), cStringArguments);
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

char **SimpleCommand::getArgumentsAsCStrings(Sequence *pSequence, std::string *pCommand,
                                             std::vector<std::string> *pArguments) const {

//    std::cerr << "Command " << pCommand->c_str() << std::endl;
//    std::cerr << "Command " << pSequence->getHomeString() << std::endl;

    std::vector<const char *> cStrings;
    // First insert the command itself;
    cStrings.insert(cStrings.begin(), pCommand->c_str());

    // Loop and add all remaining arguments
    for (const std::string &arg : *pArguments) {

        // if the argument is ~ replace it with the home directory
        if (arg == "~" && !pSequence->getHomeString().empty()) {
            cStrings.push_back(pSequence->getHomeString().c_str());
        } else {
            cStrings.push_back(arg.c_str());
        }
    }

    // Add null because this is required
    cStrings.push_back(NULL);

    auto **cStringArray = const_cast<char **>(&cStrings[0]);
    return cStringArray;
}

const std::string &SimpleCommand::getCommand() const {
    return command;
}

const std::vector<std::string> &SimpleCommand::getArguments() const {
    return arguments;
}
