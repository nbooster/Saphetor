#include <print>

// #define DISABLE_LIB_PRINTING

#include <vcfpp.hpp>
#include <dal.hpp>

int main(int argc, char *argv[])
{
    if ( argc < 3 or std::strncmp(argv[1], "--vcf", 5) not_eq 0 )
    {
        std::println(stderr, "Input error.\nCorrect usage: ./vcf_importer --vcf input.vcf\nExiting...");

        return EXIT_FAILURE;
    }

    constexpr std::string_view table { "variants" };

    Saphetor::DAL Dal;

    Dal.initSchema(table);

    Saphetor::VCF vcf(argv[2]);

    Dal.insertVariantsBatched(table.data(), vcf.getVariants());

    // Dal.printRecords(table);

    std::println(stdout, "Total records (variants) were inserted into table '{}': {}", table, Dal.totalVariants(table).value());

    std::println(stdout, "Program Finished successfully!");

    return EXIT_SUCCESS;
}