#include <print>
#include <ranges>
#include <format>

#include <json.hpp>

#include <dal.hpp>

namespace Saphetor
{

void DAL::DalConfig::print() const
{
    COUT("\nDatabase Configuration Environment Variables:\n\n"
                         "SAPHETOR_DB_PATH: {}\n"
                         "SAPHETOR_DB_SYNC_MODE: {}\n"
                         "SAPHETOR_CACHE_SIZE_KB: {}\n"
                         "SAPHETOR_MMAP_SIZE_BYTES: {}\n"
                         "SAPHETOR_QUERY_BATCH_SIZE: {}\n",
    this->dbPath, this->syncMode, this->cacheSizeKb, this->mmapSizeBytes, this->batchSize);
}

// Can be optimized more...
std::string DAL::variantDataToJsonString(const Variant& v)
{
    thread_local nlohmann::json data;

    data["FILTER"] = v.filter;

    data["QUAL"] = v.qual;

    data["INFO"] = v.info;

    data["FORMAT"] = v.format;

    double val;

    for ( auto& [key, value] : data["INFO"].items() )
    {
        if ( isNumber(value.get<std::string_view>(), val) )
        {
            if ( isDoubleAnInteger(val) ) [[likely]] 
                value = static_cast<int>(val);

            else
                value = val;
        }
    }

    for ( auto& [key, values] : data["FORMAT"].items() )
    {
        for ( auto& value : values )
        {
            if ( isNumber(value.get<std::string_view>(), val) )
            {
                if ( isDoubleAnInteger(val) ) [[likely]] 
                    value = static_cast<int>(val);

                else
                    value = val;
            }
        }
    }

    return data.dump();
}

DAL::DalConfig DAL::fromEnvironment()
{
    DalConfig cfg;
    
    const auto getEnvString = [](const char* name, const char* def)
    {
        const char* val { std::getenv(name) };

        if ( val == nullptr )
            CERR("'{}' env var value NOT set. Using the DEFAULT one.", name);

        return val ? std::string(val) : std::string(def);
    };

    cfg.dbPath = getEnvString("SAPHETOR_DB_PATH", "variants.db");

    cfg.syncMode = getEnvString("SAPHETOR_DB_SYNC_MODE", "FAST");
    
    const auto getEnvInt = [&getEnvString](const char* name, const ssize_t def)
    {
        const std::string val { getEnvString(name, "") };

        if ( val.empty() ) 
            return def;

        ssize_t result { def };

        std::from_chars(val.data(), val.data() + val.size(), result);

        return result;
    };

    cfg.cacheSizeKb = getEnvInt("SAPHETOR_CACHE_SIZE_KB", -262'144);

    cfg.mmapSizeBytes = getEnvInt("SAPHETOR_MMAP_SIZE_BYTES", 2'147'483'648);

    cfg.batchSize = std::min(std::max(static_cast<ssize_t>(1), getEnvInt("SAPHETOR_QUERY_BATCH_SIZE", 20)), static_cast<ssize_t>(DAL::MAX_BATCH_SIZE)); 

    return cfg;
}

DAL::DAL(): 
singleSql { "INSERT INTO {} (CHROMOSOME, POSITION, REF, ALT, DATA) VALUES (?, ?, ?, ?, ?);" },
batchSql { "INSERT INTO {} (CHROMOSOME, POSITION, REF, ALT, DATA) VALUES " },
config{ DAL::fromEnvironment() },
db{ this->config.dbPath, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE }
{
    this->config.print();

    if ( this->config.syncMode == "FAST" )
    {
        this->db.exec("PRAGMA journal_mode = OFF;");

        this->db.exec("PRAGMA synchronous = OFF;");
    }

    else
    {
        this->db.exec("PRAGMA journal_mode = WAL;");
        
        this->db.exec("PRAGMA synchronous = NORMAL;");
    }
    
    this->db.exec(std::format("PRAGMA cache_size = {};", this->config.cacheSizeKb));

    this->db.exec("PRAGMA temp_store = MEMORY;");

    this->db.exec(std::format("PRAGMA mmap_size = {};", this->config.mmapSizeBytes));

    const std::string_view placeholders { "(?, ?, ?, ?, ?)" };
    
    for ( size_t i{}; i < this->config.batchSize; ++i )
    {
        this->batchSql += placeholders;

        if ( i < this->config.batchSize - 1 )
            this->batchSql += ", ";
    }

    this->batchSql += ";";

    COUT("Database '{}' connected successfully.", config.dbPath);
}

DalErrorCode DAL::initSchema(const std::string_view table, const bool drop_if_exists, const bool vacuum)
{
    try
    {
        if ( not ( isAlphaNumeric(table) and table.size() > 0 ) )
        {
            CERR("'initSchema': Table name is not alphanumeric.");

            return DalErrorCode::InvalidTableName;
        }

        if ( drop_if_exists )
        {
            this->db.exec(std::format("DROP TABLE IF EXISTS {};", table));

            if ( vacuum )
              this->db.exec("VACUUM;");

            COUT("Table '{}' was dropped if already existed.", table);
        }

        this->db.exec(
            std::format(
                "CREATE TABLE IF NOT EXISTS {} "
                "(ID INTEGER PRIMARY KEY, CHROMOSOME TEXT, POSITION INTEGER, REF TEXT, ALT TEXT, DATA TEXT);"
                "CREATE INDEX IF NOT EXISTS idx_chr_pos ON {} (CHROMOSOME, POSITION);",
                table, table
            )
        );

        COUT("Schema for table '{}' was initialized.", table);

        return DalErrorCode::Success;
    }

    catch ( const std::exception& e )
    {
        CERR("Exception thrown: {}", e.what());

        return DalErrorCode::SQLException;
    }
}

DalErrorCode DAL::insertVariant(const std::string_view table, const Variant& variant)
{
    try
    {
        if ( not ( isAlphaNumeric(table) and table.size() > 0 ) )
        {
            CERR("'insertVariant': Table name is not alphanumeric.");

            return DalErrorCode::InvalidTableName;
        }

        SQLite::Statement insertStmt(this->db, 
            std::format("INSERT INTO {} (CHROMOSOME, POSITION, REF, ALT, DATA) VALUES (?, ?, ?, ?, ?);", table) );

        insertStmt.bind(1, std::string{variant.chrom});

        insertStmt.bind(2, variant.pos);

        insertStmt.bind(3, std::string{variant.ref});

        insertStmt.bind(4, std::string{variant.alt});

        insertStmt.bind(5, variantDataToJsonString(variant));

        insertStmt.exec();

        return DalErrorCode::Success;
    }

    catch ( const std::exception& e )
    {
        CERR("Exception thrown: {}", e.what());

        return DalErrorCode::SQLException;
    }
}

DalErrorCode DAL::insertVariants(const std::string_view table, const std::vector<Variant>& variants)
{
    try
    {
        if ( not ( isAlphaNumeric(table) and table.size() > 0 ) )
        {
            CERR("'insertVariants': Table name is not alphanumeric.");

            return DalErrorCode::InvalidTableName;
        }

        std::vector<std::string> jsons(variants.size());

        #pragma omp parallel for schedule(static)
        for ( size_t i = 0; i < variants.size(); ++i )
            jsons[i] = variantDataToJsonString(variants[i]);

        SQLite::Transaction transaction(this->db);

        SQLite::Statement insertStmt(this->db, 
            std::format("INSERT INTO {} (CHROMOSOME, POSITION, REF, ALT, DATA) VALUES (?, ?, ?, ?, ?);", table) );

        for ( size_t idx{}; const Variant& variant : variants )
        {
            insertStmt.bind(1, std::string{variant.chrom});

            insertStmt.bind(2, variant.pos);

            insertStmt.bind(3, std::string{variant.ref});

            insertStmt.bind(4, std::string{variant.alt});
            
            insertStmt.bind(5, jsons[idx++]);

            insertStmt.exec();

            insertStmt.reset();

            insertStmt.clearBindings();
        }

        transaction.commit();

        return DalErrorCode::Success;
    }

    catch ( const std::exception& e )
    {
        CERR("Exception thrown: {}", e.what());

        return DalErrorCode::SQLException;
    }
}

DalErrorCode DAL::insertVariantsBatched(const std::string_view table, const std::vector<Variant>& variants)
{
    try
    {
        if ( not ( isAlphaNumeric(table) and table.size() > 0 ) )
        {
            CERR("'insertVariantsBatched': Table name is not alphanumeric.");

            return DalErrorCode::InvalidTableName;
        }

        std::vector<std::string> jsons(variants.size());

        #pragma omp parallel for schedule(static)
        for ( size_t i = 0; i < variants.size(); ++i )
            jsons[i] = variantDataToJsonString(variants[i]);

        SQLite::Transaction transaction(this->db);

        #if defined(__cpp_lib_format) and __cpp_lib_format >= 202311L

        SQLite::Statement batchStmt(this->db, std::format(std::dynamic_format(batchSql), table));

        SQLite::Statement singleStmt(this->db, std::format(std::dynamic_format(singleSql), table));

        #else

        SQLite::Statement batchStmt(this->db, std::vformat(batchSql, std::make_format_args(table)));

        SQLite::Statement singleStmt(this->db, std::vformat(singleSql, std::make_format_args(table)));

        #endif

        size_t idx{};

        const auto endIdx { variants.size() - this->config.batchSize };

        if ( variants.size() < this->config.batchSize )
            goto SerialInsertLabel;

        while ( idx < endIdx ) [[likely]]
        {
            for ( size_t row{}; row < this->config.batchSize; ++row ) 
            {
                const auto& variant { variants[idx] };

                const auto bindOffset { (row * DAL::COLUMNS) };

                batchStmt.bind(bindOffset + 1, std::string{variant.chrom});

                batchStmt.bind(bindOffset + 2, variant.pos);

                batchStmt.bind(bindOffset + 3, std::string{variant.ref});

                batchStmt.bind(bindOffset + 4, std::string{variant.alt});

                batchStmt.bind(bindOffset + 5, jsons[idx++]);
            }

            batchStmt.exec();

            batchStmt.reset();

            batchStmt.clearBindings();
        }

        SerialInsertLabel:

        for ( ; idx < variants.size(); ++idx )
        {
            const auto& variant { variants[idx] };
            
            singleStmt.bind(1, std::string{variant.chrom});

            singleStmt.bind(2, variant.pos);

            singleStmt.bind(3, std::string{variant.ref});

            singleStmt.bind(4, std::string{variant.alt});

            singleStmt.bind(5, jsons[idx]);

            singleStmt.exec();

            singleStmt.reset();

            singleStmt.clearBindings();
        }

        transaction.commit();

        return DalErrorCode::Success;
    }

    catch ( const std::exception& e )
    {
        CERR("Exception thrown: {}", e.what());

        return DalErrorCode::SQLException;
    }
}

std::expected<size_t, DalErrorCode> DAL::totalVariants(const std::string_view table)
{
    try
    {
        if ( not ( isAlphaNumeric(table) and table.size() > 0 ) )
        {
            CERR("'totalVariants': Table name is not alphanumeric.");

            return std::unexpected(DalErrorCode::InvalidTableName);
        }

        SQLite::Statement queryStmt(this->db, std::format("SELECT COUNT(*) FROM {};", table) );

        queryStmt.executeStep();

        return queryStmt.getColumn(0).getInt64();
    }

    catch ( const std::exception& e )
    {
        CERR("Exception thrown: {}", e.what());

        return std::unexpected(DalErrorCode::SQLException);
    }
}

DalErrorCode DAL::printTables(std::ostream& stream)
{
    try
    {
        stream << "DB User Tables:\n";

        SQLite::Statement queryStmt(this->db, "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%';");

        while ( queryStmt.executeStep() )
            std::println(stream, "{}", queryStmt.getColumn(0).getText());

        stream.flush();

        return DalErrorCode::Success;
    }

    catch ( const std::exception& e )
    {
        CERR("Exception thrown: {}", e.what());

        return DalErrorCode::SQLException;
    }
}

DalErrorCode DAL::printRecords( const std::string_view table, 
                                const bool count, 
                                std::vector<std::string> outputColumns, 
                                const std::unordered_map<std::string, std::string>& filters, 
                                const uint64_t limit, 
                                const bool printQuery, 
                                const bool printHeader, 
                                std::ostream& stream
                            )
{
    static constexpr std::array<std::string_view, 5> validColumns { "CHROMOSOME", "POSITION", "REF", "ALT", "DATA" };

    static constexpr std::array<std::string_view, 6> filterColumns { "CHROMOSOME", "POSITION", "REF", "ALT", "FILTER", "QUAL" };

    static const std::vector<char> extraCharsAllowed { ',', '.', '_', '-' };

    try
    {
        removeDuplicates(outputColumns);

        if ( not (count or outputColumns.size() > 0) )
        {
            CERR("'printRecords': either count must be true or outputColumns not empty.");

            return DalErrorCode::PrintRecordsInvalidArgs;
        }

        std::string preQuery { "SELECT" };

        if ( not count )
        {
            for ( const auto& col : outputColumns )
                if ( std::ranges::contains(validColumns, col) )
                    preQuery += std::format(" {},", col);

            if ( preQuery == "SELECT" )
            {
                CERR("'printRecords': no column in outputColumns was valid.");

                return DalErrorCode::PrintRecordsInvalidOutputCols;
            }

            preQuery.erase(preQuery.size() - 1);

            preQuery += std::format(" FROM {}", table);
        }

        auto query { count ? std::format("SELECT COUNT(*) FROM {}", table) : preQuery };

        if ( filters.size() > 0 )
            query += " WHERE";

        for ( const auto& [key, value] : filters )
        {   // Sanitization for SQL Injection 
            if ( not ( std::ranges::contains(filterColumns, key) and isAlphaNumeric(value, extraCharsAllowed) ) ) [[unlikely]]
            {
                CERR("'printRecords': Invalid filters.");

                return DalErrorCode::PrintRecordsInvalidFilters;
            }

            if ( key == "FILTER" )
                query += std::format(" json_extract(DATA, '$.FILTER') = '{}' AND", value);

            else if ( key == "QUAL" )
                query += std::format(" ROUND(CAST(DATA->>'$.QUAL' AS REAL), {}) = {} AND", DAL::NUMERIC_EQUALITY_PRECISION, value);

            else if ( key == "POSITION" )
                query += std::format(" {} = {} AND", key, value);

            else
                query += std::format(" {} = '{}' AND", key, value);
        }

        if ( query.ends_with(" AND") )
            query.erase(query.size() - 4);

        if ( not count )
            query += " ORDER BY CHROMOSOME, POSITION";

        if ( limit > 0 and not count )
            query += std::format(" LIMIT {}", limit);

        query += ";";

        if ( printQuery )
            std::println(stream, "{}", query);

        if ( printHeader )
        {
            const auto headerView { outputColumns | std::views::join_with('\t') };

            std::print(stream, "{}\n", std::string{headerView.begin(), headerView.end()});
        }

        SQLite::Statement queryStmt(this->db, query);

        if ( count )
        {
            queryStmt.executeStep();

            std::println(stream, "{}", queryStmt.getColumn(0).getInt64());
        }

        else
        {
            while ( queryStmt.executeStep() )
            {
                for ( uint16_t colIdx{}; colIdx < queryStmt.getColumnCount(); ++colIdx )
                {
                    std::print(stream, "{}\t", queryStmt.getColumn(colIdx).getText());
                }

                std::println(stream);
            }
        }

        stream.flush();

        return DalErrorCode::Success;
    }

    catch ( const std::exception& e )
    {
        CERR("Exception thrown: {}", e.what());

        return DalErrorCode::SQLException;
    }
}

} // namespace Saphetor