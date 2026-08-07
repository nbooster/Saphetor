#include <fstream>
#include <filesystem>
#include <string>
#include <sstream>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <dal.hpp>

namespace fs = std::filesystem;

using namespace Saphetor;

std::string createTempVCF(const std::string_view content)
{
    const fs::path tempPath { fs::temp_directory_path() / "test_dummy.vcf" };

    std::ofstream out(tempPath);

    out << content;

    return tempPath.string();
}

Variant createDummyVariant()
{
    Variant v;

    v.chrom = "chr1";
    v.qual = 123.456;
    v.pos = 1000;
    v.ref = "A";
    v.alt = "T";
    v.id = ".";

    return v;
}

TEST_CASE("VCF Parser accurately extracts standard variants", "[vcf][basic]")
{
    constexpr std::string_view vcfContent = 
        "##fileformat=VCFv4.2\n"
        "##source=test\n"
        "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\tFORMAT\tSample1\n"
        "chr1\t10177\trs367896724\tA\tAC\t100\tPASS\tAC=1;AF=0.425319;AN=2\tGT:DP:GQ\t0/1:20:48\n"
        "chr2\t20000\t.\tG\tT\t50.5\t.\tDP=15\tGT\t1/1\n";

    const auto filePath { createTempVCF(vcfContent) };
    
    VCF parser(filePath.c_str(), true);

    const auto& variants { parser.getVariants() };

    REQUIRE(variants.size() == 2);

    SECTION("Check first variant (chr1)")
    {
        const auto& v = variants[0];

        CHECK(v.chrom == "chr1");
        CHECK(v.pos == 10177);
        CHECK(v.id == "rs367896724");
        CHECK(v.ref == "A");
        CHECK(v.alt == "AC");
        CHECK(v.qual == 100.0);
        CHECK(v.filter == "PASS");

        REQUIRE(v.info.contains("AC"));
        CHECK(v.info.at("AC") == "1");
        REQUIRE(v.info.contains("AF"));
        CHECK(v.info.at("AF") == "0.425319");

        REQUIRE(v.format.contains("GT"));
        CHECK(v.format.at("GT")[0] == "0/1");
        REQUIRE(v.format.contains("DP"));
        CHECK(v.format.at("DP")[0] == "20");
    }

    SECTION("Check second variant (chr2)")
    {
        const auto& v { variants[1] };

        CHECK(v.chrom == "chr2");
        CHECK(v.pos == 20000);
        CHECK(v.qual == 50.5);
    }

    fs::remove(filePath);
}

TEST_CASE("VCF Parser handles Boolean INFO flags correctly", "[vcf][info]")
{
    constexpr std::string_view vcfContent = 
        "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n"
        "chr1\t12345\t.\tC\tT\t.\t.\tDP=10;SOMATIC;VALIDATED\n";

    const auto filePath { createTempVCF(vcfContent) };
    
    VCF parser(filePath.c_str(), true);

    const auto& variants { parser.getVariants() };

    REQUIRE(variants.size() == 1);
    
    const auto& v { variants[0] };
    
    REQUIRE(v.info.contains("DP"));
    CHECK(v.info.at("DP") == "10");

    REQUIRE(v.info.contains("SOMATIC"));
    CHECK(v.info.at("SOMATIC") == "");
    
    REQUIRE(v.info.contains("VALIDATED"));
    CHECK(v.info.at("VALIDATED") == "");

    fs::remove(filePath);
}

TEST_CASE("VCF Parser safely fails on malformed files missing #CHROM", "[vcf][header]")
{
    constexpr std::string_view malformedContent = 
        "##fileformat=VCFv4.2\n"
        "##contig=<ID=chr1,length=248956422>\n"
        "chr1\t12345\t.\tC\tT\t.\t.\tDP=10\n";

    const auto filePath { createTempVCF(malformedContent) };
    
    VCF parser(filePath.c_str(), true);

    const auto& variants { parser.getVariants() };

    CHECK(variants.size() == 0);

    fs::remove(filePath);
}

TEST_CASE("VCF Parser skips deceptive #CHROM inside headers", "[vcf][header]")
{
    constexpr std::string_view trickyContent = 
        "##fileformat=VCFv4.2\n"
        "##description=\"This contains #CHROM in the middle of a string\"\n"
        "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n"
        "chr1\t12345\t.\tC\tT\t.\t.\tDP=10\n";

    const auto filePath { createTempVCF(trickyContent) };
    
    VCF parser(filePath.c_str(), true);

    const auto& variants { parser.getVariants() };

    REQUIRE(variants.size() == 1);
    CHECK(variants[0].chrom == "chr1");

    fs::remove(filePath);
}

TEST_CASE("VCF Parser handles invalid variants correctly", "[vcf][invalid]")
{
    constexpr std::string_view vcfContent = 
        "#CHROM\tPOS\tID\tREF\tALT\tQUAL\tFILTER\tINFO\n"
        // Valids:
        "chr1\t12345\t.\tC\tT\t.\t.\tDP=10;SOMATIC;VALIDATED\n"
        "chr2\t12345\t.\tC\tT\t.\t.\tDP=10;SOMATIC;VALIDATED\t\t\t\n"
        "chr3\t12345\t.\tC\t.\t.\t.\t.\n"
        "chr4\t12345\t\tC\t\t\t\t\n"
        // Invalids:
        "\t12345\t.\tC\tT\t.\t.\tDP=10;SOMATIC;VALIDATED\n"
        ".\t12345\t.\tC\tT\t.\t.\tDP=10;SOMATIC;VALIDATED\n"
        "chr1\t\t.\tC\tT\t.\t.\tDP=10;SOMATIC;VALIDATED\n"
        "chr1\t.\t.\tC\tT\t.\t.\tDP=10;SOMATIC;VALIDATED\n"
        "chr1\t0\t.\tC\tT\t.\t.\tDP=10;SOMATIC;VALIDATED\n"
        "chr1\t12345\t.\t\tT\t.\t.\tDP=10;SOMATIC;VALIDATED\n"
        "chr1\t12345\t.\t.\tT\t.\t.\tDP=10;SOMATIC;VALIDATED\n"
        "\t\t\t\t\t\t\t\t\t\t\t\n"
        "\t\t\t\t\t\t\t\n"
        "\t\t\n"
        "\n";

    const auto filePath { createTempVCF(vcfContent) };
    
    VCF parser(filePath.c_str(), true);

    const auto& variants { parser.getVariants() };

    REQUIRE(variants.size() == 4);

    fs::remove(filePath);
}

TEST_CASE("DAL Schema Initialization", "[dal][schema]")
{
    DAL dal;
    
    SECTION("Initialize valid table with default parameters") {
        REQUIRE(dal.initSchema("test_variants") == DalErrorCode::Success);
    }

    SECTION("Initialize valid table and drop existing") {
        REQUIRE(dal.initSchema("test_variants", true, true) == DalErrorCode::Success);
    }

    SECTION("Initialize schema with an invalid table name") {
        REQUIRE(dal.initSchema("") == DalErrorCode::InvalidTableName);
    }
}

TEST_CASE("DAL Variant Insertions & Counting", "[dal][insert]")
{
    DAL dal;

    const std::string tableName = "test_insertions";
    
    REQUIRE(dal.initSchema(tableName, true, false) == DalErrorCode::Success);

    Variant v1 = createDummyVariant();
    Variant v2 = createDummyVariant();
    Variant v3 = createDummyVariant();

    SECTION("Insert a single variant") {
        REQUIRE(dal.insertVariant(tableName, v1) == DalErrorCode::Success);
        
        auto count = dal.totalVariants(tableName);
        REQUIRE(count.has_value());
        REQUIRE(count.value() == 1);
    }

    SECTION("Insert multiple variants using std::vector") {
        std::vector<Variant> variants = {v1, v2};
        REQUIRE(dal.insertVariants(tableName, variants) == DalErrorCode::Success);
        
        auto count = dal.totalVariants(tableName);
        REQUIRE(count.has_value());
        REQUIRE(count.value() == 2);
    }

    SECTION("Insert multiple variants using batch method") {
        std::vector<Variant> variants = {v1, v2, v3};
        REQUIRE(dal.insertVariantsBatched(tableName, variants) == DalErrorCode::Success);
        
        auto count = dal.totalVariants(tableName);
        REQUIRE(count.has_value());
        REQUIRE(count.value() == 3);
    }

    SECTION("Count variants on a non-existent table fails gracefully") {
        auto count = dal.totalVariants("non_existent_table");
        REQUIRE_FALSE(count.has_value());
        REQUIRE(count.error() == DalErrorCode::SQLException); 
    }
}

TEST_CASE("DAL Stream Outputs and Printing", "[dal][print]")
{
    DAL dal;
    const std::string tableName = "test_printing";
    
    REQUIRE(dal.initSchema(tableName, true, false) == DalErrorCode::Success);
    REQUIRE(dal.insertVariant(tableName, createDummyVariant()) == DalErrorCode::Success);

    SECTION("printTables outputs to stream") {
        std::ostringstream oss;
        REQUIRE(dal.printTables(oss) == DalErrorCode::Success);
        
        std::string output = oss.str();
        REQUIRE_FALSE(output.empty());
        REQUIRE_THAT(output, Catch::Matchers::ContainsSubstring(tableName));
    }

    SECTION("printRecords outputs successful valid queries") {
        std::ostringstream oss;
        std::vector<std::string> cols = {"CHROMOSOME", "POSITION", "REF", "ALT"};
        
        DalErrorCode result = dal.printRecords(
            tableName, 
            false,  // count
            cols,   // outputColumns
            {},     // filters
            10,     // limit
            true,   // printQuery
            true,   // printHeader
            oss
        );

        REQUIRE(result == DalErrorCode::Success);
        REQUIRE_FALSE(oss.str().empty());
    }

    SECTION("printRecords fails on invalid output columns") {
        std::ostringstream oss;
        std::vector<std::string> invalidCols = {"NON_EXISTENT_COL"};
        
        DalErrorCode result = dal.printRecords(
            tableName, 
            false, 
            invalidCols, 
            {}, 
            10, 
            false, 
            true, 
            oss
        );

        REQUIRE(result == DalErrorCode::PrintRecordsInvalidOutputCols);
    }

    SECTION("printRecords fails gracefully on missing table") {
        std::ostringstream oss;
        DalErrorCode result = dal.printRecords(
            "fake_table", 
            false, 
            {"CHROMOSOME"}, 
            {}, 
            0, 
            false, 
            false, 
            oss
        );

        REQUIRE((result == DalErrorCode::SQLException));
    }
};
