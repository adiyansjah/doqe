# Generate the actual link paths so the checker does not rely on archive names.
function(doqe_generate_link_inputs target)
  set(inputs "$<TARGET_FILE:${target}>")
  foreach(library IN LISTS ARGN)
    list(APPEND inputs "$<TARGET_FILE:${library}>")
  endforeach()
  list(APPEND inputs ${libcxx_archive} ${libcxxabi_archive} ${libunwind_archive})
  list(JOIN inputs "\n" inputs)
  file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${target}-link-inputs.txt"
    CONTENT "${inputs}\n")
endfunction()

function(doqe_add_module_boundary_checks)
  # Keep the checker in sync with the target declarations, including visibility.
  list(JOIN doqe_module_edges "\n" module_edges)
  file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/module-edges.txt" CONTENT "${module_edges}\n")

  set(module_check_targets doqe)
  foreach(module IN LISTS doqe_modules)
    list(APPEND module_check_targets "doqe_${module}")
  endforeach()
  doqe_generate_link_inputs(doqe ${doqe_link_closure_cli})
  if(BUILD_TESTING)
    list(APPEND module_check_targets doqe_tests)
    get_property(links TARGET doqe_tests PROPERTY LINK_LIBRARIES)
    set(test_links)
    set(test_libraries)
    set(support_links)
    if(TARGET doqe_test_support)
      list(APPEND module_check_targets doqe_test_support)
      get_property(support_libraries TARGET doqe_test_support PROPERTY LINK_LIBRARIES)
      get_property(support_interface TARGET doqe_test_support PROPERTY INTERFACE_LINK_LIBRARIES)
      foreach(link IN LISTS support_libraries)
        if(link MATCHES "^GTest::gtest(_main)?$")
          list(APPEND support_archives "${link}" GTest::gtest)
        else()
          string(REGEX REPLACE "^doqe_" "" module "${link}")
          list(APPEND support_links "${module}")
          list(APPEND support_archives ${doqe_link_closure_${module}})
          if(link IN_LIST support_interface)
            list(APPEND support_public_links "${module}")
          endif()
        endif()
      endforeach()
    endif()
    foreach(link IN LISTS links)
      if(link MATCHES "^GTest::gtest(_main)?$")
        list(APPEND test_libraries "${link}" GTest::gtest)
      elseif(link STREQUAL "doqe_test_support")
        list(APPEND test_libraries doqe_test_support ${support_archives})
        set(test_support_public_links "${support_public_links}")
      else()
        string(REGEX REPLACE "^doqe_" "" module "${link}")
        list(APPEND test_links "${module}")
        list(APPEND test_libraries ${doqe_link_closure_${module}})
      endif()
    endforeach()
    if(NOT TARGET doqe_test_support)
      set(support_links "${test_links}")
    endif()
    list(REMOVE_DUPLICATES test_libraries)
    doqe_generate_link_inputs(doqe_tests ${test_libraries})
  endif()
  add_custom_target(module-boundaries
    COMMAND "${CMAKE_COMMAND}"
      "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      "-DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
      "-DNINJA=${CMAKE_MAKE_PROGRAM}"
      "-DCLANG=${CMAKE_CXX_COMPILER}"
      "-DC_COMPILER=${CMAKE_C_COMPILER}"
      "-DBUILD_TESTING=${BUILD_TESTING}"
      "-DTEST_LINKS=${test_links}"
      "-DSUPPORT_LINKS=${support_links}"
      "-DTEST_SUPPORT_PUBLIC_LINKS=${test_support_public_links}"
      "-DGOOGLETEST_SOURCE_DIR=${googletest_SOURCE_DIR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check-module-includes.cmake"
    DEPENDS ${module_check_targets}
    VERBATIM)
  if(BUILD_TESTING)
    add_test(NAME Modules.IncludeBoundaries COMMAND "${CMAKE_COMMAND}"
      --build "${CMAKE_CURRENT_BINARY_DIR}" --target module-boundaries)
    set_tests_properties(Modules.IncludeBoundaries PROPERTIES RUN_SERIAL TRUE)
    add_test(NAME Modules.AdrEdges COMMAND "${CMAKE_COMMAND}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check-module-edges.cmake")
  endif()
endfunction()
cmake_language(DEFER CALL doqe_add_module_boundary_checks)
