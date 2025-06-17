#ifndef RAYNOISE_TEST_UTILS_H
#define RAYNOISE_TEST_UTILS_H

#include <string>
#include <vector>
#include <cstdint> // For uint8_t

// To determine coordinate type (float/double) based on raylib compilation.
// This ideally comes from an installed raylib config header.
// If raylib installs raylibconfig.h to a known include path, use that.
// For now, we might have to assume or make it a compile def for the test.
// Let's assume RAYLIB_DOUBLE_RAYS is passed as a compile definition for the test,
// or we default it.
#ifndef RAYLIB_DOUBLE_RAYS
#define RAYLIB_DOUBLE_RAYS 0 // Default to float if not defined elsewhere for the test compilation
#endif

struct RayNoiseTestOutput {
#if RAYLIB_DOUBLE_RAYS
    double x, y, z;
#else
    float x, y, z;
#endif
    double time;
    // Ray vector (nx, ny, nz) - these are floats as per rayply.cpp
    float nx, ny, nz;
    // Color
    uint8_t red, green, blue, alpha;
    // Uncertainty components - all double
    double total_variance;
    double range_variance;
    double angular_variance;
    double aoi_variance;
    double mixed_pixel_variance;

    // Default constructor
    RayNoiseTestOutput() :
        x(0), y(0), z(0), time(0),
        nx(0), ny(0), nz(0),
        red(0), green(0), blue(0), alpha(0),
        total_variance(0), range_variance(0), angular_variance(0),
        aoi_variance(0), mixed_pixel_variance(0) {}
};

/**
 * @brief Parses a specific point's data from a binary PLY file output by raynoise.
 *
 * @param filePath Path to the binary PLY file.
 * @param pointIndex 0-based index of the point to parse.
 * @param out_data Reference to a RayNoiseTestOutput struct to be filled.
 * @return true if parsing was successful, false otherwise.
 */
bool parseRayNoiseOutputPly(
    const std::string& filePath,
    int pointIndex,
    RayNoiseTestOutput& out_data
);

/**
 * @brief Parses all points' data from a binary PLY file output by raynoise.
 *
 * @param filePath Path to the binary PLY file.
 * @param out_data_vector Reference to a vector of RayNoiseTestOutput structs to be filled.
 * @return true if parsing was successful, false otherwise.
 */
bool parseAllRayNoiseOutputPly(
    const std::string& filePath,
    std::vector<RayNoiseTestOutput>& out_data_vector
);


#endif // RAYNOISE_TEST_UTILS_H
