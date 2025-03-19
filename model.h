#ifndef MODEL_H
#define MODEL_H

#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/dnn.hpp>
#include <vector>
#include <string>
#include <fstream>

class model
{
public:
    model();
public:
    model(const std::string& configPath, const std::string& weightsPath, const std::string& namesPath);
    void detectAndDisplay(std::vector<cv::Mat>& frames);

private:
    cv::dnn::Net yoloNet_;
    std::vector<std::string> classNames_;
    void printMessage(const std::string& message) const;
};

#endif // MODEL_H
