#include "raynoise_test_utils.h"
#include <fstream>
#include <sstream>
#include <iostream> // For cerr
#include <vector>
#include <map>
#include <algorithm> // For std::find
#include <cstring>   // For std::memcpy

// Helper to get type size
size_t getPlyTypeSize(const std::string& type_str) {
    if (type_str == "float" || type_str == "f4" || type_str == "float32") return 4;
    if (type_str == "double" || type_str == "f8" || type_str == "float64") return 8;
    if (type_str == "char" || type_str == "i1" || type_str == "int8") return 1;
    if (type_str == "uchar" || type_str == "u1" || type_str == "uint8") return 1;
    if (type_str == "short" || type_str == "i2" || type_str == "int16") return 2;
    if (type_str == "ushort" || type_str == "u2" || type_str == "uint16") return 2;
    if (type_str == "int" || type_str == "i4" || type_str == "int32") return 4;
    if (type_str == "uint" || type_str == "u4" || type_str == "uint32") return 4;
    return 0;
}

struct PlyProperty {
    std::string name;
    std::string type_str;
    size_t size;
    size_t offset;
    bool is_list;

    PlyProperty(std::string n, std::string t, size_t s, size_t o) :
        name(std::move(n)), type_str(std::move(t)), size(s), offset(o), is_list(false) {}
};

bool parsePlyHeader(std::ifstream& ifs, size_t& num_vertices, std::vector<PlyProperty>& properties, size_t& vertex_byte_size, bool& is_binary_le) {
    std::string line;
    bool in_vertex_element = false;
    vertex_byte_size = 0;
    is_binary_le = false;
    num_vertices = 0;
    properties.clear();

    if (!ifs.is_open()) {
        std::cerr << "Error: [parsePlyHeader] File stream not open." << std::endl;
        return false;
    }

    std::getline(ifs, line);
    if (line.rfind("ply", 0) != 0) { // Check if line starts with "ply"
        std::cerr << "Error: Not a PLY file or missing 'ply' magic number. Line: " << line << std::endl;
        return false;
    }

    while (std::getline(ifs, line)) {
        // Trim whitespace (especially trailing \r on Windows)
        line.erase(line.find_last_not_of(" \n\r\t")+1);

        std::stringstream ss(line);
        std::string keyword;
        ss >> keyword;

        if (keyword == "format") {
            std::string format_type;
            ss >> format_type;
            if (format_type == "binary_little_endian") {
                is_binary_le = true;
            } else if (format_type == "ascii") {
                 std::cerr << "Error: ASCII PLY format not supported by this parser for raynoise output." << std::endl;
                return false;
            }
        } else if (keyword == "element") {
            std::string element_name;
            ss >> element_name;
            if (element_name == "vertex") {
                in_vertex_element = true;
                ss >> num_vertices;
            } else {
                in_vertex_element = false;
            }
        } else if (keyword == "property") {
            if (in_vertex_element) {
                std::string type, name;
                ss >> type;
                if (type == "list") {
                    std::cerr << "Warning: List properties not handled by this parser. Skipping: " << line << std::endl;
                    continue;
                }
                ss >> name;
                size_t type_size = getPlyTypeSize(type);
                if (type_size == 0) {
                    std::cerr << "Warning: Unknown property type '" << type << "' for property '" << name << "'. Skipping." << std::endl;
                    continue;
                }
                properties.emplace_back(name, type, type_size, vertex_byte_size);
                vertex_byte_size += type_size;
            }
        } else if (keyword == "end_header") {
            break;
        }
    }

    if (!is_binary_le) {
        std::cerr << "Error: PLY file is not in binary_little_endian format." << std::endl;
        return false;
    }
    if (properties.empty() && num_vertices > 0) { // Allow properties to be empty if num_vertices is 0
         std::cerr << "Error: No vertex properties found in header for non-empty cloud." << std::endl;
         return false;
    }
    return true;
}

template<typename T>
T readValueFromBuffer(const char* buffer, size_t offset) {
    T value;
    std::memcpy(&value, buffer + offset, sizeof(T));
    return value;
}

bool parseRayNoiseOutputPly(const std::string& filePath, int pointIndex, RayNoiseTestOutput& out_data) {
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs.is_open()) {
        std::cerr << "Error: Cannot open file " << filePath << std::endl;
        return false;
    }

    size_t num_vertices = 0;
    std::vector<PlyProperty> properties;
    size_t vertex_byte_size = 0;
    bool is_binary_le = false;
    long header_end_pos = 0;


    if (!parsePlyHeader(ifs, num_vertices, properties, vertex_byte_size, is_binary_le)) {
        return false;
    }
    header_end_pos = ifs.tellg(); // Get position after header

    if (num_vertices == 0) {
         std::cerr << "Info: PLY file '" << filePath << "' has 0 vertices. Cannot parse point data." << std::endl;
         return false;
    }

    if (pointIndex < 0 || static_cast<size_t>(pointIndex) >= num_vertices) {
        std::cerr << "Error: Point index " << pointIndex << " is out of bounds (0-" << (num_vertices > 0 ? num_vertices -1 : 0) << ") in file " << filePath << "." << std::endl;
        return false;
    }
    if (vertex_byte_size == 0 && num_vertices > 0) {
        std::cerr << "Error: Vertex byte size is 0, cannot read point data from " << filePath << "." << std::endl;
        return false;
    }


    ifs.seekg(header_end_pos + (static_cast<long>(pointIndex) * static_cast<long>(vertex_byte_size)));

    std::vector<char> buffer(vertex_byte_size);
    ifs.read(buffer.data(), vertex_byte_size);
    if (ifs.gcount() != static_cast<std::streamsize>(vertex_byte_size)) {
        std::cerr << "Error: Failed to read vertex data for point " << pointIndex << " from " << filePath << ". Expected " << vertex_byte_size << " bytes, got " << ifs.gcount() << "." << std::endl;
        return false;
    }

    std::map<std::string, const PlyProperty*> prop_map;
    for(const auto& prop : properties) {
        prop_map[prop.name] = &prop;
    }

    auto get_prop_val = [&](const std::string& name, auto& val_ref) -> bool {
        auto it = prop_map.find(name);
        if (it == prop_map.end()) {
            std::cerr << "Error: Property '" << name << "' not found in PLY header of " << filePath << "." << std::endl;
            return false;
        }
        const PlyProperty* p = it->second;
        using ValType = std::remove_reference_t<decltype(val_ref)>;
        if (p->size != sizeof(ValType) && !(std::is_same_v<ValType, float> && p->type_str == "double") && !(std::is_same_v<ValType, double> && p->type_str == "float")) {
             // Allow reading float as double or vice versa with appropriate cast, but check other size mismatches.
             // This simple parser assumes types match struct definitions mostly.
        }
        val_ref = readValueFromBuffer<ValType>(buffer.data(), p->offset);
        return true;
    };

#if RAYLIB_DOUBLE_RAYS
    if (!get_prop_val("x", out_data.x)) return false;
    if (!get_prop_val("y", out_data.y)) return false;
    if (!get_prop_val("z", out_data.z)) return false;
#else
    if (!get_prop_val("x", out_data.x)) return false;
    if (!get_prop_val("y", out_data.y)) return false;
    if (!get_prop_val("z", out_data.z)) return false;
#endif

    if (!get_prop_val("time", out_data.time)) return false;

    if (! ( (get_prop_val("nx", out_data.nx) && get_prop_val("ny", out_data.ny) && get_prop_val("nz", out_data.nz)) ||
            (get_prop_val("rayx", out_data.nx) && get_prop_val("rayy", out_data.ny) && get_prop_val("rayz", out_data.nz)) ) ) {
        std::cerr << "Error: Ray vector properties (nx/ny/nz or rayx/rayy/rayz) not found in " << filePath << "." << std::endl;
        return false;
    }

    if (!get_prop_val("red",   out_data.red)) return false;
    if (!get_prop_val("green", out_data.green)) return false;
    if (!get_prop_val("blue",  out_data.blue)) return false;
    if (!get_prop_val("alpha", out_data.alpha)) return false;

    if (!get_prop_val("total_variance", out_data.total_variance)) return false;
    if (!get_prop_val("range_variance", out_data.range_variance)) return false;
    if (!get_prop_val("angular_variance", out_data.angular_variance)) return false;
    if (!get_prop_val("aoi_variance", out_data.aoi_variance)) return false;
    if (!get_prop_val("mixed_pixel_variance", out_data.mixed_pixel_variance)) return false;

    return true;
}


bool parseAllRayNoiseOutputPly(const std::string& filePath, std::vector<RayNoiseTestOutput>& out_data_vector) {
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs.is_open()) {
        std::cerr << "Error: Cannot open file " << filePath << std::endl;
        return false;
    }

    size_t num_vertices = 0;
    std::vector<PlyProperty> properties;
    size_t vertex_byte_size = 0;
    bool is_binary_le = false;
    long header_end_pos = 0;

    if (!parsePlyHeader(ifs, num_vertices, properties, vertex_byte_size, is_binary_le)) {
        return false;
    }
    header_end_pos = ifs.tellg();

    if (num_vertices == 0) {
        out_data_vector.clear();
        return true;
    }
    if (vertex_byte_size == 0) {
         std::cerr << "Error: Vertex byte size is 0, cannot read point data from " << filePath << "." << std::endl;
        return false;
    }

    out_data_vector.resize(num_vertices);
    std::vector<char> buffer(vertex_byte_size);

    std::map<std::string, const PlyProperty*> prop_map;
    for(const auto& prop : properties) {
        prop_map[prop.name] = &prop;
    }
    auto get_prop_val = [&](const char* buf, const std::string& name, auto& val_ref) -> bool {
        auto it = prop_map.find(name);
        if (it == prop_map.end()) { return false; } // Property not found in this file
        const PlyProperty* p = it->second;
        using ValType = std::remove_reference_t<decltype(val_ref)>;
        val_ref = readValueFromBuffer<ValType>(buf, p->offset);
        return true;
    };


    for (size_t i = 0; i < num_vertices; ++i) {
        ifs.seekg(header_end_pos + (static_cast<long>(i) * static_cast<long>(vertex_byte_size)));
        ifs.read(buffer.data(), vertex_byte_size);
        if (ifs.gcount() != static_cast<std::streamsize>(vertex_byte_size)) {
            std::cerr << "Error: Failed to read vertex data for point " << i << " from " << filePath << "." << std::endl;
            out_data_vector.clear();
            return false;
        }

        RayNoiseTestOutput current_point_data;
        bool success = true;

#if RAYLIB_DOUBLE_RAYS
        success &= get_prop_val(buffer.data(), "x", current_point_data.x);
        success &= get_prop_val(buffer.data(), "y", current_point_data.y);
        success &= get_prop_val(buffer.data(), "z", current_point_data.z);
#else
        success &= get_prop_val(buffer.data(), "x", current_point_data.x);
        success &= get_prop_val(buffer.data(), "y", current_point_data.y);
        success &= get_prop_val(buffer.data(), "z", current_point_data.z);
#endif
        success &= get_prop_val(buffer.data(), "time", current_point_data.time);

        if (!( (get_prop_val(buffer.data(), "nx", current_point_data.nx) && get_prop_val(buffer.data(), "ny", current_point_data.ny) && get_prop_val(buffer.data(), "nz", current_point_data.nz)) ||
               (get_prop_val(buffer.data(), "rayx", current_point_data.nx) && get_prop_val(buffer.data(), "rayy", current_point_data.ny) && get_prop_val(buffer.data(), "rayz", current_point_data.nz)) )) {
            success = false; // Ray vector properties not found
        }

        success &= get_prop_val(buffer.data(), "red",   current_point_data.red);
        success &= get_prop_val(buffer.data(), "green", current_point_data.green);
        success &= get_prop_val(buffer.data(), "blue",  current_point_data.blue);
        success &= get_prop_val(buffer.data(), "alpha", current_point_data.alpha);

        success &= get_prop_val(buffer.data(), "total_variance", current_point_data.total_variance);
        success &= get_prop_val(buffer.data(), "range_variance", current_point_data.range_variance);
        success &= get_prop_val(buffer.data(), "angular_variance", current_point_data.angular_variance);
        success &= get_prop_val(buffer.data(), "aoi_variance", current_point_data.aoi_variance);
        success &= get_prop_val(buffer.data(), "mixed_pixel_variance", current_point_data.mixed_pixel_variance);

        if (!success) {
            std::cerr << "Error: Failed to parse all required properties for point " << i << " from " << filePath << "." << std::endl;
            out_data_vector.clear();
            return false;
        }
        out_data_vector[i] = current_point_data;
    }
    return true;
}
