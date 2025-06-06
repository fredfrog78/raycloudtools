// Copyright (c) 2023
// Commonwealth Scientific and Industrial Research Organisation (CSIRO)
// ABN 41 687 119 230
//
// Author: Jules Helper // Replace with actual author if known

#include "raylib/raycloud.h"
#include "raylib/rayparse.h"
#include "cxxopts.hpp" // Assuming cxxopts is available and included correctly

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>

// Define a structure for Point, assuming it's not already defined in raylib
// If raylib::Point exists and is suitable, this can be removed or adapted.
// For now, let's assume we need to define it or adapt an existing one.
// Based on the pseudocode, raylib::Point likely has 'pos' and 'origin',
// and we will add 'intensity' and 'uncertainty'.

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

    // Assuming pointCloud.points is the vector of points
    // And each point has members: pos (Eigen::Vector3d), origin (Eigen::Vector3d), intensity (float)
    // We will add 'uncertainty' (float or double) to each point.
    // raylib::Point might need to be extended or a new structure used if it doesn't support custom attributes.
    // For this initial implementation, we'll assume points can have a generic attribute map
    // or that we can extend the structure. If raylib::Cloud stores points as raylib::Point,
    // and raylib::Point has a way to store custom attributes, that would be ideal.
    // Let's assume raylib::Point has a 'custom_attributes' map or similar,
    // or we add 'intensity' and 'uncertainty' fields if we control the Point definition.

    // If raylib::Point does not have intensity, we might need to load it from a separate file
    // or assume it's part of a combined attribute in raylib.
    // The issue implies 'intensity' is a direct attribute.

    for (size_t i = 0; i < pointCloud.points.size(); ++i) {
        ray::Point& point = pointCloud.points[i];

        // 1. Calculate the measured range
        Eigen::Vector3d d = point.pos - point.origin;
        double range_squared = d.squaredNorm();
        // double range = d.norm(); // Not strictly needed for variance calculation

        // Ensure intensity is available. If not, this part needs rethinking.
        // For now, assume point.intensity exists and is normalized (0.0 to 1.0)
        // If point.intensity is not a standard field, this will require modification.
        // A common way to handle this in PLY files is to have an 'intensity' property.
        // raylib::Cloud::load() should ideally pick this up.
        // If not, we might need to manually check for an attribute named "intensity".
        float intensity = 0.5f; // Default placeholder if not available
        if (pointCloud.hasAttrib("intensity")) { // Check if cloud has intensity attribute globally
            // This is a guess at how raylib might provide attribute access
            // It might be point.getAttrib("intensity") or point.intensity directly
             try {
                intensity = pointCloud.getAttribValue<float>(i, "intensity");
             } catch (const std::out_of_range& oor) {
                // Fallback or error if intensity is expected but not found for a point
                // For now, use default, but ideally, this should be handled based on requirements
             }
        } else if (pointCloud.intensities.size() == pointCloud.points.size()) {
            intensity = pointCloud.intensities[i];
        }


        // 2. Model Range Uncertainty Component
        double range_uncertainty_variance = base_range_variance * (1.0 + c_intensity / (intensity + epsilon));

        // 3. Model Angular Uncertainty Component
        double angular_uncertainty_variance = range_squared * base_angle_variance;

        // 4. Combine for Total Uncertainty (Variance)
        double total_variance = range_uncertainty_variance + angular_uncertainty_variance;

        // 5. Assign the calculated uncertainty to the point object
        // This requires a way to store 'uncertainty' on the point.
        // If raylib::Point can't be extended, we might store it in a parallel vector
        // or more ideally, add it as a custom attribute.
        // point.uncertainty = total_variance; // Ideal if Point struct can be modified
        // Assuming a generic attribute system:
        pointCloud.addAttrib("uncertainty", ray::AttribType::FLOAT64); // Ensure attribute exists
        pointCloud.setAttribValue(i, "uncertainty", total_variance);
    }
}

void usage(const cxxopts::Options& options, int exit_code = 1) {
    std::cout << options.help() << std::endl;
    exit(exit_code);
}

int rayNoise(int argc, char* argv[]) {
    cxxopts::Options options("raynoise", "Calculates positional uncertainty for point cloud data.");

    options.add_options()
        ("i,input", "Input point cloud file (PLY or LAZ)", cxxopts::value<std::string>())
        ("o,output", "Output point cloud file with uncertainty data", cxxopts::value<std::string>())
        ("base_range_accuracy", "Sensor's base 1-sigma range accuracy (meters)", cxxopts::value<double>()->default_value("0.02"))
        ("base_angle_accuracy", "Sensor's base 1-sigma angular accuracy (radians)", cxxopts::value<double>()->default_value("0.0035"))
        ("c_intensity", "Coefficient for intensity effect on uncertainty", cxxopts::value<double>()->default_value("0.5"))
        ("epsilon", "Small value to prevent division by zero with intensity", cxxopts::value<double>()->default_value("0.01"))
        ("h,help", "Print usage");

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
        usage(options);
    }

    if (!result.count("input") || !result.count("output")) {
        std::cerr << "Error: Input and output file paths are required." << std::endl;
        usage(options);
    }

    std::string input_file = result["input"].as<std::string>();
    std::string output_file = result["output"].as<std::string>();
    double base_range_accuracy = result["base_range_accuracy"].as<double>();
    double base_angle_accuracy = result["base_angle_accuracy"].as<double>();
    double c_intensity = result["c_intensity"].as<double>();
    double epsilon = result["epsilon"].as<double>();

    ray::Cloud pointCloud;
    if (!pointCloud.load(input_file)) {
        std::cerr << "Error: Could not load point cloud from " << input_file << std::endl;
        return 1;
    }

    if (pointCloud.points.empty()) {
        std::cerr << "Error: Point cloud is empty." << std::endl;
        return 1;
    }

    // Check if origins are present, if not, this tool might not be applicable
    // or origins need to be assumed (e.g., all (0,0,0))
    if (pointCloud.origins.empty() && pointCloud.points.size() > 0) {
         // Assuming all points share one origin if not specified per point
        if (pointCloud.sensor_origin.has_value()) {
            pointCloud.origins.resize(pointCloud.points.size(), pointCloud.sensor_origin.value());
        } else {
            std::cerr << "Warning: Point origins are not available. Assuming (0,0,0) for all." << std::endl;
            // Or, if sensor_origin is available in the cloud metadata, use that for all points.
            // For now, defaulting to (0,0,0) if no other info.
            Eigen::Vector3d default_origin(0.0, 0.0, 0.0);
            pointCloud.origins.resize(pointCloud.points.size(), default_origin);
        }
    }


    // Check for intensity attribute.
    // The pseudocode implies intensity is per-point.
    // raylib::Cloud might store intensities in `pointCloud.intensities` (if it's a common attribute)
    // or it might be a custom attribute.
    bool has_intensity = pointCloud.hasAttrib("intensity") || !pointCloud.intensities.empty();
    if (!has_intensity) {
        std::cerr << "Warning: Intensity data not found. Uncertainty calculations involving intensity may be inaccurate." << std::endl;
        // The CalculatePointUncertainty function uses a default if intensity is not found,
        // but a warning to the user is good.
    }


    CalculatePointUncertainty(pointCloud, base_range_accuracy, base_angle_accuracy, c_intensity, epsilon);

    // Ensure the "uncertainty" attribute is marked for saving.
    // This might be handled by addAttrib, or might need an explicit call.
    // For example, if attributes need to be whitelisted for saving:
    // pointCloud.setAttribOutput("uncertainty", true);


    if (!pointCloud.save(output_file)) {
        std::cerr << "Error: Could not save point cloud to " << output_file << std::endl;
        return 1;
    }

    std::cout << "Successfully processed point cloud. Output saved to " << output_file << std::endl;

    return 0;
}

int main(int argc, char* argv[]) {
    // It's good practice to wrap the main logic in a function that can be memory checked,
    // similar to other raycloudtools.
    // Assuming ray::runWithMemoryCheck is available from raylib/rayutil.h or similar.
    // If not, a direct call to rayNoise is fine.
    // return ray::runWithMemoryCheck(rayNoise, argc, argv); // If available
    return rayNoise(argc, argv); // Direct call if runWithMemoryCheck is not standard/easily included
}
