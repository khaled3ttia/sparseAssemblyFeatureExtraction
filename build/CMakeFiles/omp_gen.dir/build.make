# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.16

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/khaled/Documents/khaled/sparse_construction/sparseAssemblyFeatureExtraction

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/khaled/Documents/khaled/sparse_construction/sparseAssemblyFeatureExtraction/build

# Utility rule file for omp_gen.

# Include the progress variables for this target.
include CMakeFiles/omp_gen.dir/progress.make

omp_gen: CMakeFiles/omp_gen.dir/build.make

.PHONY : omp_gen

# Rule to build all files generated by this target.
CMakeFiles/omp_gen.dir/build: omp_gen

.PHONY : CMakeFiles/omp_gen.dir/build

CMakeFiles/omp_gen.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/omp_gen.dir/cmake_clean.cmake
.PHONY : CMakeFiles/omp_gen.dir/clean

CMakeFiles/omp_gen.dir/depend:
	cd /home/khaled/Documents/khaled/sparse_construction/sparseAssemblyFeatureExtraction/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/khaled/Documents/khaled/sparse_construction/sparseAssemblyFeatureExtraction /home/khaled/Documents/khaled/sparse_construction/sparseAssemblyFeatureExtraction /home/khaled/Documents/khaled/sparse_construction/sparseAssemblyFeatureExtraction/build /home/khaled/Documents/khaled/sparse_construction/sparseAssemblyFeatureExtraction/build /home/khaled/Documents/khaled/sparse_construction/sparseAssemblyFeatureExtraction/build/CMakeFiles/omp_gen.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/omp_gen.dir/depend

