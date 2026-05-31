function(cppx_transpile out_var)
  set(outputs)
  foreach(input IN LISTS ARGN)
    get_filename_component(input_abs "${input}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(input_ext "${input_abs}" EXT)
    file(RELATIVE_PATH input_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${input_abs}")

    if(input_ext STREQUAL ".cppx")
      set(output_ext ".cpp")
    elseif(input_ext STREQUAL ".hx")
      set(output_ext ".h")
    else()
      message(FATAL_ERROR "cppx_transpile only supports .cppx and .hx: ${input}")
    endif()

    string(REGEX REPLACE "\\.(cppx|hx)$" "${output_ext}" output_rel "${input_rel}")
    set(output_abs "${CMAKE_CURRENT_BINARY_DIR}/generated/cppx/${output_rel}")
    get_filename_component(output_dir "${output_abs}" DIRECTORY)

    add_custom_command(
      OUTPUT "${output_abs}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
      COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/cppx_transpile.py"
        "${input_rel}"
        -o "${output_abs}"
      DEPENDS
        "${input_abs}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/cppx_transpile.py"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      VERBATIM
    )
    list(APPEND outputs "${output_abs}")
  endforeach()
  set("${out_var}" "${outputs}" PARENT_SCOPE)
endfunction()
