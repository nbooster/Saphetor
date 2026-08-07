#include <print>
#include <ranges>
#include <algorithm>
#include <execution>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <omp.h>

#include <vcfpp.hpp>

namespace Saphetor
{

bool Variant::valid() const noexcept
{
    return not ( this->chrom.empty() or this->chrom == "." or this->pos == 0 or this->ref.empty() or this->ref == "." );

    // 4. (Optional) Check ALT if your parser strictly deals with variants (not reference blocks)
    // if (v.alt.empty() || v.alt == ".") return false;
}

Variant VCF::parseVariant([[maybe_unused]] const std::string_view line)
{
    thread_local std::vector<std::string_view> variantTokens;

    variantTokens.clear();

    for ( const auto& chunk : line | std::views::split(VCF::DELIMITER) )
        variantTokens.emplace_back(chunk.begin(), chunk.end());

    if ( variantTokens.size() < VCF::COLUMNS - 1 ) [[unlikely]]
        return {};

    Variant variant;

    variant.chrom = variantTokens[0];

    std::from_chars(variantTokens[1].data(), variantTokens[1].data() + variantTokens[1].size(), variant.pos);

    variant.id = variantTokens[2];

    variant.ref = variantTokens[3];

    variant.alt = variantTokens[4];

    std::from_chars(variantTokens[5].data(), variantTokens[5].data() + variantTokens[5].size(), variant.qual);

    variant.filter = variantTokens[6];

    if ( variantTokens[7] != "." ) [[likely]]
    {
        for ( const auto& infoTokenRange : variantTokens[7] | std::views::split(';') ) 
        {
            StringType infoToken( infoTokenRange.begin(), infoTokenRange.end() );
            
            if ( infoToken.empty() ) [[unlikely]] 
                continue;

            const size_t eq_pos { infoToken.find('=') };

            if ( eq_pos not_eq StringType::npos ) 
            {
                StringType key = infoToken.substr(0, eq_pos);

                StringType value = infoToken.substr(eq_pos + 1);

                variant.info[key] = value;
            }

            else 
                variant.info[infoToken] = ""; 
        }
    }

    if ( variantTokens.size() >= VCF::COLUMNS ) [[likely]]
    {
        std::vector<StringType> formatKeys;

        for ( const auto& formatKeyToken : variantTokens[8] | std::views::split(':') )
        {
            const StringType formatKey ( formatKeyToken.begin(), formatKeyToken.end() );

            variant.format[formatKey];

            formatKeys.emplace_back(formatKey);
        }

        for ( uint16_t column{ VCF::COLUMNS }; column < variantTokens.size(); ++column )
            for ( uint16_t idx{}; const auto& formatValueToken : variantTokens[column] | std::views::split(':') )
                variant.format[formatKeys[idx++]].emplace_back(StringType{ formatValueToken.begin(), formatValueToken.end() });
    }

    return variant;
}

bool VCF::parse()
{
    const std::string_view fsv(this->buffer, this->bufferSize);

    size_t pos = std::string_view::npos;

    if ( fsv.starts_with("#CHROM\t") ) [[unlikely]]
        pos = 0;

    else 
    {
        pos = fsv.find("\n#CHROM\t");
        
        if ( pos not_eq std::string_view::npos ) 
            pos += 1;
    }

    char* result { (pos not_eq std::string_view::npos) ? (this->buffer + pos) : nullptr };
    
    if ( not result )
    {
        CERR("Invalid VCF: #CHROM header line not found.");

        return false;
    }

    const size_t offset { static_cast<size_t>(result - this->buffer) };

    std::vector<size_t> newLinePositions;

    findAllNewlines(result, this->bufferSize - offset, newLinePositions);

    this->variants.resize(newLinePositions.size());

    #pragma omp parallel for schedule(static)
    for ( size_t i = 0; i < newLinePositions.size() - 1; ++i )
    {
        const size_t startIdx { newLinePositions[i] + 1 };
        
        this->variants[i] = VCF::parseVariant(std::string_view{result + startIdx, newLinePositions[i+1] - startIdx});
    }

    #if defined(TBB_INTERFACE_VERSION) or defined(__TBB_cpp_generic_implementation)

    this->variants.erase(
        std::remove_if(std::execution::par_unseq, this->variants.begin(), this->variants.end(), 
            [](const auto& v) { return not v.valid(); }
        ),
        this->variants.end()
    );

    #else

    std::erase_if(this->variants, [](const auto& v) { return not v.valid(); });

    #endif

    this->variants.shrink_to_fit();

    return true;
}

VcfErrorCode VCF::parseVariantsFromFile()
{
    const unique_fd fd { open(this->filePath.data(), O_RDONLY) };

    if ( fd == -1 )
    {
        CERR("Error (open): {}", std::strerror(errno));

        return VcfErrorCode::FileNotFound; 
    }

    if ( isGzipFile(fd) )
    {
        if ( not decompressGzipMmap(fd, this->decompressedBuffer) )
            return VcfErrorCode::DecompressionError;

        this->buffer = this->decompressedBuffer.data();

        this->bufferSize = this->decompressedBuffer.size();
    }

    else
    {
        struct stat sb;

        if ( fstat(fd, std::addressof(sb)) == -1 ) 
        { 
            CERR("Error (fstat): {}", std::strerror(errno));

            return VcfErrorCode::FstatError; 
        }

        this->bufferSize = sb.st_size;

        this->buffer = static_cast<char*>(mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0));

        if ( this->buffer == MAP_FAILED ) 
        { 
            CERR("Error (mmap): {}", std::strerror(errno));

            return VcfErrorCode::MmapError;
        }
    }

    if ( not this->parse() )
        return VcfErrorCode::MalformedHeader;

    COUT("File {} was parsed successfully ({} total valid variants found).", this->filePath.data(), this->variants.size());

    return VcfErrorCode::Success;
}

VCF::VCF(const char *vcf_path, const bool parse)
: filePath{ vcf_path }
{
    if ( parse )
        this->parseVariantsFromFile();
}

VCF::~VCF()
{
    if ( this->buffer not_eq this->decompressedBuffer.data() and this->buffer not_eq MAP_FAILED )
        munmap(static_cast<void*>(this->buffer), this->bufferSize);
}

const std::vector<Variant>& VCF::getVariants() const { return variants; }

} // namespace Saphetor;