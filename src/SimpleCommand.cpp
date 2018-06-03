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

    // first set up the proper redirects
    this->processRedirects(pSequence);

    // Both cd and pwd are special cases
    // cd is handled in the parent process since changing it in child will have no effect
    // on the parent
    if (command == "cd" || command == "exit") {
        std::cerr << "command " << command << " does not work inside a pipeline" << std::endl;
        exit(0);
    } else if (command == "pwd") {
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        std::cout << cwd << std::endl;
        exit(0);
    } else if (command == "history") {
        char *argv[] = {"cat", "/var/tmp/history.txt", NULL};
        int ret = execvp("/bin/cat", argv);
        exit(EXIT_FAILURE);
    } else if (command == "lastcommand") {
        char *argv[] = {"tail", "-n", "1", "/var/tmp/history.txt", NULL};
        int ret = execvp("/usr/bin/tail", argv);
        exit(EXIT_FAILURE);
    }

    // e.g /bin/ls
    std::string commandPath = findCommand(pSequence);

    // command was not found in any of the paths or in the current directory (on ./cmd)
    if (commandPath.empty()) {
        std::cerr << command << ": command not found" << std::endl;
        exit(0);
    }

    // the arguments can not be provided to execvp as a vector
    std::vector<std::string> args = this->getArguments();

    // we need +2 for the command an NULL
    char *argv[this->getArguments().size() + 2];
    argv[0] = const_cast<char *>(command.c_str());

    // this little statement took about 4 hours of debugging!!!!
    // statement was args.size() + 1 and caused no errors :(
    for (int j = 0; j < args.size(); ++j) {
        if (args[j] == "~" && !pSequence->getHomeString().empty()) {
            // ~ represents user home
            argv[j + 1] = const_cast<char *>(pSequence->getHomeString().c_str());
        } else {
            argv[j + 1] = const_cast<char *>(args[j].c_str());
        }

    }

    // and last add null to the args
    argv[args.size() + 1] = NULL;

    int ret = execvp(commandPath.c_str(), argv);
    exit(EXIT_FAILURE);
}

/**
 * Sets up all the necessary redirects
 * @param pSequence pointer to the sequence of this command
 */
void SimpleCommand::processRedirects(Sequence *pSequence) {
    int fdIn = std::numeric_limits<int>::min();
    int fdOut = std::numeric_limits<int>::min();
    int fdErr = std::numeric_limits<int>::min();

    std::vector<std::string> errors;

    for (const auto &redirect : redirects) {

        std::string newFile = redirect.getNewFile();
        unsigned long tildeIndex = newFile.find('~');

        // e.g. ~/Documents/input.txt
        if (tildeIndex == 0) {
            // remove ~ character
            newFile.erase(0, 1);
            // insert the home path at the begin
            newFile.insert(0, pSequence->getHomeString());
        }

        if (!newFile.empty() &&
            (redirect.getType() == IORedirect::OUTPUT || redirect.getType() == IORedirect::APPEND)) {
            // We need to output to another filedescriptor

            // set up append or overwrite flags
            int flags = redirect.getType() == IORedirect::OUTPUT ? IORedirect::TRUNC_FLAGS : IORedirect::APPEND_FLAGS;
            unsigned long found = newFile.find('&');

            if (redirect.getOldFileDescriptor() == 1) {
                // The old one was stdout

                if (found == 0) {
                    // get the new one after &
                    fdOut = stoi(newFile.substr(found + 1));
                } else {
                    fdOut = open(newFile.c_str(), flags, 0644);
                }

                if (fdOut == -1) {
                    checkForErrno(&errors);
                }


            } else if (redirect.getOldFileDescriptor() == 2) {
                // The old one was stderr

                if (found == 0) {
                    // get the new one after &
                    fdErr = stoi(newFile.substr(found + 1));
                } else {
                    fdErr = open(newFile.c_str(), flags, 0644);
                }

                if (fdErr == -1) {
                    checkForErrno(&errors);
                }
            }

        }

        if (!newFile.empty() && redirect.getType() == IORedirect::INPUT) {
            fdIn = open(newFile.c_str(), IORedirect::READ_FLAGS, 0644);
            if (fdIn == -1) {
                checkForErrno(&errors);
            }

        }

    }

    if (fdIn != std::numeric_limits<int>::min() && fdIn != -1) dup2(fdIn, 0);
    if (fdOut != std::numeric_limits<int>::min() && fdOut != -1) dup2(fdOut, 1);
    if (fdErr != std::numeric_limits<int>::min() && fdErr != -1) dup2(fdErr, 2);

    for (const auto &error : errors) {
        std::cerr << error << std::endl;
    }

    if (!errors.empty()) {
        exit(EXIT_FAILURE);
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

void SimpleCommand::checkForErrno(std::vector<std::string> *errors) {
    switch (errno) {
        case ENOENT:
            errors->emplace_back("No such file or directory");
            break;
        case EACCES:
            errors->emplace_back("Permission denied");
            break;
        case EISDIR:
            errors->emplace_back("Is a directory");
            break;
        case ENOTDIR:
            errors->emplace_back("Path is not a directory");
            break;
        default:
            errors->emplace_back("Could not open file");
            break;
    }
}

const std::string &SimpleCommand::getCommand() const {
    return command;
}

const std::vector<std::string> &SimpleCommand::getArguments() const {
    return arguments;
}


