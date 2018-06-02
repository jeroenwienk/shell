#include <iostream>
#include <unistd.h>
#include "SimpleCommand.h"
#include "Sequence.h"
#include <cerrno>
#include <climits>
#include <fcntl.h>
#include <limits>

/**
 * Execute this command
 * @param pSequence pointer to the sequence of this command
 */
void SimpleCommand::execute(Sequence *pSequence) {
    // Both cd and pwd are special cases
    // cd is handled in the parent process since changing it in child will have no effect
    // on the parent

    this->processRedirects();

    if (command == "cd" || command == "exit") {
        std::cerr << "command " << command << " does not work inside a pipeline" << std::endl;
        // clear and ignore for possible broken pipe
        std::cin.clear();
        std::cin.ignore(INT_MAX);
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

    char **formattedArguments = getFormattedArguments(pSequence, &command, &arguments);

    int ret = execvp(commandPath.c_str(), formattedArguments);
}

void SimpleCommand::processRedirects() {
    int fdIn = std::numeric_limits<int>::min();
    int fdOut = std::numeric_limits<int>::min();
    int fdErr = std::numeric_limits<int>::min();

    std::vector<std::string> errors;

    for (const auto &redirect : redirects) {

        if (!redirect.getNewFile().empty() &&
            (redirect.getType() == IORedirect::OUTPUT || redirect.getType() == IORedirect::APPEND)) {
            // We need to output to another filedescriptor

            int flags = redirect.getType() == IORedirect::OUTPUT ? IORedirect::TRUNC_FLAGS : IORedirect::APPEND_FLAGS;
            unsigned long found = redirect.getNewFile().find('&');

            if (redirect.getOldFileDescriptor() == 1) {
                // The old one was stdout

                if (found == 0) {
                    fdOut = stoi(redirect.getNewFile().substr(found + 1));
                } else {
                    fdOut = open(redirect.getNewFile().c_str(), flags, 0644);
                }


            } else if (redirect.getOldFileDescriptor() == 2) {
                // The old one was stderr

                if (found == 0) {
                    fdErr = stoi(redirect.getNewFile().substr(found + 1));
                } else {
                    fdErr = open(redirect.getNewFile().c_str(), flags, 0644);
                }
            }

        }

        if (!redirect.getNewFile().empty() && redirect.getType() == IORedirect::INPUT) {

            fdIn = open(redirect.getNewFile().c_str(), IORedirect::READ_FLAGS, 0644);
            if (fdIn == -1) {
                switch (errno) {
                    case ENOENT:
                        errors.emplace_back("No such file or directory");
                        break;
                    case EACCES:
                        errors.emplace_back("Permission denied");
                        break;
                    case EISDIR:
                        errors.emplace_back("Is a directory");
                        break;
                    default:
                        errors.emplace_back("Could not open file");
                        break;
                }
            }

        }

        if (fdOut == -1) {
            errors.emplace_back("Could not open or create file");
        }
    }

    if (fdIn != std::numeric_limits<int>::min()) dup2(fdIn, 0);
    if (fdOut != std::numeric_limits<int>::min()) dup2(fdOut, 1);
    if (fdErr != std::numeric_limits<int>::min()) dup2(fdErr, 2);

    for (const auto &error : errors) {
        std::cerr << error << std::endl;
    }
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
                std::cerr << "No such file or directory" << std::endl;
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

char **SimpleCommand::getFormattedArguments(Sequence *pSequence, const std::string *command,
                                            const std::vector<std::string> *arguments) const {
    std::vector<const char *> cStrings;

    // First insert the command itself;
    cStrings.insert(cStrings.begin(), command->c_str());

    // Loop and add all remaining arguments
    for (const std::string &arg : *arguments) {

        // if the argument is ~ replace it with the home directory
        if (arg == "~" && !pSequence->getHomeString().empty()) {
            cStrings.push_back(pSequence->getHomeString().c_str());
        } else {
            cStrings.push_back(arg.c_str());
        }
    }

    // Add null because this is required
    cStrings.push_back(NULL);

    auto **ret = const_cast<char **>(&cStrings[0]);
    return ret;
}


const std::string &SimpleCommand::getCommand() const {
    return command;
}

const std::vector<std::string> &SimpleCommand::getArguments() const {
    return arguments;
}


