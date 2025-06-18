#include "gtest/gtest.h"
#include "raynoise_test_utils.h" // Now local to tests/raytest
#include <string>
#include <vector>
#include <cstdlib> // For std::getenv, std::system
#include <iostream> // For cout/cerr
#include <iomanip>  // For std::fixed, std::setprecision
#include <cmath>    // For std::abs
#include <fstream>  // For checking file existence
#include <algorithm> // for std::isalnum

// Define a struct to hold parameters for each test instance
struct RayNoiseTestCaseParams {
    std::string test_name_suffix; // e.g., "Basic_P1"
    std::string input_file_name;  // e.g., "test_basic.ply"
    std::vector<std::string> raynoise_args;
    int point_index_to_check;
    RayNoiseTestOutput expected_values; // The struct from raynoise_test_utils.h
};

// Global variables to store paths from environment (set by CTest or testmain)
std::string g_raynoise_exe_path;
std::string g_raynoise_data_dir;
std::string g_raynoise_test_output_dir_base = "raynoise_gtest_outputs"; // Default relative to CTest working dir

const double G_FLOAT_TOLERANCE = 1e-6;

// Class to pretty print test parameters in GTest output
struct PrintToStringParamName {
    template <class ParamType>
    std::string operator()(const testing::TestParamInfo<ParamType>& info) const {
        std::string name = info.param.test_name_suffix;
        for (char &c : name) {
            if (!std::isalnum(static_cast<unsigned char>(c))) {
                c = '_';
            }
        }
        return name;
    }
};

// The Test Fixture using ::testing::TestWithParam
class RayNoiseGTest : public ::testing::TestWithParam<RayNoiseTestCaseParams> {
public:
    static void SetUpTestSuite() {
        const char* exe_path_env = std::getenv("RAYNOISE_EXE_PATH");
        if (exe_path_env) {
            g_raynoise_exe_path = exe_path_env;
        } else {
            // This is a critical error for the tests to run.
            // GTest's ASSERT macros can't be used in SetUpTestSuite directly to fail all tests.
            // Throwing an exception or using ADD_FAILURE() might be options,
            // but for simplicity, we'll log and tests will likely fail at runtime.
            std::cerr << "CRITICAL ERROR: RAYNOISE_EXE_PATH environment variable not set. Tests will fail." << std::endl;
        }

        const char* data_dir_env = std::getenv("RAYNOISE_DATA_DIR");
        if (data_dir_env) {
            g_raynoise_data_dir = data_dir_env;
        } else {
            std::cerr << "CRITICAL ERROR: RAYNOISE_DATA_DIR environment variable not set. Tests will fail." << std::endl;
        }

        const char* output_dir_base_env = std::getenv("RAYNOISE_TEST_OUTPUT_DIR_BASE");
        if (output_dir_base_env) {
            g_raynoise_test_output_dir_base = output_dir_base_env;
        }

        std::string mkdir_cmd = "mkdir -p \"" + g_raynoise_test_output_dir_base + "\"";
        #ifdef _WIN32
        // Replace mkdir -p with something Windows-friendly if necessary, e.g. system("if not exist ... mkdir ...")
        // For now, assume POSIX-like environment or CMake handles directory creation.
        // Or, more robustly, use C++17 std::filesystem::create_directories
        // This simple version might fail on Windows if 'mkdir -p' is not available.
        // A common CTest setup often pre-creates the output directory.
        // For this project, CMAKE_CURRENT_BINARY_DIR for the test command implies the dir should exist.
        // Let's assume the test output dir structure (e.g. build/tests/raynoise_gtest_outputs)
        // is handled by CMake when defining the test or test executable's working directory.
        // The `file(MAKE_DIRECTORY)` in the main tests/CMakeLists.txt for the old C++ test runner
        // was for `${CMAKE_CURRENT_BINARY_DIR}/raynoise_test_outputs`.
        // This new GTest setup might need a similar guarantee or rely on CTest's working directory management.
        // For now, we'll attempt a generic mkdir -p.
        #endif
        int ret = std::system(mkdir_cmd.c_str());
        if (ret != 0) {
            // This is not necessarily fatal if the directory already exists or is created by other means
            std::cout << "Notice: 'mkdir -p " << g_raynoise_test_output_dir_base << "' command returned " << ret
                      << ". Assuming directory exists or will be handled." << std::endl;
        }

        std::cout << std::fixed << std::setprecision(15);
        std::cout << "RayNoiseGTest: Using raynoise executable: " << g_raynoise_exe_path << std::endl;
        std::cout << "RayNoiseGTest: Using test data directory: " << g_raynoise_data_dir << std::endl;
        std::cout << "RayNoiseGTest: Using output directory for raynoise PLYs: " << g_raynoise_test_output_dir_base << std::endl;
        std::cout << "RayNoiseGTest: Float comparison tolerance: " << G_FLOAT_TOLERANCE << std::endl;
    }

    static void TearDownTestSuite() {
        // Optional: clean up generated files or directories
        // std::string rmdir_cmd = "rm -rf " + g_raynoise_test_output_dir_base;
        // std::system(rmdir_cmd.c_str());
    }

protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_P(RayNoiseGTest, RunsAndChecksOutput) {
    const RayNoiseTestCaseParams& params = GetParam();

    ASSERT_FALSE(g_raynoise_exe_path.empty()) << "Raynoise executable path not set (check RAYNOISE_EXE_PATH env var).";
    ASSERT_FALSE(g_raynoise_data_dir.empty()) << "Raynoise data directory path not set (check RAYNOISE_DATA_DIR env var).";

    std::string input_ply_path = g_raynoise_data_dir + "/" + params.input_file_name;

    std::string safe_output_filename_base = params.test_name_suffix;
    for (char &c : safe_output_filename_base) {
        if (!std::isalnum(static_cast<unsigned char>(c))) { c = '_'; }
    }
    std::string output_ply_path = g_raynoise_test_output_dir_base + "/" + safe_output_filename_base + "_output.ply";

    std::cout << "\n--- Running GTest instance: " << params.test_name_suffix << " ---" << std::endl;
    std::cout << "Input PLY: " << input_ply_path << std::endl;
    std::cout << "Output PLY: " << output_ply_path << std::endl;

    std::string command_str = "\"" + g_raynoise_exe_path + "\" \"" + input_ply_path + "\" \"" + output_ply_path + "\"";
    for (const auto& arg : params.raynoise_args) {
        command_str += " " + arg;
    }
    std::cout << "Executing: " << command_str << std::endl;

    int system_ret = std::system(command_str.c_str());
    ASSERT_EQ(system_ret, 0) << "raynoise execution failed for " << params.test_name_suffix << " with exit code " << system_ret
                             << ". Check logs in " << g_raynoise_test_output_dir_base;


    std::ifstream outfile_check(output_ply_path, std::ios::binary);
    ASSERT_TRUE(outfile_check.good()) << "Output PLY file not found or not readable: " << output_ply_path;
    outfile_check.close();

    RayNoiseTestOutput actual_data;
    std::cout << "  Checking Point Index: " << params.point_index_to_check << std::endl;
    ASSERT_TRUE(parseRayNoiseOutputPly(output_ply_path, params.point_index_to_check, actual_data))
        << "Failed to parse output PLY '" << output_ply_path << "' for point " << params.point_index_to_check;

    const RayNoiseTestOutput& expected_data = params.expected_values;

    EXPECT_NEAR(actual_data.total_variance, expected_data.total_variance, G_FLOAT_TOLERANCE) << "Mismatch for total_variance";
    EXPECT_NEAR(actual_data.range_variance, expected_data.range_variance, G_FLOAT_TOLERANCE) << "Mismatch for range_variance";
    EXPECT_NEAR(actual_data.angular_variance, expected_data.angular_variance, G_FLOAT_TOLERANCE) << "Mismatch for angular_variance";
    EXPECT_NEAR(actual_data.aoi_variance, expected_data.aoi_variance, G_FLOAT_TOLERANCE) << "Mismatch for aoi_variance";
    EXPECT_NEAR(actual_data.mixed_pixel_variance, expected_data.mixed_pixel_variance, G_FLOAT_TOLERANCE) << "Mismatch for mixed_pixel_variance";
}

const RayNoiseTestCaseParams gtest_raynoise_cases[] = {
    {
        "Basic_P1", "test_basic.ply",
        {"--c_aoi", "0", "--penalty_mixed", "0", "--c_intensity", "0.5", "--epsilon", "0.01"},
        0,
        []{ RayNoiseTestOutput d;
            d.total_variance = 0.0006591635802469136;
            d.range_variance = 0.0006469135802469136;
            d.angular_variance = 0.00001225;
            return d; }()
    },
    {
        "Basic_P2", "test_basic.ply",
        {"--c_aoi", "0", "--penalty_mixed", "0", "--c_intensity", "0.5", "--epsilon", "0.01"},
        1,
        []{ RayNoiseTestOutput d;
            d.total_variance = 0.001090108682800641;
            d.range_variance = 0.001041108682800641;
            d.angular_variance = 0.000049;
            return d; }()
    },
    {
        "AoI_P1_check", "test_aoi.ply",
        {"--c_intensity", "0", "--penalty_mixed", "0"},
        0,
        []{ RayNoiseTestOutput d;
            d.total_variance = 0.1398728609039869;
            d.range_variance = 0.0004;
            d.angular_variance = 0.0000245;
            d.aoi_variance = 0.1394483609039869;
            return d; }()
    },
    {
        "AoI_P2_check", "test_aoi.ply",
        {"--c_intensity", "0", "--penalty_mixed", "0"},
        1,
        []{ RayNoiseTestOutput d;
            d.total_variance = 0.09942215099009901;
            d.range_variance = 0.0004;
            d.angular_variance = 0.00001225;
            d.aoi_variance = 0.09900990099009901;
            return d; }()
    },
    {
        "Mixed_P_test", "test_mixed.ply",
        {"--c_intensity", "0", "--c_aoi", "0"},
        0,
        []{ RayNoiseTestOutput d;
            d.total_variance = 0.5004275625;
            d.range_variance = 0.0004;
            d.angular_variance = 0.0000275625;
            d.mixed_pixel_variance = 0.5;
            return d; }()
    },
    {
        "Mixed_SF1", "test_mixed.ply",
        {"--c_intensity", "0", "--c_aoi", "0"},
        1,
        []{ RayNoiseTestOutput d;
            d.total_variance = 0.0004123725;
            d.range_variance = 0.0004;
            d.angular_variance = 0.0000123725;
            return d; }()
    },
    // --- Phase 1 Chunking Tests ---
    {
        "Basic_Chunked_SmallChunk_P1", "test_basic.ply",
        {"--c_aoi", "0.1", "--epsilon_aoi", "0.01", // Enable AoI for fallback check
         "--penalty_mixed", "0.5", // Enable mixed for fallback check (should be 0)
         "--c_intensity", "0.5", "--epsilon", "0.01",
         "--chunk_size", "2"}, // Small chunk size
        0, // Point index to check
        []{ RayNoiseTestOutput d; // Expected values for point 0 from Basic_P1
            d.total_variance = 0.0006469135802469136 /*range*/ + 0.00001225 /*angular*/ + (0.1/0.01) /*AoI fallback*/ + 0.0 /*Mixed fallback*/;
            d.range_variance = 0.0006469135802469136;
            d.angular_variance = 0.00001225;
            d.aoi_variance = (0.1/0.01); // c_aoi / epsilon_aoi
            d.mixed_pixel_variance = 0.0; // Simplified
            return d; }()
    },
    {
        "AoI_Chunked_P1_VerifySimplification", "test_aoi.ply",
        { // Args from AoI_P1_check, but with chunk_size
         "--c_intensity", "0", "--penalty_mixed", "0",
         "--c_aoi", "0.1", "--epsilon_aoi", "0.01", // Default AoI params
         "--chunk_size", "3"},
        0, // Point index from AoI_P1_check
        []{ RayNoiseTestOutput d; // Base values from AoI_P1_check
            d.range_variance = 0.0004;
            d.angular_variance = 0.0000245;
            // AoI is now fallback
            d.aoi_variance = (0.1/0.01); // c_aoi / epsilon_aoi (defaults)
            d.mixed_pixel_variance = 0.0; // Simplified (was 0 anyway)
            d.total_variance = d.range_variance + d.angular_variance + d.aoi_variance + d.mixed_pixel_variance;
            return d; }()
    },
    {
        "Mixed_Chunked_P_VerifySimplification", "test_mixed.ply",
        { // Args from Mixed_P_test, but with chunk_size
         "--c_intensity", "0",
         "--c_aoi", "0.1", "--epsilon_aoi", "0.01", // Enable AoI for fallback check
         "--penalty_mixed", "0.5", // This was the original penalty
         "--k_mixed", "8", "--depth_thresh_mixed", "0.05", "--min_front_mixed", "1", "--min_behind_mixed", "1", // Mixed params
         "--chunk_size", "5"},
        0, // Point index from Mixed_P_test
        []{ RayNoiseTestOutput d; // Base values from Mixed_P_test
            d.range_variance = 0.0004;
            d.angular_variance = 0.0000275625;
            d.aoi_variance = (0.1/0.01); // AoI is now fallback
            d.mixed_pixel_variance = 0.0; // Mixed pixel is simplified to 0
            d.total_variance = d.range_variance + d.angular_variance + d.aoi_variance + d.mixed_pixel_variance;
            return d; }()
    },
    {
        "Basic_Chunked_LargeChunk_P1", "test_basic.ply", // Chunk size larger than file
        {"--c_aoi", "0.1", "--epsilon_aoi", "0.01",
         "--penalty_mixed", "0", // keep mixed off for this basic test
         "--c_intensity", "0.5", "--epsilon", "0.01",
         "--chunk_size", "100"}, // test_basic.ply has few points (e.g. <10)
        0,
        []{ RayNoiseTestOutput d; // Same as Basic_Chunked_SmallChunk_P1 because simplifications apply even if one chunk
            d.total_variance = 0.0006469135802469136 + 0.00001225 + (0.1/0.01) + 0.0;
            d.range_variance = 0.0006469135802469136;
            d.angular_variance = 0.00001225;
            d.aoi_variance = (0.1/0.01);
            d.mixed_pixel_variance = 0.0;
            return d; }()
    },
    {
        "Basic_Chunked_ExplicitAoIEpsilon_P1", "test_basic.ply",
        {"--c_aoi", "0.2", "--epsilon_aoi", "0.05", // Non-default AoI params
         "--penalty_mixed", "0",
         "--c_intensity", "0.5", "--epsilon", "0.01",
         "--chunk_size", "2"},
        0,
        []{ RayNoiseTestOutput d;
            d.range_variance = 0.0006469135802469136; // from Basic_P1
            d.angular_variance = 0.00001225;      // from Basic_P1
            d.aoi_variance = (0.2/0.05);          // Explicit c_aoi / epsilon_aoi
            d.mixed_pixel_variance = 0.0;         // Simplified
            d.total_variance = d.range_variance + d.angular_variance + d.aoi_variance + d.mixed_pixel_variance;
            return d; }()
    }
};

INSTANTIATE_TEST_SUITE_P(
    RayNoiseTests,
    RayNoiseGTest,
    ::testing::ValuesIn(gtest_raynoise_cases),
    PrintToStringParamName()
);

// No main() function is defined here.
// It is assumed that this file will be linked with a main function (e.g., from testmain.cpp in the project)
// that calls ::testing::InitGoogleTest(&argc, argv); and RUN_ALL_TESTS();
// The global path variables (g_raynoise_exe_path, etc.) are expected to be populated
// either by SetUpTestSuite reading environment variables (as implemented) or by testmain.cpp
// before RUN_ALL_TESTS() if a different mechanism is preferred.
// For CTest, environment variables are a common way to pass such paths.
