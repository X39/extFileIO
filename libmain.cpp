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
#define ARG_COUNT_MISSMATCH(EXPECTED) "Argument count missmatch! Expected " STR(EXPECTED) "."
#define LONG_RESULT_KEY_NOT_FOUND "Long Result key unknown or expired."
#define FILE_NOT_FOUND "File provided is not existing."
#define DIRECTORY_NOT_FOUND "Directory provided is not existing or a file."
#define FAILED_TO_OPEN_FILE_FOR_WRITE "Could not open file for write."


using namespace std::string_literals;

class host
{
public:
    static constexpr int exec_ok = 0;
    static constexpr int exec_err = -1;
    static constexpr int exec_more = 1;
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
        bool is_done() const { return value.length() > index; }
    };
    using func = std::function<sqf::value(std::vector<sqf::value>)>;

    std::unordered_map<std::string, func> m_map;
    std::vector<long_result> m_long_results;
    size_t m_long_result_keys;

    host(std::unordered_map<std::string, func> map) : m_long_result_keys(0), m_map(map)
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
            return {};
        }
        return it->second(values);
    }

public:
    static sqf::value err(std::string str)
    {
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

            size_t key = (size_t)(values[0].as<float>());
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
                return exec_ok;
            }
            else
            {
                return exec_more;
            }
        }
        else
        {
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
                return exec_ok;
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
    host::instance().execute(output, outputSize, function, argv, argc);

    return 0;
}

host& host::instance()
{
    static host h({
        { "read_file", [](std::vector<sqf::value> values) -> sqf::value {
            if (values.size() != 1) { return host::err(ARG_COUNT_MISSMATCH(1)); }
            std::filesystem::path fpath = values[0].as<std::string>();
            if (!std::filesystem::exists(fpath)) { return host::err(FILE_NOT_FOUND); }
            if (std::filesystem::is_directory(fpath)) { return host::err(FILE_NOT_FOUND); }
            std::ifstream file(fpath);
            if (!file.is_open() || !file.good()) { return host::err(FILE_NOT_FOUND); }
            std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            return contents;
        } },
        { "list_dir", [](std::vector<sqf::value> values) -> sqf::value {
            if (values.size() != 1) { return host::err(ARG_COUNT_MISSMATCH(1)); }
            std::filesystem::path fpath = values[0].as<std::string>();
            if (!std::filesystem::exists(fpath)) { return host::err(DIRECTORY_NOT_FOUND); }
            if (!std::filesystem::is_directory(fpath)) { return host::err(DIRECTORY_NOT_FOUND); }
            std::vector<sqf::value> paths;
            
            for (auto it = std::filesystem::recursive_directory_iterator(fpath, std::filesystem::directory_options::skip_permission_denied);
                it != std::filesystem::recursive_directory_iterator();
                ++it)
            {
                paths.push_back({
                    std::filesystem::is_directory(it->path()) ? "d"s : "f"s,
                    it->path().string()
                    });
            }
            return paths;
        } },
        { "write_truncate", [](std::vector<sqf::value> values) -> sqf::value {
            if (values.size() != 2) { return host::err(ARG_COUNT_MISSMATCH(2)); }
            std::filesystem::path fpath = values[0].as<std::string>();
            if (std::filesystem::is_directory(fpath)) { return host::err(FILE_NOT_FOUND); }
            auto contents = values[1].as<std::string>();

            std::ofstream file(fpath, std::ios::trunc | std::ios::out);

            if (!file.good())
            {
                return host::err(FAILED_TO_OPEN_FILE_FOR_WRITE);
            }

            file << contents;
            file.flush();
            file.close();

            return {};
        } },
        { "write_append", [](std::vector<sqf::value> values) -> sqf::value {
            if (values.size() != 2) { return host::err(ARG_COUNT_MISSMATCH(2)); }
            std::filesystem::path fpath = values[0].as<std::string>();
            if (std::filesystem::is_directory(fpath)) { return host::err(FILE_NOT_FOUND); }
            auto contents = values[1].as<std::string>();

            std::ofstream file(fpath, std::ios::app | std::ios::out);

            if (!file.good())
            {
                return host::err(FAILED_TO_OPEN_FILE_FOR_WRITE);
            }

            file << contents;
            file.flush();
            file.close();

            return {};
        } }
    });
    return h;
}




int main()
{
    const size_t size = 10000;
    char buff[size];

    const size_t args1_size = 1;
    const char* args1[args1_size] = {
        "\"external\""
    };
    RVExtensionArgs(buff, size, "list_dir", args1, args1_size);

    const size_t args2_size = 1;
    const char* args2[args2_size] = {
        "1"
    };
    RVExtensionArgs(buff, size, "?", args2, args2_size);
    RVExtensionArgs(buff, size, "?", args2, args2_size);
    RVExtensionArgs(buff, size, "?", args2, args2_size);
    RVExtensionArgs(buff, size, "?", args2, args2_size);
    RVExtensionArgs(buff, size, "?", args2, args2_size);
    RVExtensionArgs(buff, size, "?", args2, args2_size);
}