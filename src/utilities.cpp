#include <utilities.hpp>

#include <libdeflate.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <cmath>
#include <cstring>

#include <print>
#include <utility>
#include <algorithm>
#include <unordered_set>

namespace Saphetor
{

unique_fd::unique_fd(const int f) : fd(f) {}

unique_fd::~unique_fd() { if ( fd not_eq -1 ) close(fd); }

unique_fd::operator int() const { return fd; }

[[gnu::optimize("-O3")]]
bool isDoubleAnInteger(double val) 
{
    return std::isfinite(val) and std::trunc(val) == val;
}

void findAllNewlines(const char* buffer, const size_t buffer_size, std::vector<size_t>& positions, const uint32_t lineLengthHint)
{
    if ( buffer_size == 0 ) [[unlikely]]
        return;

    positions.reserve(buffer_size / lineLengthHint);

    const char* ptr { buffer };

    const char* end { buffer + buffer_size };

    while ( ( ptr = static_cast<const char*>(std::memchr(ptr, '\n', end - ptr) ) ) not_eq nullptr )
    {
        positions.emplace_back(ptr - buffer);

        ++ptr;
    }
}

bool isGzipFile(const int fd)
{
    uint8_t header[2] {0};

    ssize_t bytesRead { pread(fd, header, 2, 0) };

    // Check for Gzip signature: 0x1F, 0x8B
    return bytesRead == 2 and header[0] == 0x1F and header[1] == 0x8B;
}

bool isNumber(const std::string_view s, double& val)
{
    const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);

    // Valid if no error occurred AND the entire string was consumed
    return ec == std::errc{} and ptr == s.data() + s.size();
}

bool isAlphaNumeric(const std::string_view sv, const std::vector<char>& extras)
{
    for ( char c : sv )
        if ( not (std::ranges::contains(extras, c) or (c >= '0' and c <= '9') or (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z'))) 
            return false;

    return true;
}

void removeDuplicates(std::vector<std::string>& vec)
{
    std::unordered_set<std::string> seen;
    
    auto newEnd = std::remove_if(vec.begin(), vec.end(), [&seen](const std::string& value)
    {
        if ( seen.find(value) == seen.end() )
        {
            seen.insert(value);

            return false;
        }

        return true;
    });

    vec.erase(newEnd, vec.end());
}

bool decompressGzipMmap(const int fd, std::vector<char>& out_buffer)
{
    struct stat sb;

    if ( fstat(fd, std::addressof(sb)) == -1 )
    {
        std::println(stderr, "Failed to get file stats.");

        return false;
    }

    const size_t compressed_size { static_cast<size_t>(sb.st_size) };

    if ( compressed_size < 18 ) // Minimum valid gzip size
    {
        std::println(stderr, "File is too small to be a valid gzip.");

        return false;
    }

    const uint8_t* mapped_data { static_cast<const uint8_t*>(
        mmap(nullptr, compressed_size, PROT_READ, MAP_PRIVATE, fd, 0)
    )};

    if ( mapped_data == MAP_FAILED )
    {
        std::println(stderr, "mmap failed.");

        return false;
    }

    madvise(const_cast<void*>(static_cast<const void*>(mapped_data)), compressed_size, MADV_SEQUENTIAL);

    uint32_t isize_32 { 0 };

    std::memcpy(std::addressof(isize_32), mapped_data + compressed_size - 4, 4);

    constexpr uint64_t FOUR_GB { 1ULL << 32 };

    uint64_t k { 0 };

    if ( compressed_size > isize_32 )
        k = (compressed_size - isize_32 + FOUR_GB - 1) / FOUR_GB;

    libdeflate_decompressor* decompressor { libdeflate_alloc_decompressor() };

    if ( not decompressor)
    {
        munmap(const_cast<void*>(static_cast<const void*>(mapped_data)), compressed_size);

        return false;
    }

    libdeflate_result res { LIBDEFLATE_INSUFFICIENT_SPACE };

    size_t actual_out_size { 0 };

    while ( res == LIBDEFLATE_INSUFFICIENT_SPACE )
    {
        const uint64_t estimated_size { static_cast<uint64_t>(isize_32) + (k * FOUR_GB) };
        
        out_buffer.resize(estimated_size);

        res = libdeflate_gzip_decompress(
            decompressor,
            mapped_data, compressed_size,
            out_buffer.data(), out_buffer.size(),
            std::addressof(actual_out_size)
        );

        if ( res == LIBDEFLATE_INSUFFICIENT_SPACE )
            k++;
    }

    libdeflate_free_decompressor(decompressor);

    munmap(const_cast<void*>(static_cast<const void*>(mapped_data)), compressed_size);

    if ( res not_eq LIBDEFLATE_SUCCESS )
    {
        std::println(stderr, "Decompression failed. Error code: {}", std::to_underlying(res));

        return false;
    }

    out_buffer.resize(actual_out_size);

    return true;
}

} // namespace Saphetor