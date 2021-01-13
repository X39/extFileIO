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
#include "external/sqf-value/sqf-value/value.hpp"
#include "external/sqf-value/sqf-value/methodhost.hpp"


#define STR(CONTENT) #CONTENT
#define ARG_COUNT_MISSMATCH(EXPECTED) "Argument count mismatch! Expected " STR(EXPECTED) "."
#define ARG_COUNT_MISSMATCH(MIN, MAX) "Argument count mismatch! Expected at least " STR(MIN) " up to " STR(MAX) "."
#define LONG_RESULT_KEY_NOT_FOUND "Long Result key unknown or expired."
#define FILE_NOT_FOUND "File not found."
#define FILE_WITH_NAME_ALREADY_EXISTS_FOUND "File already exists."
#define DIRECTORY_NOT_FOUND "Provided directory doesn't exist or is a file."
#define FAILED_TO_OPEN_FILE_FOR_WRITE "Could not open file for write operation."
#define SAME_PATH_PROVIDED "Paths provided is the same."
#define PATH_EMPTY "Path provided is empty."
#define PATH_EMPTY_DIR "Path provided is empty. Pass in \".\" to make it list the current direcotry."
#define UNKNOWN_WRITE_MODE "Unknown write mode."
#define UNKNOWN_READ_MODE "Unknown read mode."
#define FILE_ARRAY_DATA_PASSED_NOT_ALL_SCALAR "The data inside of the array passed in was not all scalar."


using namespace std::string_literals;


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
    auto res = sqf::methodhost::instance().execute(output, outputSize, function, argv, argc);
    return res;
}

using ret_d = sqf::method::ret<sqf::value, std::string>;
ret_d fnc_impl_read(std::string filepath, std::optional<std::string> modifiers)
{
    // Get file path (arg0)
    if (filepath.empty()) { return ret_d::err(PATH_EMPTY); }
    std::filesystem::path fpath = std::filesystem::absolute(filepath);

    // ensure file is not directory and exists
    if (!std::filesystem::exists(fpath)) { return ret_d::err(FILE_NOT_FOUND);; }
    if (std::filesystem::is_directory(fpath)) { return ret_d::err(FILE_NOT_FOUND);; }

    // Get mode (arg1) ("i", "b")
    std::ios::openmode mode = std::ios::in;
    if (modifiers.has_value())
    {
        for (auto c : *modifiers)
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
                return ret_d::err(UNKNOWN_READ_MODE);
                break;
            }
        }
    }

    // read in file
    std::ifstream file(fpath, mode);
    if (!file.is_open() || !file.good()) { return ret_d::err(FILE_NOT_FOUND);; }
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
        return ret_d::ok(out);
    }
    else
    {
        return ret_d::ok(contents);
    }
}
ret_d fnc_impl_write_str(std::string filepath, bool append, std::string contents)
{
    // Get file path (arg0)
    if (filepath.empty()) { return ret_d::err(PATH_EMPTY); }
    std::filesystem::path fpath = std::filesystem::absolute(filepath);

    // ensure file is not directory
    if (std::filesystem::is_directory(fpath)) { return ret_d::err(FILE_NOT_FOUND);; }

    std::ios::openmode mode = append ? (std::ios::out | std::ios::app) : (std::ios::out | std::ios::trunc);

    // open fstream
    std::ofstream file(fpath, mode);
    if (!file.good()) { return { {}, FAILED_TO_OPEN_FILE_FOR_WRITE }; }

    // Get contents to write (arg2)
    file << contents;

    file.flush();
    file.close();

    return ret_d::ok({});
}
ret_d fnc_impl_write_vec(std::string filepath, bool append, std::vector<sqf::value> contents)
{
    // Get file path (arg0)
    if (filepath.empty()) { return ret_d::err(PATH_EMPTY); }
    std::filesystem::path fpath = std::filesystem::absolute(filepath);

    // ensure file is not directory
    if (std::filesystem::is_directory(fpath)) { return ret_d::err(FILE_NOT_FOUND);; }

    std::ios::openmode mode = append ? (std::ios::out | std::ios::app) : (std::ios::out | std::ios::trunc);

    // open fstream
    std::ofstream file(fpath, mode);
    if (!file.good()) { return { {}, FAILED_TO_OPEN_FILE_FOR_WRITE }; }

    // Get contents to write (arg2)
    auto out = std::vector<char>();
    out.reserve(contents.size());
    for (auto val : contents)
    {
        if (!val.is_scalar()) { return { {},FILE_ARRAY_DATA_PASSED_NOT_ALL_SCALAR }; }
        out.push_back((char)(int)float(val));
    }
    file.write(out.data(), out.size());

    file.flush();
    file.close();

    return ret_d::ok({});
}
ret_d fnc_impl_list(std::string filepath, std::optional<std::string> filter)
{
    if (filepath.empty()) { return { {}, PATH_EMPTY_DIR }; }
    std::filesystem::path fpath = std::filesystem::absolute(filepath);

    std::string ends_with_filter = filter.has_value() ? filter.value() : ""s;
    std::transform(ends_with_filter.begin(), ends_with_filter.end(), ends_with_filter.begin(),
        [](char c) -> char { return (char)std::tolower(c); });

    if (!std::filesystem::exists(fpath)) { return  { {}, DIRECTORY_NOT_FOUND }; }
    if (!std::filesystem::is_directory(fpath)) { return  { {}, DIRECTORY_NOT_FOUND }; }

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
                ends_with_filter))
        {
            continue;
        }

        paths.push_back({
            std::filesystem::is_directory(it->path()) ? "d"s : "f"s,
            str
            });
    }
    return ret_d::ok(paths);
}
ret_d fnc_impl_copy(std::string current_name, std::string new_name)
{
    // Get file path
    if (current_name.empty()) { return ret_d::err(PATH_EMPTY); }
    if (new_name.empty()) { return ret_d::err(PATH_EMPTY); }
    std::filesystem::path path_current = std::filesystem::absolute(current_name);
    std::filesystem::path path_new = std::filesystem::absolute(new_name);

    if (path_current == path_new) { return ret_d::err(SAME_PATH_PROVIDED); }

    // ensure current exists
    if (!std::filesystem::exists(path_current)) { return ret_d::err(FILE_NOT_FOUND);; }

    // ensure new does not exists
    if (std::filesystem::exists(path_current)) { return ret_d::err(FILE_WITH_NAME_ALREADY_EXISTS_FOUND); }

    std::filesystem::copy(path_current, path_new);

    return ret_d::ok({});
}
ret_d fnc_impl_rename(std::string current_name, std::string new_name)
{
    // Get file path
    if (current_name.empty()) { return ret_d::err(PATH_EMPTY); }
    if (new_name.empty()) { return ret_d::err(PATH_EMPTY); }
    std::filesystem::path path_current = std::filesystem::absolute(current_name);
    std::filesystem::path path_new = std::filesystem::absolute(new_name);

    if (path_current == path_new) { return ret_d::err(SAME_PATH_PROVIDED); }

    // ensure current exists
    if (!std::filesystem::exists(path_current)) { return ret_d::err(FILE_NOT_FOUND); }

    // ensure new does not exists
    if (std::filesystem::exists(path_current)) { return ret_d::err(FILE_WITH_NAME_ALREADY_EXISTS_FOUND); }

    std::filesystem::rename(path_current, path_new);

    return ret_d::ok({});
}
ret_d fnc_impl_delete(std::string filename)
{
    // Get file path
    if (filename.empty()) { return ret_d::err(PATH_EMPTY); }
    std::filesystem::path fpath = std::filesystem::absolute(filename);

    // ensure current exists
    if (std::filesystem::exists(filename)) { return ret_d::err(FILE_NOT_FOUND); }

    if (std::filesystem::is_directory(filename))
    {
        std::filesystem::remove_all(fpath);
    }
    else
    {
        std::filesystem::remove(fpath);
    }

    return ret_d::ok({});
}

sqf::methodhost& sqf::methodhost::instance()
{
    using namespace std::string_literals;
    // We could use lambdas for this ...
    // but auto-formatters for some reason just refuse
    // to work nice with the whole initializer list stuff
    static sqf::methodhost h({
        { "read", { sqf::method::create(fnc_impl_read) } },
        { "write", { sqf::method::create(fnc_impl_write_str), sqf::method::create(fnc_impl_write_vec) } },
        { "list", { sqf::method::create(fnc_impl_list) } },
        { "copy", { sqf::method::create(fnc_impl_copy) } },
        { "rename", { sqf::method::create(fnc_impl_rename) } },
        { "delete", { sqf::method::create(fnc_impl_delete) } }
        });
    return h;
}



#if defined(_DEBUG)
#include <array>
#include <iostream>

size_t strlen(const char* str, size_t max)
{
    size_t i;
    for (i = 0; i < max && str[i] != '\0'; i++) {}

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
    if (res == sqf::methodhost::exec_more) { std::cout << buff << std::endl; }
    else { std::cout << sqf::value::parse(buff).to_string(false) << std::endl; }

    auto orig_res = std::string(buff, strlen(buff, buff_size));
    const char* in_orig_res[1] = { orig_res.c_str() };
    while (res == sqf::methodhost::exec_more)
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

    auto m1 = sqf::method::create(
        [](std::string path, std::optional<std::string> mode) -> method::ret<bool, std::string> {
            return method::ret<bool, std::string>::err("abc");
        });
    auto m2 = sqf::method::create(
        [](std::string path, std::optional<std::string> mode) -> method::ret<bool, std::string> {
            return method::ret<bool, std::string>::ok(mode.has_value());
        });
    auto m3 = sqf::method::create(
        [](std::string path, std::optional<std::string> mode) -> bool {
            return mode.has_value();
        });
    std::cout << "can_call 1: " << m1.can_call({ "'test'"_sqf }) << "\n" <<
        "can_call 2: " << m1.can_call({ "'test'"_sqf, "'test'"_sqf }) << "\n" <<
        "can_call 3: " << m1.can_call({ "'test'"_sqf, "'test'"_sqf, "'test'"_sqf }) << "\n" <<
        "can_call 4: " << m1.can_call({ "'test'"_sqf, "2"_sqf }) << "\n" <<
        "can_call 5: " << m1.can_call({ "2"_sqf, "2"_sqf }) << "\n" <<
        "can_call 6: " << m1.can_call({ "2"_sqf }) << "\n";

    auto call1_res = m1.call_generic({ "'test'"_sqf });
    std::cout << "call 1: " << (call1_res.is_ok() ? "ok" : "err") << " - " <<
        (call1_res.is_ok() ? call1_res.get_ok().to_string() : call1_res.get_err().to_string()) << "\n";

    auto call2_res = m2.call_generic({ "'test'"_sqf });
    std::cout << "call 2: " << (call2_res.is_ok() ? "ok" : "err") << " - " <<
        (call2_res.is_ok() ? call2_res.get_ok().to_string() : call2_res.get_err().to_string()) << "\n";

    auto call3_res = m2.call_generic({ "'test'"_sqf, "'test'"_sqf });
    std::cout << "call 3: " << (call3_res.is_ok() ? "ok" : "err") << " - " <<
        (call3_res.is_ok() ? call3_res.get_ok().to_string() : call3_res.get_err().to_string()) << std::endl;

    auto call4_res = m3.call_generic({ "'test'"_sqf, "'test'"_sqf });
    std::cout << "call 3: " << (call4_res.is_ok() ? "ok" : "err") << " - " <<
        (call4_res.is_ok() ? call4_res.get_ok().to_string() : call4_res.get_err().to_string()) << std::endl;



    // Will error and return "more"
    execute<50>("list"s, std::array{ "\"\""_sqf });
    // Will return array of current working dir
    execute<1000>("list"s, std::array{ "'.'"_sqf, "'.cpp'"_sqf });
    execute<1000>("write"s, std::array{ "'truncate.bin'"_sqf, "false"_sqf, "[65]"_sqf });
    execute<1000>("write"s, std::array{ "'truncate.bin'"_sqf, "false"_sqf, "[66]"_sqf });
    execute<1000>("write"s, std::array{ "'truncate.bin'"_sqf, "false"_sqf, "[67]"_sqf });
    execute<1000>("write"s, std::array{ "'append.bin'"_sqf, "true"_sqf, "[65]"_sqf });
    execute<1000>("write"s, std::array{ "'append.bin'"_sqf, "true"_sqf, "[66]"_sqf });
    execute<1000>("write"s, std::array{ "'append.bin'"_sqf, "true"_sqf, "[67]"_sqf });
    execute<1000>("write"s, std::array{ "'truncate.txt'"_sqf, "false"_sqf, "'A'"_sqf });
    execute<1000>("write"s, std::array{ "'truncate.txt'"_sqf, "false"_sqf, "'B'"_sqf });
    execute<1000>("write"s, std::array{ "'truncate.txt'"_sqf, "false"_sqf, "'C'"_sqf });
    execute<1000>("write"s, std::array{ "'append.txt'"_sqf, "true"_sqf, "'A'"_sqf });
    execute<1000>("write"s, std::array{ "'append.txt'"_sqf, "true"_sqf, "'B'"_sqf });
    execute<1000>("write"s, std::array{ "'append.txt'"_sqf, "true"_sqf, "'C'"_sqf });
}
#endif