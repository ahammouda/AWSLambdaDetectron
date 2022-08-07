#include "utils.hpp"

// Essentially the python 'split' function
// https://stackoverflow.com/questions/10058606/splitting-a-string-by-a-character
std::vector<std::string> splitString(std::string textString,  char delimeter) {
    std::stringstream textStream(textString);
    std::vector<std::string> seglist;

    std::string segment;
    while(std::getline(textStream, segment, delimeter))
    {
        seglist.push_back(segment);
    }

    return seglist;
}
