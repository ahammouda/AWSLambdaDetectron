#include "utils.hpp"

/* bboxOne for example - bboxTwo reflects an analagous shape
 * <xL,yL> ...... <xL,yM>
 *   |               |
 *   |               |
 * <xL,yM> ...... <xM,yM>
 */
bool bBoxOverlap(
        const float bboxOne[4],
        const float bboxTwo[4],
        float tolerance)
{
    float xL=bboxOne[0], yL=bboxOne[1], xM=bboxOne[2], yM=bboxOne[3];
    float xPl=bboxTwo[0], yPl=bboxTwo[1], xPm=bboxTwo[2], yPm=bboxTwo[3];

    if ( xPl > xL + tolerance && xPl < xM - tolerance ){
        return true;
    }
    if ( xPm > xL + tolerance && xPm < xM - tolerance ){
        return true;
    }
    if ( yPl > yL + tolerance && yPl < yM - tolerance ) {
        return true;
    }
    if ( yPm > yL + tolerance && yPm < yM - tolerance ) {
        return true;
    }
    return false;
}

/* Not only run this algorithm, but also print out a csv file of bboxes side by side with scores both
     before and after
 */
void dedupPredictions(std::list<Prediction> &predictions, float tolerance){
    int i=0,j=0;
    std::map<int,Prediction> redundantPredictions = {};
    std::map<int,Prediction> relevantPredictions = {};
    // std::list<Prediction> finalPredictions = {};

    for (Prediction const &pred1: predictions){
        j=0;
        for (Prediction const &pred2: predictions){
            // Don't bother comparing elements already removed or elements that are equal
            if (i==j ||
                redundantPredictions.find(i) != redundantPredictions.end() ||
                redundantPredictions.find(j) != redundantPredictions.end() ){
                continue;
            }
            if ( pred1.classification == pred2.classification &&
                 bBoxOverlap(pred1.boundingBox,pred2.boundingBox, tolerance) ) {
                    // Remove the lower scoring prediction
                    if (pred1.score > pred2.score){
                        relevantPredictions[i] = pred1;
                        // keep pred1
                        redundantPredictions[j] = pred2;
                    } else {
                        relevantPredictions[j] = pred2;
                        // keep pred2
                        redundantPredictions[i] = pred1;
                    }
            } else {
                relevantPredictions[i] = pred1;
                relevantPredictions[j] = pred2;
            }
            j++;
        }
        i++;
    }
    std::cout << " ********* Post - Deduplication ********* " << std::endl;
    i=0;
    for (Prediction &predElem: predictions){
        if (relevantPredictions.find(i) != redundantPredictions.end()){
            std::cout << i << ", " << predElem.boundingBox[0] << ","
                << predElem.boundingBox[1] << "," << predElem.boundingBox[2] << ","
                << predElem.boundingBox[3] << "," << predElem.score
                << predElem.classification << std::endl;
            predElem.imageId = std::to_string((int)floor(predElem.boundingBox[0])) + "to"
                                   + std::to_string((int)ceil(predElem.boundingBox[2])) + "x"
                                   + std::to_string((int)floor(predElem.boundingBox[1])) + "to"
                                   + std::to_string((int)ceil(predElem.boundingBox[3]));

            std::cout << "image " << i << "id: " << predElem.imageId
                << "- starts with" << std::to_string((int)floor(predElem.boundingBox[0])) << std::endl;
        } else {
            predElem.imageId = "";
        }
        ++i;
    }
    //return predictions;
}

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
