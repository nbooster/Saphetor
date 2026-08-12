cmake_minimum_required(VERSION 3.28)

set(NAME vcf_importer)

project(${NAME} VERSION 0.0.1 DESCRIPTION "VCF to SQLite DB program." LANGUAGES CXX)

function(configure_target name)

	if( DISABLE_LIB_PRINTING )

    	target_compile_definitions(${name} PRIVATE DISABLE_LIB_PRINTING)

    	message(STATUS "Library print statements for ${name} are OFF.")

    else()

    	message(STATUS "Library print statements for ${name} are ON.")

	endif()

    set_target_properties(${name} PROPERTIES 
		COMPILE_WARNING_AS_ERROR ${WARNINGS_AS_ERRORS}
	)

	message(STATUS "Target: ${name} Warnings As Errors: ${WARNINGS_AS_ERRORS}")

	set(STATIC_FLAG "")

    set(ARCH_FLAG native)

	if ( BIN_TYPE STREQUAL "STATIC" )

		set(ARCH_FLAG x86-64-v3)

        set(STATIC_FLAG -static-libstdc++ -static-libgcc)

        message(STATUS "Static build for target: ${name}")

	endif()

	target_compile_options(${name} PRIVATE
		$<$<CONFIG:Release>: -ffast-math -march=${ARCH_FLAG} -mtune=native -fno-pic -fno-rtti -fno-stack-protector>

		$<$<CONFIG:RelWithDebInfo>: -g1 -fno-omit-frame-pointer>

		$<$<CONFIG:Debug>: -Og -ggdb -fno-omit-frame-pointer -ftrapv -ftrivial-auto-var-init=zero -fhardened -fsanitize=address,undefined>  # -fsanitize=thread, -fanalyzer

		-Wall -Wextra -Wpedantic -pedantic-errors -Wsuggest-override -Wduplicated-cond -Wcast-align -Wlogical-op -Wshadow
		#-Wfloat-equal -Weffc++ -Wconversion -Wsign-conversion -Wold-style-cast -Wcast-qual
	)

	target_link_options(${name} PRIVATE 
		$<$<CONFIG:Release>: -s>

		$<$<CONFIG:Debug>: -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now -fsanitize=address,undefined -static-libasan>

		${STATIC_FLAG}

		-no-pie
	)

	target_compile_definitions(${name} PRIVATE 
		$<$<CONFIG:Debug>: _GLIBCXX_DEBUG>
	)

	set_target_properties(${name} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

	set_target_properties(${name} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)

	target_include_directories(${name} PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include/
	)

endfunction()

set(CMAKE_NO_VERBOSE)
set(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_POSITION_INDEPENDENT_CODE OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# set_property(GLOBAL PROPERTY RULE_MESSAGES OFF)

set(WARNINGS_AS_ERRORS ON)

set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

get_filename_component(COMPILER_DIR ${CMAKE_CXX_COMPILER} DIRECTORY)
get_filename_component(COMPILER_LIB_DIR "${COMPILER_DIR}/../lib64" ABSOLUTE)

if( EXISTS "${COMPILER_LIB_DIR}/libstdc++.so" )

    list(APPEND CMAKE_BUILD_RPATH "${COMPILER_LIB_DIR}")

endif()

option(BUILD_TESTS "Build unit tests using Catch2" OFF)

option(DISABLE_LIB_PRINTING "Disable library print statements." OFF)

set(BIN_TYPE "SHARED" CACHE STRING "Build type: STATIC or SHARED")

if( NOT CMAKE_BUILD_TYPE )

  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build (Debug, Release)." FORCE)

endif()

message(STATUS "CMake Version: ${CMAKE_VERSION} (Min Required: 3.28).")

message(STATUS "Generator: ${CMAKE_GENERATOR}")

message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

message(STATUS "Targets Binary Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

add_executable(${PROJECT_NAME})

if( CMAKE_CXX_COMPILER_LOADED )

	message(STATUS "Compiler path: ${CMAKE_CXX_COMPILER}")
	message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
	message(STATUS "Compiler version: ${CMAKE_CXX_COMPILER_VERSION}")
	message(STATUS "Compiler is part of GCC: ${CMAKE_COMPILER_IS_GNUCXX}")
	message(STATUS "Compiler extensions: ${CMAKE_CXX_EXTENSIONS}")

endif()

message(STATUS "C++ Version: ${CMAKE_CXX_STANDARD}")

configure_target(${PROJECT_NAME})

set(CMAKE_INCLUDE_CURRENT_DIR OFF)

file(GLOB SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

file(GLOB HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)

target_sources(${PROJECT_NAME} PRIVATE ${SOURCES} ${HEADERS} ${CMAKE_CURRENT_SOURCE_DIR}/${NAME}.cpp)

############################## THIRDPARTY ##############################

set(FETCHCONTENT_TRY_FIND_PACKAGE_WITH_FIND_PACKAGE ALWAYS)

find_package(OpenMP REQUIRED)

find_package(TBB QUIET)

if(TBB_FOUND)
    message(STATUS "TBB found! Enabling parallel algorithms.")
    set(TBB_LINK_FLAG TBB::tbb)
else()
    message(STATUS "TBB not found. Falling back to sequential algorithms.")
    set(TBB_LINK_FLAG "")
endif()

find_package(Boost QUIET CONFIG)

if( Boost_FOUND )

    message(STATUS "----------------------------------------")
    message(STATUS "Boost successfully found!")
    message(STATUS "Boost Version: ${Boost_VERSION}")
    message(STATUS "Boost Include Dir: ${Boost_INCLUDE_DIRS}")
    message(STATUS "----------------------------------------")

else()

    message(WARNING "----------------------------------------")
    message(WARNING "Boost was NOT found on this system.")
    message(WARNING "----------------------------------------")

endif()

include(FetchContent)

FetchContent_Declare(
    SQLiteCpp
    GIT_REPOSITORY https://github.com/SRombauts/SQLiteCpp.git
    GIT_TAG        3.3.3
)

FetchContent_MakeAvailable(SQLiteCpp)

set(LIBDEFLATE_BUILD_GZIP OFF CACHE BOOL "" FORCE)
set(LIBDEFLATE_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
set(LIBDEFLATE_BUILD_STATIC_LIB ON CACHE BOOL "" FORCE)
set(LIBDEFLATE_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    libdeflate
    GIT_REPOSITORY https://github.com/ebiggers/libdeflate.git
    GIT_TAG        v1.25
)

FetchContent_MakeAvailable(libdeflate)

set_target_properties(libdeflate_static PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)

########################################################################

target_link_libraries(${PROJECT_NAME} PRIVATE SQLiteCpp OpenMP::OpenMP_CXX libdeflate::libdeflate_static Boost::headers ${TBB_LINK_FLAG})

if( BUILD_TESTS )

    enable_testing()
    
    FetchContent_Declare(
		Catch2
		GIT_REPOSITORY https://github.com/catchorg/Catch2.git
		GIT_TAG        v3.15.3
	)

	FetchContent_MakeAvailable(Catch2)

	set(TESTS_NAME test_${NAME})

	add_executable(${TESTS_NAME})

	configure_target(${TESTS_NAME})

	target_sources(${TESTS_NAME} PRIVATE ${SOURCES} ${HEADERS} ${CMAKE_CURRENT_SOURCE_DIR}/tests/${TESTS_NAME}.cpp)

	target_link_libraries(${TESTS_NAME} PRIVATE SQLiteCpp OpenMP::OpenMP_CXX libdeflate::libdeflate_static Boost::headers ${TBB_LINK_FLAG} Catch2::Catch2WithMain)

	include(Catch)

	catch_discover_tests(${TESTS_NAME})

endif()
