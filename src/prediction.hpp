#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <list>
#include <map>

// Conversion struct from torch tensor classification output to human readable
// (Defenitions where created in the python MetaDataCatalouge map):
//      node,text,edge,arrowhead <=> 0,1,2,3
const static std::map<int,std::string> CLASSES {
  {0,"node"},
  {1,"text"},
  {2,"edge"},
  {3,"arrowhead"},
};

struct Prediction
{
    float score;
    float boundingBox[4];
    std::string classification;
};

std::list<Prediction> makePredictions(std::string imageFilePath, std::string modelFilePath);