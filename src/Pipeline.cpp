#include <iostream>
#include <unistd.h>
#include <wait.h>
#include "Pipeline.h"
#include "SimpleCommand.h"
#include "Sequence.h"

/**
 * Destructor.
 */
Pipeline::~Pipeline() {
    for (SimpleCommand *cmd : commands)
        delete cmd;
}

/**
 * Executes the commands on this pipeline.
 */
void Pipeline::execute(Sequence *pSequence) {

    std::cout << "executing pipe" << std::endl;

    //pSequence->logPaths();

    unsigned long numOfPipes = commands.size();
    int status;
    int pipes[2 * numOfPipes];

    // first create all the pipes
    for (int i = 0; i < numOfPipes; i++) {
        if (pipe(pipes + i * 2) < 0) {
            std::cerr << "PIPE FAILED" << std::endl;
            exit(EXIT_FAILURE);
        }

    }

    int counter = 0;

    for (std::vector<SimpleCommand>::size_type i = 0; i != commands.size(); i++) {
        int childPid = fork();

        if (childPid == 0) {

            // if not the last command
            if (i < commands.size() - 1) {
                if (dup2(pipes[counter + 1], 1) < 0) {
                    std::cerr << "Failed dup2" << std::endl;
                    exit(EXIT_FAILURE);
                }
            }

            // if not the first command
            if (i != 0) {
                if (dup2(pipes[counter - 2], 0) < 0) {
                    std::cerr << "Failed dup2" << std::endl;
                    exit(EXIT_FAILURE);
                }
            }

            for (int j = 0; j < 2 * numOfPipes; j++) {
                close(pipes[j]);
            }

            commands[i]->execute(pSequence);
            // Should never get here
            exit(EXIT_FAILURE);
        } else if (childPid < 0) {
            std::cerr << "Failed to create child process" << std::endl;
            exit(EXIT_FAILURE);
        }

        counter += 2;
    }

    for (int j = 0; j < 2 * numOfPipes; j++) {
        close(pipes[j]);
    }


    for (int j = 0; j < numOfPipes + 1; j++) {
        wait(&status);
    }

}

const std::vector<SimpleCommand *> &Pipeline::getCommands() const {
    return commands;
}
