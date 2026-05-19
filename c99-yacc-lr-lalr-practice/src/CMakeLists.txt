add_library(yacc_core
    yacc/first/first_set.cpp
    yacc/lalr/lalr_builder.cpp
    yacc/lr1/lr1_items.cpp
    yacc/model/grammar.cpp
    yacc/parser/yacc_parser.cpp
    yacc/preprocess/grammar_preprocessor.cpp
    yacc/report/report_exporter.cpp
    yacc/runtime/lr_parser.cpp
    yacc/table/parse_table.cpp
)

target_include_directories(yacc_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(yacc_parse_tool
    tools/yacc_parse_main.cpp
)

target_link_libraries(yacc_parse_tool PRIVATE yacc_core)
