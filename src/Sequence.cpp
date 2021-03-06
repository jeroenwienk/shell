#include <iostream>
#include <unistd.h>
#include <wait.h>
#include "Sequence.h"
#include "Pipeline.h"
#include "SimpleCommand.h"

Sequence::Sequence() : pathString(getenv("PATH")), homeString(getenv("HOME")),
                       paths(generatePathArray(getenv("PATH"))) {

}

/**
 * Destructor.
 */
Sequence::~Sequence() {
    for (Pipeline *p : pipelines)
        delete p;
}

/**
 * Executes a sequence, i.e. runs all pipelines and - depending if the ampersand
 * was used - waits for execution to be finished or not.
 */
void Sequence::execute() {
    for (Pipeline *p : pipelines) {
        auto firstCommand = p->getCommands().front();

        // if the first and only command is cd execute it
        // if cd is used in a pipe it will do nothing but the other commands will execute
        if (firstCommand->getCommand() == "cd" && p->getCommands().size() == 1) {
            auto arguments = firstCommand->getArguments();
            std::string path = arguments.empty() ? "" : arguments.front();
            firstCommand->changeDirectory(this, &path);
            continue;
        } else if (firstCommand->getCommand() == "exit" && p->getCommands().size() == 1) {
            std::cout << "exiting, goodbye!" << std::endl;
            exit(0);
        }

        // we need to fork here because there might be async pipes so the parent
        // has to be able to continue executing
        int childPid = fork();
        if (childPid == 0) {
            p->execute(this);
            exit(0);
        } else if (childPid < 0) {
            std::cerr << "Failed to create child process" << std::endl;
            exit(EXIT_FAILURE);
        } else {
            int returnValue;
            // only wait if its not async pipe
            if (!p->isAsync()) {
                waitpid(childPid, &returnValue, 0);
            }
        }

    }
}

/**
 * Take the PATH string and convert it to an array of strings
 * @return array of strings containing paths
 */
std::vector<std::string> Sequence::generatePathArray(std::string pathString) {
    // Paths are seperated by :
    const char delimiter = ':';
    std::vector<std::string> result;
    size_t previous = 0;
    size_t index = pathString.find(delimiter);
    while (index != std::string::npos) {
        result.push_back(pathString.substr(previous, index - previous));
        previous = index + 1;
        index = pathString.find(delimiter, previous);
    }
    result.push_back(pathString.substr(previous));
    return result;
}

/**
 * Utility for logging all available paths including the home path
 */
void Sequence::logPaths() {
    std::cout << "Paths:" << std::endl;
    for (const auto &path : paths) {
        std::cout << path << std::endl;
    }
    std::cout << "Home:" << std::endl;
    std::cout << homeString << std::endl;
}

const std::vector<std::string> &Sequence::getPaths() const {
    return paths;
}

const std::string &Sequence::getHomeString() const {
    return homeString;
}

