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
CMAKE_SOURCE_DIR = /home/user/Desktop/mmap/subscriber

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/user/Desktop/mmap/subscriber/build

# Include any dependencies generated for this target.
include CMakeFiles/subscriber_mmap.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/subscriber_mmap.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/subscriber_mmap.dir/flags.make

CMakeFiles/subscriber_mmap.dir/subscriber_mmap.c.o: CMakeFiles/subscriber_mmap.dir/flags.make
CMakeFiles/subscriber_mmap.dir/subscriber_mmap.c.o: ../subscriber_mmap.c
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/user/Desktop/mmap/subscriber/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building C object CMakeFiles/subscriber_mmap.dir/subscriber_mmap.c.o"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -o CMakeFiles/subscriber_mmap.dir/subscriber_mmap.c.o   -c /home/user/Desktop/mmap/subscriber/subscriber_mmap.c

CMakeFiles/subscriber_mmap.dir/subscriber_mmap.c.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing C source to CMakeFiles/subscriber_mmap.dir/subscriber_mmap.c.i"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -E /home/user/Desktop/mmap/subscriber/subscriber_mmap.c > CMakeFiles/subscriber_mmap.dir/subscriber_mmap.c.i

CMakeFiles/subscriber_mmap.dir/subscriber_mmap.c.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling C source to assembly CMakeFiles/subscriber_mmap.dir/subscriber_mmap.c.s"
	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -S /home/user/Desktop/mmap/subscriber/subscriber_mmap.c -o CMakeFiles/subscriber_mmap.dir/subscriber_mmap.c.s

# Object files for target subscriber_mmap
subscriber_mmap_OBJECTS = \
"CMakeFiles/subscriber_mmap.dir/subscriber_mmap.c.o"

# External object files for target subscriber_mmap
subscriber_mmap_EXTERNAL_OBJECTS =

subscriber_mmap: CMakeFiles/subscriber_mmap.dir/subscriber_mmap.c.o
subscriber_mmap: CMakeFiles/subscriber_mmap.dir/build.make
subscriber_mmap: CMakeFiles/subscriber_mmap.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/user/Desktop/mmap/subscriber/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking C executable subscriber_mmap"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/subscriber_mmap.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/subscriber_mmap.dir/build: subscriber_mmap

.PHONY : CMakeFiles/subscriber_mmap.dir/build

CMakeFiles/subscriber_mmap.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/subscriber_mmap.dir/cmake_clean.cmake
.PHONY : CMakeFiles/subscriber_mmap.dir/clean

CMakeFiles/subscriber_mmap.dir/depend:
	cd /home/user/Desktop/mmap/subscriber/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/user/Desktop/mmap/subscriber /home/user/Desktop/mmap/subscriber /home/user/Desktop/mmap/subscriber/build /home/user/Desktop/mmap/subscriber/build /home/user/Desktop/mmap/subscriber/build/CMakeFiles/subscriber_mmap.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/subscriber_mmap.dir/depend

