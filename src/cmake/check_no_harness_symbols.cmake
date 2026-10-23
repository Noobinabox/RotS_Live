# Fails when the symbol table of the program at BINARY, as NM -C lists it, has any line that
# names namespace harness. Run as: cmake -DNM=<nm> -DBINARY=<program> -P <this file>
#
# The live server must not contain the integration harness's code, such as its entry policy, so
# ageland is checked with this script after every build that runs the tests.

if(NOT NM OR NOT BINARY)
    message(FATAL_ERROR "Pass -DNM=<nm program> and -DBINARY=<program to check>.")
endif()

execute_process(
    COMMAND "${NM}" -C "${BINARY}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE symbol_listing
    ERROR_VARIABLE nm_errors
)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "${NM} could not list the symbols of ${BINARY} (${nm_result}): ${nm_errors}")
endif()

# Demangled names hold semicolons and square brackets, which CMake lists treat specially; replace
# them so each matching line counts as one list element.
string(REPLACE ";" "," symbol_listing "${symbol_listing}")
string(REPLACE "[" "(" symbol_listing "${symbol_listing}")
string(REPLACE "]" ")" symbol_listing "${symbol_listing}")
string(REGEX MATCHALL "[^\n]*harness::[^\n]*" harness_symbols "${symbol_listing}")
list(LENGTH harness_symbols harness_symbol_count)
if(harness_symbol_count GREATER 0)
    list(GET harness_symbols 0 first_harness_symbol)
    message(FATAL_ERROR
        "${BINARY} lists ${harness_symbol_count} harness:: symbols, the first: ${first_harness_symbol}")
endif()

message(STATUS "${BINARY} lists no harness:: symbols.")
