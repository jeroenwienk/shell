#ifndef SHELL_SEQUENCE_H
#define SHELL_SEQUENCE_H

#include <vector>

class Pipeline;

/**
 * Top-level class for an entered line in our shell.
 * Contains a list of pipelines to execute in order.
 */
class Sequence {
private:
    std::vector<Pipeline *> pipelines;
    std::vector<std::string> paths;
    std::string pathString;
    std::string homeString;

public:
    Sequence();

    ~Sequence();

    std::vector<std::string> generatePathArray(std::string pathString);

    void logPaths();

    void addPipeline(Pipeline *pipeline) {
        pipelines.push_back(pipeline);
    }

    const std::vector<std::string> &getPaths() const;

    void execute();

    const std::string &getHomeString() const;
};


#endif //SHELL_SEQUENCE_H
