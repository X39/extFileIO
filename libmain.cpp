#if defined(_MSC_BUILD)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <unordered_map>
#include <vector>
#include <list>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <functional>

#include "libmain.hpp"
#include "external/sqf-value/sqf-value/value.h"


#define STR(CONTENT) #CONTENT
#define ARG_COUNT_MISSMATCH(EXPECTED) "Argument count mismatch! Expected " STR(EXPECTED) "."
#define ARG_COUNT_MISSMATCH(MIN, MAX) "Argument count mismatch! Expected at least " STR(MIN) " up to " STR(MAX) "."
#define LONG_RESULT_KEY_NOT_FOUND "Long Result key unknown or expired."
#define FILE_NOT_FOUND "File not found."
#define DIRECTORY_NOT_FOUND "Provided directory doesn't exist or is a file."
#define FAILED_TO_OPEN_FILE_FOR_WRITE "Could not open file for write operation."
#define PATH_EMPTY_FILE "Path provided is empty."
#define PATH_EMPTY_DIR "Path provided is empty. Pass in \".\" to make it list the current direcotry."
#define UNKNOWN_WRITE_MODE "Unknown write mode."
#define UNKNOWN_READ_MODE "Unknown read mode."
#define FILE_ARRAY_DATA_PASSED_NOT_ALL_SCALAR "The data inside of the array passed in was not all scalar."


using namespace std::string_literals;

class host
{
public:
    static constexpr int exec_ok = 0;
    static constexpr int exec_err = -1;
    static constexpr int exec_more = 1;
    static constexpr const char* msg_unknown_method = "Method passed is not known to extension.";
private:
    class long_result
    {
        std::string value;
        size_t index;

    public:
        size_t key;
        long_result(size_t key, std::string str) : value(str), index(0), key(key)
        {

        }
        void next(char* output, size_t size)
        {
            if (size == 0) { return; }

            // move size one back for '\0'
            size--;

            // get start and end of writable value
            auto len = value.length();
            auto start = index;
            auto end = start + size > len ? len : start + size;

            // copy value to output
            strncpy(output, value.data() + start, end - start);

            // Set end to '\0'
            output[end - start] = '\0';

            // set index to end
            index = end;
        }
        bool is_done() const { return value.length() <= index; }
    };
    using func = std::function<sqf::value(std::vector<sqf::value>)>;

    std::unordered_map<std::string, func> m_map;
    std::vector<long_result> m_long_results;
    size_t m_long_result_keys;

    host(std::unordered_map<std::string, func> map) : m_long_result_keys(0), m_map(map), has_errord(false)
    {
    }

    static void err(std::string s, char* output, size_t output_size)
    {
        sqf::value val = s;
        auto str = val.to_string();
        strncpy(output, str.data(), str.length());
        output[str.length()] = '\0';
    }

    sqf::value execute(std::string function, std::vector<sqf::value> values)
    {
        auto it = m_map.find(function);
        if (it == m_map.end())
        {
            return err(msg_unknown_method);
        }
        return it->second(values);
    }
    bool has_errord;
public:
    static sqf::value err(std::string str)
    {
        instance().has_errord = true;
        return str;
    }
    static host& instance();
    
    int execute(char* output, int outputSize, const char* in_function, const char** argv, int argc)
    {
        std::string function(in_function);
        std::vector<sqf::value> values;
        for (size_t i = 0; i < argc; i++)
        {
            values.push_back(sqf::value::parse(argv[i]));
        }
        if (function == "?")
        {
            if (values.size() != 1)
            {
                err(ARG_COUNT_MISSMATCH(1), output, outputSize);
                return exec_err;
            }

            size_t key = (size_t)(float(values[0]));
            auto lr = std::find_if(
                m_long_results.begin(),
                m_long_results.end(),
                [key](long_result& res) -> bool { return res.key == key; });

            if (lr == m_long_results.end())
            {
                err(LONG_RESULT_KEY_NOT_FOUND, output, outputSize);
                return exec_err;
            }
            lr->next(output, outputSize);
            if (lr->is_done())
            {
                m_long_results.erase(lr);
                return has_errord ? exec_err : exec_ok;
            }
            else
            {
                return exec_more;
            }
        }
        else
        {
            has_errord = false;
            auto res = host::instance().execute(function, values);
            auto result = res.to_string();

            if (result.length() + 1 > outputSize)
            {
                auto key = ++m_long_result_keys;
                m_long_results.emplace_back(key, result);
                auto key_string = sqf::value((float)key).to_string();
                strncpy(output, key_string.data(), key_string.length());
                output[key_string.length()] = '\0';
                return exec_more;
            }
            else
            {
                strncpy(output, result.data(), result.length());
                output[result.length()] = '\0';
                return has_errord ? exec_err : exec_ok;
            }
        }
    }
};


DLLEXPORT void STDCALL RVExtensionVersion(char* output, int outputSize)
{

}

DLLEXPORT void STDCALL RVExtension(char* output, int outputSize, const char* function)
{
    if (outputSize > 0)
    {
        output[0] = '\0';
    }
}

DLLEXPORT int STDCALL RVExtensionArgs(char* output, int outputSize, const char* function, const char** argv, int argc)
{
    auto res = host::instance().execute(output, outputSize, function, argv, argc);
    return res;
}

host& host::instance()
{
    static host h({
        { "read", [](std::vector<sqf::value> values) -> sqf::value {
            // Check arg count
            if (values.size() < 1 || values.size() > 2) { return host::err(ARG_COUNT_MISSMATCH(1, 2)); }

            // Get file path (arg0)
            if (std::string(values[0]).empty()) { return host::err(PATH_EMPTY_FILE); }
            std::filesystem::path fpath = std::string(values[0]);
            fpath = std::filesystem::absolute(fpath);

            // ensure file is not directory and exists
            if (!std::filesystem::exists(fpath)) { return host::err(FILE_NOT_FOUND); }
            if (std::filesystem::is_directory(fpath)) { return host::err(FILE_NOT_FOUND); }

            // Get mode (arg1) ("i", "b")
            std::ios::openmode mode = std::ios::in;
            if (values.size() >= 1)
            {
                auto modes = std::string(values[1]);
                for (auto c : modes)
                {
                    switch (c)
                    {
                    case 'i':
                    case 'I':
                        mode = mode | std::ios::in;
                        break;
                    case 'b':
                    case 'B':
                        mode = mode | std::ios::binary;
                        break;
                    default:
                        return host::err(UNKNOWN_READ_MODE);
                        break;
                    }
                }
            }

            // read in file
            std::ifstream file(fpath, mode);
            if (!file.is_open() || !file.good()) { return host::err(FILE_NOT_FOUND); }
            std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

            // return array if binary mode was supplied
            if ((mode & std::ios::binary) == std::ios::binary)
            {
                std::vector<sqf::value> out;
                out.reserve(contents.length());
                for (auto c : contents)
                {
                    out.push_back(sqf::value((float)c));
                }
                return out;
            }
            else
            {
                return contents;
            }
        } },
        { "list", [](std::vector<sqf::value> values) -> sqf::value {
            if (values.size() < 1 || values.size() > 2) { return host::err(ARG_COUNT_MISSMATCH(1, 2)); }

            if (std::string(values[0]).empty()) { return host::err(PATH_EMPTY_DIR); }
            std::filesystem::path fpath = std::string(values[0]);
            fpath = fpath.lexically_normal();


            std::string ends_with_filter = "";
            if (values.size() >= 2)
            {
                ends_with_filter = std::string(values[1]);
                std::transform(ends_with_filter.begin(), ends_with_filter.end(), ends_with_filter.begin(),
                    [](char c) -> char { return (char)std::tolower(c); });
            }

            if (!std::filesystem::exists(fpath)) { return host::err(DIRECTORY_NOT_FOUND); }
            if (!std::filesystem::is_directory(fpath)) { return host::err(DIRECTORY_NOT_FOUND); }

            std::vector<sqf::value> paths;

            for (auto it = std::filesystem::recursive_directory_iterator(fpath, std::filesystem::directory_options::skip_permission_denied);
                it != std::filesystem::recursive_directory_iterator();
                ++it)
            {
                auto str = std::filesystem::absolute(it->path()).string();
                auto str_low = str;
                std::transform(str_low.begin(), str_low.end(), str_low.begin(),
                    [](char c) -> char { return (char)std::tolower(c); });

                if (str.length() < ends_with_filter.length()) { continue; }
                if (!ends_with_filter.empty() &&
                    0 != str_low.compare(
                    str_low.length() - ends_with_filter.length(),
                    ends_with_filter.length(),
                    ends_with_filter)) { continue; }

                paths.push_back({
                    std::filesystem::is_directory(it->path()) ? "d"s : "f"s,
                    str
                    });
            }
            return paths;
        } },
        { "write", [](std::vector<sqf::value> values) -> sqf::value {
            // Check arg count
            if (values.size() != 3) { return host::err(ARG_COUNT_MISSMATCH(3)); }

            // Get file path (arg0)
            if (std::string(values[0]).empty()) { return host::err(PATH_EMPTY_FILE); }
            std::filesystem::path fpath = std::string(values[0]);
            fpath = std::filesystem::absolute(fpath);

            // ensure file is not directory
            if (std::filesystem::is_directory(fpath)) { return host::err(FILE_NOT_FOUND); }

            // Get mode (arg1) ("t", "a", "b", "o")
            auto modes = std::string(values[1]);
            std::ios::openmode mode = std::ios::out;
            for (auto c : modes)
            {
                switch (c)
                {
                case 'o':
                case 'O':
                    mode = mode | std::ios::out;
                    break;
                case 't':
                case 'T':
                    mode = (mode | std::ios::trunc) & ~std::ios::app;
                    break;
                case 'a':
                case 'A':
                    mode = (mode | std::ios::app) & ~std::ios::trunc;
                    break;
                case 'b':
                case 'B':
                    mode = mode | std::ios::binary;
                    break;
                default:
                    return host::err(UNKNOWN_WRITE_MODE);
                    break;
                }
            }

            // open fstream
            std::ofstream file(fpath, mode);
            if (!file.good())
            {
                return host::err(FAILED_TO_OPEN_FILE_FOR_WRITE);
            }

            // Get contents to write (arg2)
            std::string contents;
            if (values[2].is_array())
            {
                auto arr = std::vector<sqf::value>(values[2]);
                auto out = std::vector<char>();
                out.reserve(arr.size());
                for (auto val : arr)
                {
                    if (!val.is_scalar())
                    {
                        return host::err(FILE_ARRAY_DATA_PASSED_NOT_ALL_SCALAR);
                    }
                    out.push_back((char)(int)float(val));
                }
                file.write(out.data(), out.size());
            }
            else
            {
                auto contents = std::string(values[2]);

                file << contents;
            }

            file.flush();
            file.close();

            return {};
        } }
    });
    return h;
}



#if defined(_DEBUG)
#include <array>
#include <iostream>

size_t strlen(const char* str, size_t max)
{
    size_t i;
    for (i = 0; i < max && str[i] != '\0'; i++) { }

    if (i == max)
    {
        std::cerr << "UNTERMINATED C-STYLE STRING DETECTED" << std::endl;
    }
    return i;
}
template<size_t buff_size, size_t data_size>
void execute(std::string function, std::array<sqf::value, data_size> data)
{
    const char* in_data[data_size];
    char* in_function = const_cast<char*>(function.c_str());
    char buff[buff_size];
    std::array<std::string, data_size> strings;

    std::cout << "Executing '" << function << "' with {";

    for (size_t i = 0; i < data_size; i++)
    {
        strings[i] = data[i].to_string();
        if (i != 0)
        {
            std::cout << ", ";
        }
        std::cout << " " << strings[i];
        in_data[i] = strings[i].c_str();
    }
    std::cout << " }" << std::endl;
    auto res = RVExtensionArgs(buff, buff_size, in_function, in_data, data_size);
    std::cout << "Result: " << res << "\n" <<
        "Length: " << strlen(buff, buff_size) << "\n";
    if (res == host::exec_more) { std::cout << buff << std::endl; }
    else { std::cout << sqf::value::parse(buff).to_string(false) << std::endl; }

    auto orig_res = std::string(buff, strlen(buff, buff_size));
    const char* in_orig_res[1] = { orig_res.c_str() };
    while (res == host::exec_more)
    {
        res = RVExtensionArgs(buff, buff_size, "?", in_orig_res, 1);
        std::cout << "Result: " << res << "\n" <<
                     "Length: " << strlen(buff, buff_size) << "\n" <<
                     buff << std::endl;
    }
    std::cout << std::endl;
}
int main()
{
    using namespace std::string_literals;
    using namespace sqf;
    // Will error and return "more"
    execute<50>("list_dir"s, std::array{ "\"\""sqf });
    // Will return array of current working dir
    execute<1000>("list_dir"s, std::array{ "'.'"sqf, "'.cpp'"sqf });
    execute<1000>("write"s, std::array{ "'truncate.bin'"sqf, "'tb'"sqf, "[65]"sqf });
    execute<1000>("write"s, std::array{ "'truncate.bin'"sqf, "'tb'"sqf, "[66]"sqf });
    execute<1000>("write"s, std::array{ "'truncate.bin'"sqf, "'tb'"sqf, "[67]"sqf });
    execute<1000>("write"s, std::array{ "'append.bin'"sqf, "'ab'"sqf, "[65]"sqf });
    execute<1000>("write"s, std::array{ "'append.bin'"sqf, "'ab'"sqf, "[66]"sqf });
    execute<1000>("write"s, std::array{ "'append.bin'"sqf, "'ab'"sqf, "[67]"sqf });
    execute<1000>("write"s, std::array{ "'truncate.txt'"sqf, "'t'"sqf, "'A'"sqf });
    execute<1000>("write"s, std::array{ "'truncate.txt'"sqf, "'t'"sqf, "'B'"sqf });
    execute<1000>("write"s, std::array{ "'truncate.txt'"sqf, "'t'"sqf, "'C'"sqf });
    execute<1000>("write"s, std::array{ "'append.txt'"sqf, "'a'"sqf, "'A'"sqf });
    execute<1000>("write"s, std::array{ "'append.txt'"sqf, "'a'"sqf, "'B'"sqf });
    execute<1000>("write"s, std::array{ "'append.txt'"sqf, "'a'"sqf, "'C'"sqf });
}
#endif