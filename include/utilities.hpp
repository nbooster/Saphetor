#ifndef UTILITIES_HPP
#define UTILITIES_HPP
#pragma once

#include <unistd.h>

#include <cstdint>

#include <print>
#include <vector>
#include <string_view>

namespace Saphetor
{
	
struct unique_fd
{
    int fd{ -1 };

    unique_fd(const int f);

    ~unique_fd();

    operator int() const;
};

bool isDoubleAnInteger(const double val);

void findAllNewlines(const char* buffer, const size_t buffer_size, std::vector<size_t>& final_result, const uint32_t lineLengthHint = 200);

bool isGzipFile(const int fd);

bool decompressGzipMmap(const int fd, std::vector<char>& out_buffer);

bool isNumber(const std::string_view s, double& val);

bool isAlphaNumeric(const std::string_view sv, const std::vector<char>& extras = { '_' });

void removeDuplicates(std::vector<std::string>& vec);

#ifndef DISABLE_LIB_PRINTING

#define COUT(...) std::println(stdout, __VA_ARGS__)

#define CERR(...) std::println(stderr, __VA_ARGS__)

#else

#define COUT(...)

#define CERR(...)

#endif

} // namespace Saphetor

#endif