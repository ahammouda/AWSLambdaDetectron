#ifndef UTILS_HPP
#define UTILS_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <list>
#include <map>
#include <cmath>
#include <string>
#include <vector>

// Conversion struct from torch tensor classification output to human readable
// (Defenitions where created in the python MetaDataCatalouge map):
//      node,text,edge,arrowhead <=> 0,1,2,3
const static std::map<int,std::string> CLASSES {
  {0,"node"},
  {1,"text"},
  {2,"edge"},
  {3,"arrowhead"},
};

// Ultimately storing a list of these items:
//[{
//   bbox
//   class
//   score
//   imageId // to be used for naming later on the cropped piece of this segmentation
// }]
struct Prediction
{
    float score;
    float boundingBox[4];
    std::string classification;
    // optional field for naming the the cropped and saved image
    // to be referenced by this bounding box (if there is one)
    std::string imageId;
};


void dedupPredictions(std::list<Prediction> &predictions, float tolerance);

std::vector<std::string> splitString(std::string textString,  char delimeter);

#endif