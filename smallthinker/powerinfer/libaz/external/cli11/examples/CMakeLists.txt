function(add_cli_exe T)
  add_executable(${T} ${ARGN})
  target_link_libraries(${T} PUBLIC CLI11)
  set_property(TARGET ${T} PROPERTY FOLDER "Examples")
  if(CLI11_FORCE_LIBCXX)
    set_property(
      TARGET ${T}
      APPEND_STRING
      PROPERTY LINK_FLAGS -stdlib=libc++)
  endif()
endfunction()

if(CLI11_BUILD_EXAMPLES_JSON)
  message(STATUS "Using nlohmann/json")
  FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.7.3/include.zip
    URL_HASH "SHA256=87b5884741427220d3a33df1363ae0e8b898099fbc59f1c451113f6732891014")

  FetchContent_GetProperties(json)
  if(NOT json_POPULATED)
    FetchContent_Populate(json)
  endif()

  add_cli_exe(json json.cpp)
  target_include_directories(json PUBLIC SYSTEM "${json_SOURCE_DIR}/single_include")

  add_test(NAME json_config_out COMMAND json --item 2)
  set_property(TEST json_config_out PROPERTY PASS_REGULAR_EXPRESSION [[{]] [["item": "2"]]
                                             [["simple": false]] [[}]])

  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/input.json" [=[{"item":3,"simple":false}]=])
  add_test(NAME json_config_in COMMAND json --config "${CMAKE_CURRENT_BINARY_DIR}/input.json")
  set_property(TEST json_config_in PROPERTY PASS_REGULAR_EXPRESSION [[{]] [["item": "3"]]
                                            [["simple": false]] [[}]])
endif()

add_cli_exe(simple simple.cpp)
add_test(NAME simple_basic COMMAND simple)
add_test(NAME simple_all COMMAND simple -f filename.txt -c 12 --flag --flag -d 1.2)
set_property(
  TEST simple_all
  PROPERTY PASS_REGULAR_EXPRESSION "Working on file: filename.txt, direct count: 1, opt count: 1"
           "Working on count: 12, direct count: 1, opt count: 1" "Received flag: 2 (2) times"
           "Some value: 1.2")

add_test(NAME simple_version COMMAND simple --version)
set_property(TEST simple_version PROPERTY PASS_REGULAR_EXPRESSION "${CLI11_VERSION}")

add_cli_exe(subcommands subcommands.cpp)
add_test(NAME subcommands_none COMMAND subcommands)
set_property(TEST subcommands_none PROPERTY PASS_REGULAR_EXPRESSION "A subcommand is required")
add_test(
  NAME subcommands_all
  COMMAND subcommands --random start --file
  name stop --count)
set_property(
  TEST subcommands_all
  PROPERTY PASS_REGULAR_EXPRESSION "Working on --file from start: name"
           "Working on --count from stop: 1, direct count: 1" "Count of --random flag: 1"
           "Subcommand: start" "Subcommand: stop")

add_cli_exe(subcom_partitioned subcom_partitioned.cpp)
add_test(NAME subcom_partitioned_none COMMAND subcom_partitioned)
set_property(
  TEST subcom_partitioned_none
  PROPERTY PASS_REGULAR_EXPRESSION "This is a timer:" "--file is required"
           "Run with --help for more information.")

add_test(NAME subcom_partitioned_all COMMAND subcom_partitioned --file this --count --count -d 1.2)
set_property(
  TEST subcom_partitioned_all
  PROPERTY PASS_REGULAR_EXPRESSION "This is a timer:"
           "Working on file: this, direct count: 1, opt count: 1"
           "Working on count: 2, direct count: 2, opt count: 2" "Some value: 1.2")
# test shows that the help prints out for unnamed subcommands
add_test(NAME subcom_partitioned_help COMMAND subcom_partitioned --help)
set_property(TEST subcom_partitioned_help
             PROPERTY PASS_REGULAR_EXPRESSION "-f,[ \\t]*--file TEXT REQUIRED" "-d,--double FLOAT")

####################################################
add_cli_exe(config_app config_app.cpp)
add_test(NAME config_app1 COMMAND config_app -p)
set_property(TEST config_app1 PROPERTY PASS_REGULAR_EXPRESSION "file=")

add_test(NAME config_app2 COMMAND config_app -p -f /)
set_property(TEST config_app2 PROPERTY PASS_REGULAR_EXPRESSION "file=\"/\"")

add_test(NAME config_app3 COMMAND config_app -f "" -p)
set_property(TEST config_app3 PROPERTY PASS_REGULAR_EXPRESSION "file=\"\"")

add_test(NAME config_app4 COMMAND config_app -f "/" -p)
set_property(TEST config_app4 PROPERTY PASS_REGULAR_EXPRESSION "file=\"/\"")

####################################################

add_cli_exe(option_groups option_groups.cpp)
add_test(NAME option_groups_missing COMMAND option_groups)
set_property(TEST option_groups_missing PROPERTY PASS_REGULAR_EXPRESSION "Exactly 1 option from"
                                                 "is required")
add_test(NAME option_groups_extra COMMAND option_groups --csv --binary)
set_property(TEST option_groups_extra PROPERTY PASS_REGULAR_EXPRESSION "but 2 were given")
add_test(NAME option_groups_extra2 COMMAND option_groups --csv --address "192.168.1.1" -o
                                           "test.out")
set_property(TEST option_groups_extra2 PROPERTY PASS_REGULAR_EXPRESSION "at most 1")

add_cli_exe(positional_arity positional_arity.cpp)
add_test(NAME positional_arity1 COMMAND positional_arity one)
set_property(TEST positional_arity1 PROPERTY PASS_REGULAR_EXPRESSION "File 1 = one")
add_test(NAME positional_arity2 COMMAND positional_arity one two)
set_property(TEST positional_arity2 PROPERTY PASS_REGULAR_EXPRESSION "File 1 = one" "File 2 = two")
add_test(NAME positional_arity3 COMMAND positional_arity 1 2 one)
set_property(TEST positional_arity3 PROPERTY PASS_REGULAR_EXPRESSION "File 1 = one")
add_test(NAME positional_arity_fail COMMAND positional_arity 1 one two)
set_property(TEST positional_arity_fail PROPERTY PASS_REGULAR_EXPRESSION "Could not convert")

add_cli_exe(positional_validation positional_validation.cpp)
add_test(NAME positional_validation1 COMMAND positional_validation one)
set_property(TEST positional_validation1 PROPERTY PASS_REGULAR_EXPRESSION "File 1 = one")
add_test(NAME positional_validation2 COMMAND positional_validation one 1 2 two)
set_property(TEST positional_validation2 PROPERTY PASS_REGULAR_EXPRESSION "File 1 = one"
                                                  "File 2 = two")
add_test(NAME positional_validation3 COMMAND positional_validation 1 2 one)
set_property(TEST positional_validation3 PROPERTY PASS_REGULAR_EXPRESSION "File 1 = one")
add_test(NAME positional_validation4 COMMAND positional_validation 1 one two 2)
set_property(TEST positional_validation4 PROPERTY PASS_REGULAR_EXPRESSION "File 1 = one"
                                                  "File 2 = two")

add_cli_exe(shapes shapes.cpp)
add_test(NAME shapes_all COMMAND shapes circle 4.4 circle 10.7 rectangle 4 4 circle 2.3 triangle
                                 4.5 ++ rectangle 2.1 ++ circle 234.675)
set_property(
  TEST shapes_all PROPERTY PASS_REGULAR_EXPRESSION "circle2" "circle4"
                           "rectangle2 with edges [2.1,2.1]" "triangle1 with sides [4.5]")

add_cli_exe(ranges ranges.cpp)
add_test(NAME ranges_range COMMAND ranges --range 1 2 3)
set_property(TEST ranges_range PROPERTY PASS_REGULAR_EXPRESSION "[2:1:3]")
add_test(NAME ranges_minmax COMMAND ranges --min 2 --max 3)
set_property(TEST ranges_minmax PROPERTY PASS_REGULAR_EXPRESSION "[2:1:3]")
add_test(NAME ranges_error COMMAND ranges --min 2 --max 3 --step 1 --range 1 2 3)
set_property(TEST ranges_error PROPERTY PASS_REGULAR_EXPRESSION "Exactly 1 option from")

add_cli_exe(validators validators.cpp)
add_test(NAME validators_help COMMAND validators --help)
set_property(
  TEST validators_help
  PROPERTY PASS_REGULAR_EXPRESSION "  -f,[ \\t]*--file TEXT:FILE[\\r\\n\\t ]+File name"
           "  -v,[ \\t]*--value INT:INT in [3 - 6][\\r\\n\\t ]+Value in range")
add_test(NAME validators_file COMMAND validators --file nonex.xxx)
set_property(
  TEST validators_file PROPERTY PASS_REGULAR_EXPRESSION "--file: File does not exist: nonex.xxx"
                                "Run with --help for more information.")
add_test(NAME validators_plain COMMAND validators --value 9)
set_property(
  TEST validators_plain PROPERTY PASS_REGULAR_EXPRESSION "--value: Value 9 not in range 3 to 6"
                                 "Run with --help for more information.")

add_cli_exe(groups groups.cpp)
add_test(NAME groups_none COMMAND groups)
set_property(
  TEST groups_none PROPERTY PASS_REGULAR_EXPRESSION "This is a timer:" "--file is required"
                            "Run with --help for more information.")
add_test(NAME groups_all COMMAND groups --file this --count --count -d 1.2)
set_property(
  TEST groups_all
  PROPERTY PASS_REGULAR_EXPRESSION "This is a timer:"
           "Working on file: this, direct count: 1, opt count: 1"
           "Working on count: 2, direct count: 2, opt count: 2" "Some value: 1.2")

add_cli_exe(inter_argument_order inter_argument_order.cpp)
add_test(NAME inter_argument_order COMMAND inter_argument_order --foo 1 2 3 --x --bar 4 5 6 --z
                                           --foo 7 8)
set_property(
  TEST inter_argument_order
  PROPERTY
    PASS_REGULAR_EXPRESSION
    [=[foo : 1
foo : 2
foo : 3
bar : 4
bar : 5
bar : 6
foo : 7
foo : 8]=])

add_cli_exe(prefix_command prefix_command.cpp)
add_test(NAME prefix_command COMMAND prefix_command -v 3 2 1 -- other one two 3)
set_property(TEST prefix_command PROPERTY PASS_REGULAR_EXPRESSION "Prefix: 3 : 2 : 1"
                                          "Remaining commands: other one two 3")

add_cli_exe(arg_capture arg_capture.cpp)
add_test(NAME arg_capture COMMAND arg_capture -v 27 --sub -v 13 --val prefix)
set_property(TEST arg_capture PROPERTY PASS_REGULAR_EXPRESSION "value=27")
add_test(NAME arg_capture2 COMMAND arg_capture -v 27 --sub -v 13 --val prefix)
set_property(TEST arg_capture2 PROPERTY PASS_REGULAR_EXPRESSION "after Args:-v 13 --val prefix")

add_cli_exe(callback_passthrough callback_passthrough.cpp)
add_test(NAME callback_passthrough1 COMMAND callback_passthrough --argname t2 --t2 test)
set_property(TEST callback_passthrough1 PROPERTY PASS_REGULAR_EXPRESSION "the value is now test")
add_test(NAME callback_passthrough2 COMMAND callback_passthrough --arg EEEK --argname arg)
set_property(TEST callback_passthrough2 PROPERTY PASS_REGULAR_EXPRESSION "the value is now EEEK")

add_cli_exe(enum enum.cpp)
add_test(NAME enum_pass COMMAND enum -l 1)
add_test(NAME enum_fail COMMAND enum -l 4)
set_property(TEST enum_fail PROPERTY PASS_REGULAR_EXPRESSION "--level: Check 4 value in {"
                                     "FAILED")

add_cli_exe(enum_ostream enum_ostream.cpp)
add_test(NAME enum_ostream_pass COMMAND enum_ostream --level medium)
set_property(TEST enum_ostream_pass PROPERTY PASS_REGULAR_EXPRESSION "Enum received: Medium")

add_cli_exe(digit_args digit_args.cpp)
add_test(NAME digit_args COMMAND digit_args -h)
set_property(TEST digit_args PROPERTY PASS_REGULAR_EXPRESSION "-3{3}")

add_cli_exe(modhelp modhelp.cpp)
add_test(NAME modhelp COMMAND modhelp -a test -h)
set_property(TEST modhelp PROPERTY PASS_REGULAR_EXPRESSION "Option -a string in help: test")

add_subdirectory(subcom_in_files)
add_test(NAME subcom_in_files COMMAND subcommand_main subcommand_a -f this.txt --with-foo)
set_property(TEST subcom_in_files PROPERTY PASS_REGULAR_EXPRESSION "Working on file: this\.txt"
                                           "Using foo!")

add_cli_exe(formatter formatter.cpp)

add_cli_exe(nested nested.cpp)

add_cli_exe(subcom_help subcom_help.cpp)
add_test(NAME subcom_help_normal COMMAND subcom_help sub --help)
add_test(NAME subcom_help_reversed COMMAND subcom_help --help sub)

add_cli_exe(retired retired.cpp)
add_test(NAME retired_retired_test COMMAND retired --retired_option)
add_test(NAME retired_retired_test2 COMMAND retired --retired_option 567)
add_test(NAME retired_retired_test3 COMMAND retired --retired_option2 567 689 789)
add_test(NAME retired_deprecated COMMAND retired --deprecate 19 20)

set_property(TEST retired_retired_test PROPERTY PASS_REGULAR_EXPRESSION "WARNING.*retired")

set_property(TEST retired_retired_test2 PROPERTY PASS_REGULAR_EXPRESSION "WARNING.*retired")

set_property(TEST retired_retired_test3 PROPERTY PASS_REGULAR_EXPRESSION "WARNING.*retired")

set_property(TEST retired_deprecated PROPERTY PASS_REGULAR_EXPRESSION "deprecated.*not_deprecated")

#--------------------------------------------
add_cli_exe(custom_parse custom_parse.cpp)
add_test(NAME cp_test COMMAND custom_parse --dv 1.7)
set_property(TEST cp_test PROPERTY PASS_REGULAR_EXPRESSION "called correct")

#-----------------------------------------------------
add_cli_exe(help_usage help_usage.cpp)
add_test(NAME help_use COMMAND help_usage --help)
set_property(TEST help_use PROPERTY PASS_REGULAR_EXPRESSION "[1..9]")

#------------------------------------------------
# This executable is for manual testing and is expected to change regularly

add_cli_exe(tester testEXE.cpp)
