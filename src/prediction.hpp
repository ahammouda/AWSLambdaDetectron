#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <list>
#include <map>

#include "utils.hpp"

std::list<Prediction> makePredictions(std::string imageFilePath, std::string modelFilePath);