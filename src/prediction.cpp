// Parts of this fall under:
//   - Copyright (c) Facebook, Inc. and its affiliates.
//   - see fb.LICENSE for more details or the original work: https://github.com/facebookresearch/detectron2/blob/main/tools/deploy/torchscript_mask_rcnn.cpp
// @lint-ignore-every CLANGTIDY

// For prediction capabilities
#include <opencv2/opencv.hpp>
#include <torch/csrc/autograd/grad_mode.h>
#include <torch/csrc/jit/runtime/graph_executor.h>
#include <torch/script.h>
// only needed for export_method=tracing
#include <torchvision/vision.h> // @oss-only

#include "prediction.hpp"

using namespace std;

c10::IValue get_tracing_inputs(cv::Mat& img, c10::Device device) {
  const int height = img.rows;
  const int width = img.cols;
  const int channels = 3;

  auto input =
      torch::from_blob(img.data, {height, width, channels}, torch::kUInt8);
  // HWC to CHW
  input = input.to(device, torch::kFloat).permute({2, 0, 1}).contiguous();
  return input;
}

c10::IValue
get_inputs(std::string export_method, cv::Mat& img, c10::Device device) {
  // Given an image, create inputs in the format required by the model.
  if (export_method == "tracing")
    return get_tracing_inputs(img, device);
  abort();
}

struct MaskRCNNOutputs {
  at::Tensor pred_boxes, pred_classes, pred_masks, scores;
  int num_instances() const {
    return pred_boxes.sizes()[0];
  }
};

MaskRCNNOutputs get_outputs(std::string export_method, c10::IValue outputs) {
  // Given outputs of the model, extract tensors from it to turn into a
  // common MaskRCNNOutputs format.
  if (export_method == "tracing") {
    auto out_tuple = outputs.toTuple()->elements();
    // They are ordered alphabetically by their field name in Instances
    return MaskRCNNOutputs{
        out_tuple[0].toTensor(),
        out_tuple[1].toTensor(),
        out_tuple[2].toTensor(),
        out_tuple[3].toTensor()};
  }
  abort();
}

list<Prediction> makePredictions(std::string imageFilePath, std::string modelFilePath)
{
    std::cout << "Calling makePredictions with model " << modelFilePath <<
      " and image " << imageFilePath << "..."  << std::endl;

    std::string export_method = "tracing";

    std::list<Prediction> predictions = {};

    std::cout << "Loading the model..."  << std::endl;
    // This doesn't seem relevent to your usecase
    // torch::autograd::AutoGradMode guard(false);
    auto module = torch::jit::load( modelFilePath );

    assert(module.buffers().size() > 0);
    // Assume that the entire model is on the same device.
    // We just put input to this device.
    auto device = (*begin(module.buffers())).device();

    std::cout << "Reading the image..."  << std::endl;
    cv::Mat input_img = cv::imread(imageFilePath, cv::IMREAD_COLOR);
    auto inputs = get_inputs(export_method, input_img, device);

    std::cout << "Running the network..."  << std::endl;
    // Run the network
    auto output = module.forward({inputs});

    // Parse Mask R-CNN outputs
    auto rcnn_outputs = get_outputs(export_method, output);
    cout << "Number of detected objects: "
        << rcnn_outputs.num_instances() << endl;

    cout << "pred_boxes: " << rcnn_outputs.pred_boxes.toString() << " "
        << rcnn_outputs.pred_boxes.sizes() << endl;
    cout << "scores: " << rcnn_outputs.scores.toString() << " "
        << rcnn_outputs.scores.sizes() << endl;
    cout << "pred_classes: " << rcnn_outputs.pred_classes.toString() << " "
        << rcnn_outputs.pred_classes.sizes() << endl;
    cout << "pred_masks: " << rcnn_outputs.pred_masks.toString() << " "
        << rcnn_outputs.pred_masks.sizes() << endl;


    for (int i; i<rcnn_outputs.num_instances();++i){
        Prediction prediction = {};
        // extract bounding box from tensor
        for (int q; q<4; q++){
          prediction.boundingBox[q] = rcnn_outputs.pred_boxes[i].data_ptr<float>()[q];
        }
        prediction.score = rcnn_outputs.scores[i].item<float>();

        // extract classification from tensor
        std::string classString = CLASSES.at( rcnn_outputs.pred_classes[i].item<int>() );
        //  https://www.techiedelight.com/convert-std-string-char-cpp/
        prediction.classification = const_cast<char*>( classString.c_str() );
        predictions.push_back(prediction);
    }

    std::cout << "Added " << predictions.size() << " predictions" << std::endl;
    return predictions;
}