# Saphetor
Saphetor interview assignment.
**vcf_importer** parses a .vcf (or .vcf.gz) genomics file and stores its valid variants into an SQL .db file.
## Configure
From the root dir run:
```basch
cmake -S ./ -B ./build/ -Wno-deprecated
```
### Available parameters
```cmake
-G Ninga # for a different build system
-DCMAKE_CXX_COMPILER=/opt/gcc-16/bin/g++ # for a specific compiler to be used if default is not enough
-DCMAKE_BUILD_TYPE=Release # for the build type 
-DDISABLE_LIB_PRINTING=ON # to disable the printing from the internal lib
-DBIN_TYPE=STATIC # for a static binary
-DBUILD_TESTS=ON # to also build the tests
```
### Example
```basch
cmake -S ./ -B ./build/ -Wno-deprecated -DCMAKE_CXX_COMPILER=/opt/gcc-16/bin/g++ -DBUILD_TESTS=ON
```
Build _vcf_importer_, using the latest downloaded and built _GCC 16.1 compiler_, with the its _tests_, in _Release_ mode, with _Unix Makefiles_ as the underlying build system, in a _non-static_ binary, with _lib printing ON_.
## Build (after configure)
```basch
cmake --build ./build/ --parallel $(nproc)
```
**It is highly recommended to keep different build type (_Release, Debug, RelWithDebInfo_) builds in separate folders.**
```basch
# Configure Debug build
cmake -B build/debug -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build/debug

# Configure Release build
cmake -B build/release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build/release
```
## Usage
```basch
./vcf_importer --vcf path_to_file.vcf
```
### Example (from root dir)
```basch
./build/bin/vcf_importer --vcf ./data/sample.vcf

# or for even faster execution, you can run something like:

LD_PRELOAD="/usr/local/lib/libmimalloc.so" ./build/bin/vcf_importer --vcf path_to_big_file.vcf.gz
```
### Run the tests (if you have built them)
```basch
ctest --test-dir ./build
```
## DB Configuration Environment Variables
```cmake
SAPHETOR_DB_PATH (default: variants.db) # db file name
SAPHETOR_DB_SYNC_MODE (default: FAST) # db sync mode
SAPHETOR_CACHE_SIZE_KB (default: -262144) # db cache size
SAPHETOR_MMAP_SIZE_BYTES (default: 2147483648) # db internal mmap size
SAPHETOR_QUERY_BATCH_SIZE (default: 20) # batch number of queries for faster variants insertion
```
## Basic Requirements
```cmake
OS: Linux
Compiler: GCC with C++ 23 support
Build system: CMake version 3.28
Underlying: Unix Makefiles or Ninja
```
## Thirdparty Libs Dependency
**OpenMP** (_Required_ for optimization)

**SQLiteCpp** (_Required_ for the SQL queries)

**libdeflate** (_Required_ for decompression)

**Catch2** (_Required_ for tests)

**Boost** (_Optional_ for optimization)

**TBB** (_Optional_ for optimization)

_Required_ libs are downloaded and build automatically by _CMake_, if they do not exist.

_Optional_ ones are not downloaded, but are used if found.
## Architecture Summary
```cmake
# Main app file
vcf_importer.cpp
# Unit tests file
tests/test_vcf_importer.cpp
# The DAL class
include/dal.hpp
src/dal.cpp
# The parser and variant classes
include/vcfpp.hpp
src/vcfpp.cpp
# Some useful utilities
include/utilities.hpp
src/utilities.cpp
# Json object handler
include/json.hpp
```
## Notes
```text
The application starts by creating (or connecting) to the .db file,
with the provided configuration.

If the table 'variants' exists it drops it, and frees its memory from the file,
and then creates it again from scratch, with the necessary index.

If it does not, then it just does the creation process.

It then decompresses (if need) and parses (in parallel) the input .vcf file,
keeping its contents in memory while it runs.

The parsing stage creates the container with the valid variants,
and then all of them are inserted fast (in batches) into the 'variants' table.

It finally queries the table to get the total number records (valid variants)
that were successfully inserted.

TODO 1: The json string creation method can be optimized even more.
TODO 2: The .vcf parser can be made more robust/complete.

*Execution speed can be improved even more by using a custom malloc lib
(with LD_PRELOAD for example) like: mimalloc, tcmalloc, jemalloc...

**For the big input file (C++ dev_assignment.vcf.gz) of the assignment,
of total size: ~600 MB uncompressed, it finishes in ~ 9.5 secs,
on an Intel(R) Xeon(R) Gold 5315Y CPU @ 3.20GHz.
```
