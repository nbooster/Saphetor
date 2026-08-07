#ifndef DAL_HPP
#define DAL_HPP
#pragma once

#include <iostream>
#include <vector>
#include <expected>

#include <vcfpp.hpp>

#include <SQLiteCpp/SQLiteCpp.h>

namespace Saphetor
{

enum class DalErrorCode : std::uint8_t 
{
    Success = 0,
    InvalidTableName,
    InsertInvalidVariant,
    PrintRecordsInvalidArgs,
    PrintRecordsInvalidOutputCols,
    PrintRecordsInvalidFilters,
    SQLException
};

class DAL final
{
	struct DalConfig
	{
	    std::string dbPath;

	    std::string syncMode;

	    ssize_t cacheSizeKb;

	    size_t mmapSizeBytes;
	    
	    size_t batchSize;

	    void print() const;
	};

	static constexpr size_t NUMERIC_EQUALITY_PRECISION { 6 };

	static constexpr size_t COLUMNS { 5 };

	std::string singleSql;

	std::string batchSql;

	DalConfig config;

	SQLite::Database db;

private:

	static DAL::DalConfig fromEnvironment();

	static std::string variantDataToJsonString(const Variant& v);

public:

	DAL();

	DalErrorCode initSchema(const std::string_view table, const bool drop_if_exists = true, const bool vacuum = true);

	DalErrorCode insertVariant(const std::string_view table, const Variant& variant);

	DalErrorCode insertVariants(const std::string_view table, const std::vector<Variant>& variants);

	DalErrorCode insertVariantsBatched(const std::string_view table, const std::vector<Variant>& variants);

	[[gnu::hot, nodiscard]] std::expected<size_t, DalErrorCode> totalVariants(const std::string_view table);

	DalErrorCode printTables(std::ostream& stream = std::cout);

	[[gnu::hot]] DalErrorCode printRecords(
						const std::string_view table, 
						const bool count = false, 
						std::vector<std::string> outputColumns = { "CHROMOSOME", "POSITION", "REF", "ALT", "DATA" },
						const std::unordered_map<std::string, std::string>& filters = {}, 
						const uint64_t limit = 0, 
						const bool printQuery = false, 
						const bool printHeader = true,
						std::ostream& stream = std::cout
					);
};

} // namespace Saphetor

#endif