// Copyright (c) 2023
// Commonwealth Scientific and Industrial Research Organisation (CSIRO)
// ABN 41 687 119 230
//
// Author: Jules Helper // Replace with actual author if known

#include "raylib/raycloud.h"
#include "raylib/rayparse.h" // For command-line parsing
// No longer include "cxxopts.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath> // For std::pow

// Function to calculate uncertainty for each point
// This is a direct translation of the provided pseudocode
void CalculatePointUncertainty(
    ray::Cloud& pointCloud,
    double base_range_accuracy,
    double base_angle_accuracy,
    double c_intensity,
    double epsilon)
{
    double base_range_variance = std::pow(base_range_accuracy, 2);
    double base_angle_variance = std::pow(base_angle_accuracy, 2);

    bool uncertainty_attrib_added = false;
    if (pointCloud.hasAttrib("uncertainty")) {
        uncertainty_attrib_added = true;
    }

    for (size_t i = 0; i < pointCloud.points.size(); ++i) {
        ray::Point& point = pointCloud.points[i];

        // Ensure origin is valid for the point
        Eigen::Vector3d origin_vec = Eigen::Vector3d::Zero(); // Default if not found
        if (i < pointCloud.origins.size()) {
            origin_vec = pointCloud.origins[i];
        } else if (pointCloud.sensor_origin.has_value()) {
            origin_vec = pointCloud.sensor_origin.value();
        }


        Eigen::Vector3d d = point.pos - origin_vec;
        double range_squared = d.squaredNorm();

        float intensity_val = 0.5f; // Default placeholder
        if (pointCloud.hasAttrib("intensity")) {
             try {
                // Ensure the attribute type is float or convertible
                intensity_val = pointCloud.getAttribValue<float>(i, "intensity");
             } catch (const std::out_of_range& oor) {
                // Point does not have this attribute, use default
             } catch (const std::bad_cast& bc) {
                // Attribute is not float, handle error or use default
                std::cerr << "Warning: Intensity attribute for point " << i << " is not of expected type (float)." << std::endl;
             }
        } else if (pointCloud.intensities.size() == pointCloud.points.size()) {
            intensity_val = static_cast<float>(pointCloud.intensities[i]); // Assuming intensities are compatible with float
        }


        double range_uncertainty_variance = base_range_variance * (1.0 + c_intensity / (intensity_val + epsilon));
        double angular_uncertainty_variance = range_squared * base_angle_variance;
        double total_variance = range_uncertainty_variance + angular_uncertainty_variance;

        if (!uncertainty_attrib_added) {
            pointCloud.addAttrib("uncertainty", ray::AttribType::FLOAT64);
            uncertainty_attrib_added = true; // Add attribute only once
        }
        pointCloud.setAttribValue(i, "uncertainty", total_variance);
    }
}

// Custom usage function
void print_usage(int exit_code = 1) {
    // clang-format off
    std::cout << "raynoise: Calculates positional uncertainty for point cloud data." << std::endl;
    std::cout << "Usage: raynoise <input_file> <output_file> [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Required arguments:" << std::endl;
    std::cout << "  <input_file>          Input point cloud file (PLY or LAZ)" << std::endl;
    std::cout << "  <output_file>         Output point cloud file with uncertainty" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --base_range_accuracy <value> (-r <value>)" << std::endl;
    std::cout << "                        Sensor's base 1-sigma range accuracy (m)." << std::endl;
    std::cout << "                        Default: 0.02" << std::endl;
    std::cout << "  --base_angle_accuracy <value> (-a <value>)" << std::endl;
    std::cout << "                        Sensor's base 1-sigma angular accuracy (rad)." << std::endl;
    std::cout << "                        Default: 0.0035" << std::endl;
    std::cout << "  --c_intensity <value> (-c <value>)" << std::endl;
    std::cout << "                        Coefficient for intensity effect." << std::endl;
    std::cout << "                        Default: 0.5" << std::endl;
    std::cout << "  --epsilon <value> (-e <value>)" << std::endl;
    std::cout << "                        Small value for intensity division." << std::endl;
    std::cout << "                        Default: 0.01" << std::endl;
    std::cout << "  --help (-h)           Print this usage message." << std::endl;
    // clang-format on
    exit(exit_code);
}

int rayNoiseMain(int argc, char* argv[]) {
    // Define arguments using rayparse.h classes
    ray::FileArgument inputFile;
    ray::FileArgument outputFile;

    // Optional arguments with default values
    ray::DoubleArgument baseRangeAccuracyArg(0.0, 10.0, 0.02); // min_val, max_val, default_val
    ray::DoubleArgument baseAngleAccuracyArg(0.0, 1.0, 0.0035);
    ray::DoubleArgument cIntensityArg(0.0, 100.0, 0.5); // Increased max for c_intensity
    ray::DoubleArgument epsilonArg(1e-9, 1.0, 0.01);   // Epsilon should be small but positive

    ray::OptionalFlagArgument helpFlag("help", 'h');
    ray::OptionalKeyValueArgument baseRangeOpt("base_range_accuracy", 'r', &baseRangeAccuracyArg);
    ray::OptionalKeyValueArgument baseAngleOpt("base_angle_accuracy", 'a', &baseAngleAccuracyArg);
    ray::OptionalKeyValueArgument cIntensityOpt("c_intensity", 'c', &cIntensityArg);
    ray::OptionalKeyValueArgument epsilonOpt("epsilon", 'e', &epsilonArg);

    // The fixed arguments list
    std::vector<ray::FixedArgument*> fixedArgs = {&inputFile, &outputFile};
    // The optional arguments list
    std::vector<ray::OptionalArgument*> optionalArgs = {&helpFlag, &baseRangeOpt, &baseAngleOpt, &cIntensityOpt, &epsilonOpt};

    if (!ray::parseCommandLine(argc, argv, fixedArgs, optionalArgs)) {
        // ray::parseCommandLine returns false if arguments don't match format or if help is implicitly handled.
        // For safety, explicitly check help flag if parse fails or only program name is given.
        if (argc == 1 || helpFlag.isSet()) { // If only program name or help flag is set
             print_usage(0); // Successful exit for help
        }
        print_usage(1); // Error exit for other parsing failures
    }
     if (helpFlag.isSet()) { // Explicitly check help flag after parsing too
        print_usage(0);
    }


    std::string input_file_str = inputFile.name();
    std::string output_file_str = outputFile.name();

    // Values from arguments. These will hold defaults if options not set.
    double base_range_accuracy = baseRangeAccuracyArg.value();
    double base_angle_accuracy = baseAngleAccuracyArg.value();
    double c_intensity = cIntensityArg.value();
    double epsilon = epsilonArg.value();

    ray::Cloud pointCloud;
    if (!pointCloud.load(input_file_str)) {
        std::cerr << "Error: Could not load point cloud from " << input_file_str << std::endl;
        return 1;
    }

    if (pointCloud.points.empty()) {
        std::cerr << "Error: Point cloud is empty after loading." << std::endl;
        return 1;
    }

    // Ensure origins are available for all points
    if (pointCloud.origins.size() != pointCloud.points.size()) {
        if (pointCloud.sensor_origin.has_value()) {
            std::cerr << "Warning: Per-point origins not found. Using single sensor_origin for all points." << std::endl;
            pointCloud.origins.assign(pointCloud.points.size(), pointCloud.sensor_origin.value());
        } else {
            std::cerr << "Warning: Point origins are not available. Assuming (0,0,0) for all points." << std::endl;
            Eigen::Vector3d default_origin = Eigen::Vector3d::Zero();
            pointCloud.origins.assign(pointCloud.points.size(), default_origin);
        }
    }

    // Check for intensity data presence
    bool has_intensity_attrib = pointCloud.hasAttrib("intensity");
    bool has_intensity_vector = !pointCloud.intensities.empty() && pointCloud.intensities.size() == pointCloud.points.size();
    if (!has_intensity_attrib && !has_intensity_vector) {
        std::cerr << "Warning: Intensity data not found (neither as attribute 'intensity' nor in .intensities vector). Calculations involving intensity may be inaccurate, using default intensity 0.5." << std::endl;
    }


    CalculatePointUncertainty(pointCloud, base_range_accuracy, base_angle_accuracy, c_intensity, epsilon);

    if (!pointCloud.save(output_file_str)) {
        std::cerr << "Error: Could not save point cloud to " << output_file_str << std::endl;
        return 1;
    }

    std::cout << "Successfully processed point cloud. Output saved to " << output_file_str << std::endl;
    return 0;
}

int main(int argc, char* argv[]) {
    // Consider using ray::runWithMemoryCheck if it's a pattern in the project
    // return ray::runWithMemoryCheck(rayNoiseMain, argc, argv);
    return rayNoiseMain(argc, argv);
}
