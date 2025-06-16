#include "raynoise_test_utils.h" // For RayNoiseTestOutput and parsing functions
#include <iostream>
#include <vector>
#include <string>
#include <cmath>   // For std::abs, std::isnormal (potentially)
#include <cstdlib> // For std::system
#include <iomanip> // For std::fixed, std::setprecision
#include <utility> // For std::pair
#include <fstream> // For ifstream to check file existence
#include <algorithm> // For std::isalnum if used in more complex sanitization

// Define a global tolerance for floating point comparisons
const double FLOAT_TOLERANCE = 1e-6;

// Path arguments to be set by main() from CLI
std::string RAYNOISE_EXE_PATH;
std::string TEST_DATA_DIR;
std::string TEST_OUTPUT_DIR = "./raynoise_cpp_test_outputs"; // Default, can be overridden by CLI

struct TestCase {
    std::string name;
    std::string input_file_name; // Relative to TEST_DATA_DIR
    std::vector<std::string> raynoise_args;
    std::vector<std::pair<int, RayNoiseTestOutput>> points_to_check; // Pair: <point_index, expected_data>
};

std::vector<TestCase> test_cases = {
    {
        "Basic_P1", "test_basic.ply",
        {"--c_aoi", "0", "--penalty_mixed", "0", "--c_intensity", "0.5", "--epsilon", "0.01"},
        {
            {0, []{ RayNoiseTestOutput d;
                    d.total_variance = 0.0006591635802469136;
                    d.range_variance = 0.0006469135802469136;
                    d.angular_variance = 0.00001225;
                    d.aoi_variance = 0.0;
                    d.mixed_pixel_variance = 0.0;
                    return d; }()}
        }
    },
    {
        "Basic_P2", "test_basic.ply",
        {"--c_aoi", "0", "--penalty_mixed", "0", "--c_intensity", "0.5", "--epsilon", "0.01"},
        {
            {1, []{ RayNoiseTestOutput d;
                    d.total_variance = 0.001090108682800641;
                    d.range_variance = 0.001041108682800641;
                    d.angular_variance = 0.000049;
                    return d; }()}
        }
    },
    {
        "AoI_P1_check", "test_aoi.ply",
        {"--c_intensity", "0", "--penalty_mixed", "0"},
        {
            {0, []{ RayNoiseTestOutput d;
                    d.total_variance = 0.1398728609039869;
                    d.range_variance = 0.0004;
                    d.angular_variance = 0.0000245;
                    d.aoi_variance = 0.1394483609039869;
                    return d; }()}
        }
    },
    {
        "AoI_P2_check", "test_aoi.ply",
        {"--c_intensity", "0", "--penalty_mixed", "0"},
        {
            {1, []{ RayNoiseTestOutput d;
                    d.total_variance = 0.09942215099009901;
                    d.range_variance = 0.0004;
                    d.angular_variance = 0.00001225;
                    d.aoi_variance = 0.09900990099009901;
                    return d; }()}
        }
    },
    {
        "Mixed_P_test", "test_mixed.ply",
        {"--c_intensity", "0", "--c_aoi", "0"},
        {
            {0, []{ RayNoiseTestOutput d;
                    d.total_variance = 0.5004275625;
                    d.range_variance = 0.0004;
                    d.angular_variance = 0.0000275625;
                    d.mixed_pixel_variance = 0.5;
                    return d; }()}
        }
    },
    {
        "Mixed_SF1", "test_mixed.ply",
        {"--c_intensity", "0", "--c_aoi", "0"},
        {
            {1, []{ RayNoiseTestOutput d;
                    d.total_variance = 0.0004123725;
                    d.range_variance = 0.0004;
                    d.angular_variance = 0.0000123725;
                    return d; }()}
        }
    }
};

// Function to run a single test case
bool run_single_test_case(const TestCase& tc) {
    std::cout << "\n--- Running Test Case: " << tc.name << " ---" << std::endl;

    std::string input_ply_path = TEST_DATA_DIR + "/" + tc.input_file_name;
    std::string safe_output_filename_base = tc.name;
    for (char &c : safe_output_filename_base) {
        if (!std::isalnum(static_cast<unsigned char>(c))) { // Cast to unsigned char for isalnum
            c = '_';
        }
    }
    std::string output_ply_path = TEST_OUTPUT_DIR + "/" + safe_output_filename_base + "_output.ply";

    std::cout << "Input PLY: " << input_ply_path << std::endl;
    std::cout << "Output PLY: " << output_ply_path << std::endl;

    std::string command_str = "\"" + RAYNOISE_EXE_PATH + "\" \"" + input_ply_path + "\" \"" + output_ply_path + "\"";
    for (const auto& arg : tc.raynoise_args) {
        command_str += " " + arg;
    }
    std::cout << "Executing: " << command_str << std::endl;

    // Redirect stdout and stderr of the command to files for inspection if needed
    std::string cmd_stdout_log = TEST_OUTPUT_DIR + "/" + safe_output_filename_base + "_stdout.log";
    std::string cmd_stderr_log = TEST_OUTPUT_DIR + "/" + safe_output_filename_base + "_stderr.log";
    std::string full_command_for_system = command_str + " > \"" + cmd_stdout_log + "\" 2> \"" + cmd_stderr_log + "\"";

    int system_ret = std::system(full_command_for_system.c_str());

    // Log stdout/stderr from the command
    std::ifstream stdout_fs(cmd_stdout_log);
    std::stringstream stdout_ss;
    stdout_ss << stdout_fs.rdbuf();
    if(!stdout_ss.str().empty()){
        std::cout << "raynoise stdout:\n" << stdout_ss.str() << std::endl;
    }
    stdout_fs.close();

    std::ifstream stderr_fs(cmd_stderr_log);
    std::stringstream stderr_ss;
    stderr_ss << stderr_fs.rdbuf();
    if(!stderr_ss.str().empty()){
        std::cerr << "raynoise stderr:\n" << stderr_ss.str() << std::endl;
    }
    stderr_fs.close();

    if (system_ret != 0) {
        std::cerr << "ERROR: raynoise execution failed for " << tc.name << " with exit code " << system_ret << std::endl;
        return false;
    }

    std::ifstream outfile_check(output_ply_path, std::ios::binary); // Open in binary to check
    if (!outfile_check.good()) {
        std::cerr << "ERROR: Output PLY file not found or not readable: " << output_ply_path << std::endl;
        return false;
    }
    outfile_check.close();

    bool case_passed = true;
    for (const auto& point_check_pair : tc.points_to_check) {
        int point_idx = point_check_pair.first;
        const RayNoiseTestOutput& expected_data = point_check_pair.second;
        RayNoiseTestOutput actual_data;

        std::cout << "  Checking Point Index: " << point_idx << std::endl;

        if (!parseRayNoiseOutputPly(output_ply_path, point_idx, actual_data)) {
            std::cerr << "    ERROR: Failed to parse output PLY for point " << point_idx << std::endl;
            case_passed = false;
            continue;
        }

        bool point_passed = true;

        auto compare_field = [&](const std::string& fname, double actual, double expected) {
            if (std::abs(actual - expected) > FLOAT_TOLERANCE) {
                std::cerr << "    FAIL: " << fname << " - Actual: " << actual
                          << ", Expected: " << expected
                          << ", Diff: " << std::abs(actual - expected) << std::endl;
                point_passed = false;
            } else {
                std::cout << "    PASS: " << fname << " - Actual: " << actual << std::endl;
            }
        };

        compare_field("total_variance", actual_data.total_variance, expected_data.total_variance);
        compare_field("range_variance", actual_data.range_variance, expected_data.range_variance);
        compare_field("angular_variance", actual_data.angular_variance, expected_data.angular_variance);
        compare_field("aoi_variance", actual_data.aoi_variance, expected_data.aoi_variance);
        compare_field("mixed_pixel_variance", actual_data.mixed_pixel_variance, expected_data.mixed_pixel_variance);

        if (!point_passed) {
            case_passed = false;
        }
    }

    if (case_passed) {
        std::cout << "--- Test Case " << tc.name << ": PASSED ---" << std::endl;
    } else {
        std::cerr << "--- Test Case " << tc.name << ": FAILED ---" << std::endl;
    }
    return case_passed;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <path_to_raynoise_exe> <path_to_test_data_dir> [path_to_test_output_dir]" << std::endl;
        return 1;
    }
    RAYNOISE_EXE_PATH = argv[1];
    TEST_DATA_DIR = argv[2];
    if (argc >= 4) {
        TEST_OUTPUT_DIR = argv[3];
    }

    // Note: Output directory creation should be handled by the test execution environment (e.g. CTest)
    // or a setup step in CMake. For simplicity here, assume it's writable.
    // std::system(("mkdir -p " + TEST_OUTPUT_DIR).c_str()); // Platform-dependent, less safe

    std::cout << std::fixed << std::setprecision(15);

    std::cout << "Using raynoise executable: " << RAYNOISE_EXE_PATH << std::endl;
    std::cout << "Using test data directory: " << TEST_DATA_DIR << std::endl;
    std::cout << "Using output directory for raynoise outputs: " << TEST_OUTPUT_DIR << std::endl;
    std::cout << "Float comparison tolerance: " << FLOAT_TOLERANCE << std::endl;

    int total_cases = static_cast<int>(test_cases.size());
    int passed_cases = 0;

    for (const auto& tc : test_cases) {
       if (run_single_test_case(tc)) {
           passed_cases++;
       }
    }

    std::cout << "\n--- Test Summary ---" << std::endl;
    std::cout << "Total test cases: " << total_cases << std::endl;
    std::cout << "Passed: " << passed_cases << std::endl;
    std::cout << "Failed: " << total_cases - passed_cases << std::endl;

    return (passed_cases == total_cases) ? 0 : 1;
}
