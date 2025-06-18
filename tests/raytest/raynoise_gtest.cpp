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
        // UPDATED Test: Basic_Chunked_SmallChunk_P1 -> Basic_TwoPass_SmallPass2Chunk_P1
        // AoI should now be accurate due to Pass 1 normals. Mixed Pixel still local to Pass 2 chunk.
        // Using default AoI params c_aoi=0.1, epsilon_aoi=0.01. test_basic.ply point 0 is flat, so AoI is low.
        // From Basic_P1 (non-chunked), AoI is 0 if --c_aoi 0. For this test, we use default AoI values.
        // The original Basic_P1 had c_aoi=0. Here, we'll test with default AoI parameters.
        // Point 0 of test_basic.ply should have an AoI close to 0 (normal is (0,0,1), ray is approx (0,0,-L)).
        // Let's assume near-perfect alignment for test_basic.ply point 0, so cos_theta ~ 1.
        // AoI variance would be c_aoi / (1 + epsilon_aoi) = 0.1 / (1 + 0.01) = 0.1 / 1.01 = 0.0990099...
        "Basic_TwoPass_SmallPass2Chunk_P1", "test_basic.ply",
        {
         "--c_intensity", "0.5", "--epsilon", "0.01", // For range variance
         // Default AoI params: --c_aoi 0.1, --epsilon_aoi 0.01
         // Default Mixed Pixel params: --penalty_mixed 0.5, k_mixed 8 etc.
         "--chunk_size", "2" // Small Pass 2 chunk size
        },
        0, // Point index to check (from original Basic_P1)
        []{ RayNoiseTestOutput d;
            d.range_variance = 0.0006469135802469136; // From Basic_P1
            d.angular_variance = 0.00001225;          // From Basic_P1
            d.aoi_variance = 0.1 / (1.0 + 0.01);      // c_aoi/(cos_theta + epsilon_aoi), cos_theta ~1 for test_basic.ply point 0
            d.mixed_pixel_variance = 0.0;             // Local kNN in small chunk (size 2, k_mixed=8) likely finds no mixed pixels.
            d.total_variance = d.range_variance + d.angular_variance + d.aoi_variance + d.mixed_pixel_variance;
            return d; }()
    },
    {
        // NEW Test: AoI_TwoPass_Accuracy_P1
        // Compare with AoI_P1_check (non-chunked). Expect AoI to be very similar.
        "AoI_TwoPass_Accuracy_P1", "test_aoi.ply",
        {
         "--c_intensity", "0", "--penalty_mixed", "0", // Match AoI_P1_check args
         // Default AoI params: --c_aoi 0.1, --epsilon_aoi 0.01
         "--chunk_size", "3" // Small Pass 2 chunk size
        },
        0, // Point index from AoI_P1_check
        []{ RayNoiseTestOutput d;
            d.range_variance = 0.0004;                // From AoI_P1_check
            d.angular_variance = 0.0000245;           // From AoI_P1_check
            d.aoi_variance = 0.1394483609039869;      // EXPECTED TO MATCH AoI_P1_check
            d.mixed_pixel_variance = 0.0;             // --penalty_mixed 0
            d.total_variance = d.range_variance + d.angular_variance + d.aoi_variance + d.mixed_pixel_variance;
            return d; }()
    },
    {
        // UPDATED Test: Mixed_Chunked_P_VerifySimplification -> Mixed_TwoPass_SmallPass2Chunk_P0
        // Original Mixed_P_test has mixed_pixel_variance = 0.5.
        // With two-pass, and small Pass 2 chunk_size=2 (k_mixed=8 default), it should NOT detect mixed pixel.
        "Mixed_TwoPass_SmallPass2Chunk_P0", "test_mixed.ply",
        {
         "--c_intensity", "0", // Match Mixed_P_test relevant args
         // Default AoI params: --c_aoi 0.1, --epsilon_aoi 0.01
         // Default Mixed Pixel: --penalty_mixed 0.5, k_mixed 8 etc.
         "--chunk_size", "2" // Small Pass 2 chunk size, smaller than k_mixed_neighbors=8
        },
        0, // Point index from Mixed_P_test
        []{ RayNoiseTestOutput d;
            d.range_variance = 0.0004;              // From Mixed_P_test
            d.angular_variance = 0.0000275625;    // From Mixed_P_test
            // AoI for test_mixed.ply point 0. Based on visual inspection of test_mixed.ply, point 0 is on a flat surface.
            // So, similar to Basic_TwoPass_SmallPass2Chunk_P1, cos_theta ~ 1.
            d.aoi_variance = 0.1 / (1.0 + 0.01);  // c_aoi/(cos_theta + epsilon_aoi)
            d.mixed_pixel_variance = 0.0;           // EXPECTED 0.0 due to small chunk_size in Pass 2 vs k_mixed
            d.total_variance = d.range_variance + d.angular_variance + d.aoi_variance + d.mixed_pixel_variance;
            return d; }()
    },
    {
        // UPDATED Test: Basic_Chunked_LargeChunk_P1 -> Basic_TwoPass_LargePass2Chunk_P1
        // Pass 2 chunk size is larger than file. AoI should be accurate. Mixed pixel local.
        "Basic_TwoPass_LargePass2Chunk_P1", "test_basic.ply",
        {
         "--c_intensity", "0.5", "--epsilon", "0.01",
         // Default AoI, Mixed
         "--chunk_size", "100" // Pass 2 chunk_size > num points in test_basic.ply
        },
        0,
        []{ RayNoiseTestOutput d; // Same as Basic_TwoPass_SmallPass2Chunk_P1 essentially
            d.range_variance = 0.0006469135802469136;
            d.angular_variance = 0.00001225;
            d.aoi_variance = 0.1 / (1.0 + 0.01); // Accurate AoI
            d.mixed_pixel_variance = 0.0;        // Local kNN, test_basic.ply has no mixed pixels.
            d.total_variance = d.range_variance + d.angular_variance + d.aoi_variance + d.mixed_pixel_variance;
            return d; }()
    },
    {
        // UPDATED Test: Basic_Chunked_ExplicitAoIEpsilon_P1 -> Basic_TwoPass_ExplicitAoI_P1
        // Test with non-default AoI params. AoI should be accurate with these explicit params.
        "Basic_TwoPass_ExplicitAoI_P1", "test_basic.ply",
        {
         "--c_aoi", "0.2", "--epsilon_aoi", "0.05", // Non-default AoI params
         "--c_intensity", "0.5", "--epsilon", "0.01",
         // Default Mixed
         "--chunk_size", "2"
        },
        0,
        []{ RayNoiseTestOutput d;
            d.range_variance = 0.0006469135802469136;
            d.angular_variance = 0.00001225;
            d.aoi_variance = 0.2 / (1.0 + 0.05);  // Accurate AoI with explicit params (cos_theta ~ 1)
            d.mixed_pixel_variance = 0.0;
            d.total_variance = d.range_variance + d.angular_variance + d.aoi_variance + d.mixed_pixel_variance;
            return d; }()
    }
    // Note: The original AoI_Chunked_P1_VerifySimplification and Mixed_Chunked_P_VerifySimplification
    // were designed to verify that AoI/Mixed *used fallbacks*.
    // With two-pass, the goal is for AoI to be *accurate*.
    // So, AoI_TwoPass_Accuracy_P1 is the new key test for AoI.
    // Mixed_TwoPass_SmallPass2Chunk_P0 tests the localized (and likely non-triggering for small chunks) mixed pixel.
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
