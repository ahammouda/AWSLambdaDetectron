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

char* UUID(){
    char *generated_pk = (char *) malloc(37*sizeof(char));
    uuid_t uuid;
    uuid_generate(uuid);
    uuid_unparse(uuid, generated_pk);
    // std::cout << generated_pk << std::endl;
    return generated_pk;
}