FILE(WRITE "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/conftest.c"
  "#include <sqlext.h>                   \n"
  "void main(void){                      \n"
  "  int odbc_version = SQL_OV_ODBC3_80; \n"
  "}                                       "
  )

EXECUTE_PROCESS(COMMAND ${CMAKE_C_COMPILER} conftest.c -I${ODBC_INCLUDE_DIRECTORIES}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp
  OUTPUT_VARIABLE OUTPUT
  RESULT_VARIABLE RESULT
  ERROR_VARIABLE ERROR)

IF(RESULT)
  MESSAGE(FATAL_ERROR "unixodbc >= 2.3.0 required.\n RESULT=${RESULT} OUTPUT=${OUTPUT} ERROR=${ERROR}")
ENDIF(RESULT)
