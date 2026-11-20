cmake_minimum_required(VERSION 3.25)

file(REAL_PATH "${SOURCE_DIR}" source_root)
file(REAL_PATH "${BINARY_DIR}" binary_root)
set(preset_build_root "${source_root}/build")
file(READ "${binary_root}/compile_commands.json" database)
string(JSON count LENGTH "${database}")

set(sources)
set(index 0)
while(index LESS count)
  string(JSON source GET "${database}" "${index}" file)
  string(JSON directory GET "${database}" "${index}" directory)
  math(EXPR index "${index} + 1")
  file(REAL_PATH "${source}" source BASE_DIRECTORY "${directory}")
  cmake_path(IS_PREFIX source_root "${source}" owned)
  cmake_path(IS_PREFIX binary_root "${source}" in_current_build)
  cmake_path(IS_PREFIX preset_build_root "${source}" in_preset_build)
  if(owned AND NOT in_current_build AND NOT in_preset_build
      AND NOT source MATCHES "/(_deps|CMakeFiles)/")
    list(APPEND sources "${source}")
  endif()
endwhile()
if(NOT sources)
  message(FATAL_ERROR "No project translation units in ${binary_root}/compile_commands.json")
endif()
list(REMOVE_DUPLICATES sources)

# Treat source and build directory names literally in clang-tidy POSIX regexes.
foreach(path IN ITEMS source_root binary_root)
  foreach(character IN ITEMS "\\" "." "^" "$" "*" "+" "?" "(" ")" "[" "]" "{" "}" "|")
    string(REPLACE "${character}" "\\${character}" ${path} "${${path}}")
  endforeach()
endforeach()
execute_process(COMMAND "${CLANG_TIDY}" -p "${BINARY_DIR}"
  "--header-filter=^${source_root}/"
  "--exclude-header-filter=^(${binary_root}|${source_root}/build)/|/(_deps|CMakeFiles)/"
  ${sources} COMMAND_ERROR_IS_FATAL ANY)
