#ifndef VCFPP_HPP
#define VCFPP_HPP
#pragma once

#include <utilities.hpp>

#include <string>
#include <vector>

#if __has_include(<boost/version.hpp>)

    #include <boost/unordered/unordered_flat_map.hpp>

    template<class Key, class Value>
    using UMapType = boost::unordered_flat_map<Key, Value>;

#else

    #include <unordered_map>
    
    template<class Key, class Value>
    using UMapType = std::unordered_map<Key, Value>;

#endif

namespace Saphetor
{

enum class VcfErrorCode : std::uint8_t 
{
    Success = 0,
    FileNotFound,
    MalformedHeader,
    DecompressionError,
    MmapError,
    FstatError
};

using StringType = std::string_view;

struct Variant final
{
    uint32_t pos{};

    double qual{ -1.0 };

    StringType chrom;

    StringType id;

    StringType ref;

    StringType alt;

    StringType filter;

    UMapType<StringType, StringType> info;
    
    UMapType<StringType, std::vector<StringType>> format;

    bool valid() const noexcept;
};

class VCF final
{
    static constexpr char DELIMITER { '\t' };

    static constexpr uint8_t COLUMNS { 9 };

    size_t bufferSize{};

    char* buffer { nullptr };

    std::string filePath;

    std::vector<char> decompressedBuffer;

    std::vector<Variant> variants;

private:

    [[gnu::hot]] Variant parseVariant(const std::string_view line);

    bool parse();

public:

    VCF(const char *vcf_path, const bool parse = true);

    ~VCF();

    VcfErrorCode parseVariantsFromFile();

    const std::vector<Variant>& getVariants() const;
};

} // namespace Saphetor;

#endif