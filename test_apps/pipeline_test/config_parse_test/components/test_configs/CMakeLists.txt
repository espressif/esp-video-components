# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly

set(build_config_dir ${CMAKE_BINARY_DIR}/config)

file(GLOB good_files_list good/*)
file(REMOVE ${build_config_dir}/good_lists.h)
set(name_lists "")
foreach(file_name ${good_files_list})
file(READ ${file_name} file_data)
get_filename_component(file_name ${file_name} NAME)
string(REPLACE "\"" "\\\"" file_data ${file_data})
string(REPLACE "\n" " \\\n" file_data ${file_data})
string(REPLACE "." "_" file_c_name "${file_name}")
set(data "const char ${file_c_name}[] = \"${file_data}\"\;\r\n\r\n")
file(APPEND ${build_config_dir}/good_lists.h ${data})
endforeach()

# add_compile_definitions(GOOD_TEST_FILES=\"good_lists.h\")

file(GLOB bad_files_list bad/*)
file(REMOVE ${build_config_dir}/bad_lists.h)
set(name_lists "")
foreach(file_name ${bad_files_list})
file(READ ${file_name} file_data)
get_filename_component(file_name ${file_name} NAME)
string(REPLACE "\"" "\\\"" file_data ${file_data})
string(REPLACE "\n" " \\\n" file_data ${file_data})
string(REPLACE "." "_" file_c_name "${file_name}")
set(data "#define ${file_c_name} \"${file_data}\"\r\n\r\n")
file(APPEND ${build_config_dir}/bad_lists.h ${data})
set(data "{${file_c_name}, \"${file_name}\"},\r\n")
list(APPEND name_lists "{${file_c_name}, \"${file_name}\"},\r\n")
endforeach()

file(APPEND ${build_config_dir}/bad_lists.h "static const struct file_list_t bad_file_lists[] = {\r\n")
file(APPEND ${build_config_dir}/bad_lists.h ${name_lists})
file(APPEND ${build_config_dir}/bad_lists.h "};\r\n")

# add_compile_definitions(BAD_TEST_FILES=\"bad_lists.h\")

idf_component_register()
