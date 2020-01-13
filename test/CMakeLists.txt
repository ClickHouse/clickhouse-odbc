find_program(PERL perl)
find_program(PYTHON2 python2)
find_program(PYTHON3 python3)
find_program(SQLCMD sqlcmd)

set(PERL_USES_MDAC 0)
set(PERL_USES_UNIXODBC 0)
set(PERL_USES_IODBC 0)

if(PERL)
    execute_process(
        COMMAND ${PERL} ${CMAKE_CURRENT_SOURCE_DIR}/detect_driver_manager.pl
        OUTPUT_VARIABLE _perl_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
#       ERROR_QUIET
    )
    message(STATUS "ODBC Driver Manager used by Perl's DBD::ODBC: ${_perl_output}")
    if(_perl_output MATCHES "Microsoft")
        set(PERL_USES_MDAC 1)
    endif()
    if(_perl_output MATCHES "unixODBC")
        set(PERL_USES_UNIXODBC 1)
    endif()
    if(_perl_output MATCHES "iODBC")
        set(PERL_USES_IODBC 1)
    endif()
endif()

set(PYTHON_USES_MDAC 0)
set(PYTHON_USES_UNIXODBC 0)
set(PYTHON_USES_IODBC 0)

if(PYTHON2)
    execute_process(
        COMMAND ${PYTHON2} ${CMAKE_CURRENT_SOURCE_DIR}/detect_driver_manager.py
        OUTPUT_VARIABLE _python_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
#       ERROR_QUIET
    )
    message(STATUS "ODBC Driver Manager used by Python 2's pyodbc: ${_python_output}")
    if(_python_output MATCHES "Microsoft")
        set(PYTHON2_USES_MDAC 1)
    endif()
    if(_python_output MATCHES "unixODBC")
        set(PYTHON2_USES_UNIXODBC 1)
    endif()
    if(_python_output MATCHES "iODBC")
        set(PYTHON2_USES_IODBC 1)
    endif()
endif()

set(PYTHON3_USES_MDAC 0)
set(PYTHON3_USES_UNIXODBC 0)
set(PYTHON3_USES_IODBC 0)

if(PYTHON3)
    execute_process(
        COMMAND ${PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/detect_driver_manager.py
        OUTPUT_VARIABLE _python_output
        OUTPUT_STRIP_TRAILING_WHITESPACE
#       ERROR_QUIET
    )
    message(STATUS "ODBC Driver Manager used by Python 3's pyodbc: ${_python_output}")
    if(_python_output MATCHES "Microsoft")
        set(PYTHON3_USES_MDAC 1)
    endif()
    if(_python_output MATCHES "unixODBC")
        set(PYTHON3_USES_UNIXODBC 1)
    endif()
    if(_python_output MATCHES "iODBC")
        set(PYTHON3_USES_IODBC 1)
    endif()
endif()

message(STATUS "Testing with:\n\t"
    "TEST_DSN=${TEST_DSN}\n\t"
    "TEST_DSN_W=${TEST_DSN_W}\n\t"
    "PERL=${PERL}\n\t"
    "PERL_USES_MDAC=${PERL_USES_MDAC}\n\t"
    "PERL_USES_UNIXODBC=${PERL_USES_UNIXODBC}\n\t"
    "PERL_USES_IODBC=${PERL_USES_IODBC}\n\t"
    "PYTHON2=${PYTHON2}\n\t"
    "PYTHON2_USES_MDAC=${PYTHON2_USES_MDAC}\n\t"
    "PYTHON2_USES_UNIXODBC=${PYTHON2_USES_UNIXODBC}\n\t"
    "PYTHON2_USES_IODBC=${PYTHON2_USES_IODBC}\n\t"
    "PYTHON3=${PYTHON3}\n\t"
    "PYTHON3_USES_MDAC=${PYTHON3_USES_MDAC}\n\t"
    "PYTHON3_USES_UNIXODBC=${PYTHON3_USES_UNIXODBC}\n\t"
    "PYTHON3_USES_IODBC=${PYTHON3_USES_IODBC}\n\t"
    "ODBC_UNIXODBC_ISQL=${ODBC_UNIXODBC_ISQL}\n\t"
    "ODBC_UNIXODBC_IUSQL=${ODBC_UNIXODBC_IUSQL}\n\t"
    "ODBC_IODBC_IODBCTEST=${ODBC_IODBC_IODBCTEST}\n\t"
    "ODBC_IODBC_IODBCTESTW=${ODBC_IODBC_IODBCTESTW}\n\t"
    "SQLCMD=${SQLCMD}"
)

set(SQL_QUERY "123456+456789" CACHE STRING "")
set(SQL_ANSWER "580245" CACHE STRING "")

if(TEST_DSN)
    if(PERL AND (
            ("${ODBC_PROVIDER}" STREQUAL "MDAC" AND PERL_USES_MDAC) OR
            ("${ODBC_PROVIDER}" STREQUAL "UnixODBC" AND PERL_USES_UNIXODBC) OR
            ("${ODBC_PROVIDER}" STREQUAL "iODBC" AND PERL_USES_IODBC)
    ))
        add_test(NAME "test.pl-a-a" COMMAND ${PERL} ${CMAKE_CURRENT_SOURCE_DIR}/test.pl "${TEST_DSN}" a)
        add_test(NAME "test.pl-w-a" COMMAND ${PERL} ${CMAKE_CURRENT_SOURCE_DIR}/test.pl "${TEST_DSN}" w)
    endif()
    if(PYTHON2 AND (
            ("${ODBC_PROVIDER}" STREQUAL "MDAC" AND PYTHON2_USES_MDAC) OR
            ("${ODBC_PROVIDER}" STREQUAL "UnixODBC" AND PYTHON2_USES_UNIXODBC) OR
            ("${ODBC_PROVIDER}" STREQUAL "iODBC" AND PYTHON2_USES_IODBC)
    ))
        add_test(NAME "test.py-2-a-a" COMMAND ${PYTHON2} ${CMAKE_CURRENT_SOURCE_DIR}/test.py "${TEST_DSN}" a)
        add_test(NAME "test.py-2-w-a" COMMAND ${PYTHON2} ${CMAKE_CURRENT_SOURCE_DIR}/test.py "${TEST_DSN}" w)
    endif()
    if(PYTHON3 AND (
            ("${ODBC_PROVIDER}" STREQUAL "MDAC" AND PYTHON3_USES_MDAC) OR
            ("${ODBC_PROVIDER}" STREQUAL "UnixODBC" AND PYTHON3_USES_UNIXODBC) OR
            ("${ODBC_PROVIDER}" STREQUAL "iODBC" AND PYTHON3_USES_IODBC)
    ))
        add_test(NAME "test.py-3-a-a" COMMAND ${PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/test.py "${TEST_DSN}" a)
        add_test(NAME "test.py-3-w-a" COMMAND ${PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/test.py "${TEST_DSN}" w)
        add_test(NAME "parametrized-regression.py-3-a" COMMAND ${CMAKE_COMMAND} -E env DSN="${TEST_DSN}" ${PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/parameterized/regression.py)
    endif()
    if(ODBC_UNIXODBC_ISQL AND "${ODBC_PROVIDER}" STREQUAL "UnixODBC")
        add_test(NAME "isql-a" COMMAND sh -c "echo select ${SQL_QUERY} | ${ODBC_UNIXODBC_ISQL} '${TEST_DSN}' | grep ${SQL_ANSWER}")
        add_test(NAME "test.sh-isql-a" COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${ODBC_UNIXODBC_ISQL} \"${TEST_DSN}\" -v -b)
    endif()
    if(ODBC_UNIXODBC_IUSQL AND "${ODBC_PROVIDER}" STREQUAL "UnixODBC")
        add_test(NAME "iusql-a" COMMAND sh -c "echo select ${SQL_QUERY} | ${ODBC_UNIXODBC_IUSQL} '${TEST_DSN}' | grep ${SQL_ANSWER}")
        add_test(NAME "test.sh-iusql-a" COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${ODBC_UNIXODBC_IUSQL} \"${TEST_DSN}\" -v -b)
    endif()
    if(ODBC_IODBC_IODBCTEST AND "${ODBC_PROVIDER}" STREQUAL "iODBC")
        add_test(NAME "iodbctest-a" COMMAND sh -c "echo select ${SQL_QUERY} | ${ODBC_IODBC_IODBCTEST} 'DSN=${TEST_DSN}' | grep ${SQL_ANSWER}")
        add_test(NAME "test.sh-iodbctest-a" COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${ODBC_IODBC_IODBCTEST} \"DSN=${TEST_DSN}\")
    endif()
    if(ODBC_IODBC_IODBCTESTW AND "${ODBC_PROVIDER}" STREQUAL "iODBC")
        add_test(NAME "iodbctestw-a" COMMAND sh -c "echo select ${SQL_QUERY} | ${ODBC_IODBC_IODBCTESTW} 'DSN=${TEST_DSN}' | grep ${SQL_ANSWER}")
        add_test(NAME "test.sh-iodbctestw-a" COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${ODBC_IODBC_IODBCTESTW} \"DSN=${TEST_DSN}\")
    endif()
endif()

if(TEST_DSN_W)
    if(PERL AND (
            ("${ODBC_PROVIDER}" STREQUAL "MDAC" AND PERL_USES_MDAC) OR
            ("${ODBC_PROVIDER}" STREQUAL "UnixODBC" AND PERL_USES_UNIXODBC) OR
            ("${ODBC_PROVIDER}" STREQUAL "iODBC" AND PERL_USES_IODBC)
    ))
        add_test(NAME "test.pl-a-w" COMMAND ${PERL} ${CMAKE_CURRENT_SOURCE_DIR}/test.pl "${TEST_DSN_W}" a)
        add_test(NAME "test.pl-w-w" COMMAND ${PERL} ${CMAKE_CURRENT_SOURCE_DIR}/test.pl "${TEST_DSN_W}" w)
    endif()
    if(PYTHON2 AND (
            ("${ODBC_PROVIDER}" STREQUAL "MDAC" AND PYTHON2_USES_MDAC) OR
            ("${ODBC_PROVIDER}" STREQUAL "UnixODBC" AND PYTHON2_USES_UNIXODBC) OR
            ("${ODBC_PROVIDER}" STREQUAL "iODBC" AND PYTHON2_USES_IODBC)
    ))
        add_test(NAME "test.py-2-a-w" COMMAND ${PYTHON2} ${CMAKE_CURRENT_SOURCE_DIR}/test.py "${TEST_DSN_W}" a)
        add_test(NAME "test.py-2-w-w" COMMAND ${PYTHON2} ${CMAKE_CURRENT_SOURCE_DIR}/test.py "${TEST_DSN_W}" w)
    endif()
    if(PYTHON3 AND (
            ("${ODBC_PROVIDER}" STREQUAL "MDAC" AND PYTHON3_USES_MDAC) OR
            ("${ODBC_PROVIDER}" STREQUAL "UnixODBC" AND PYTHON3_USES_UNIXODBC) OR
            ("${ODBC_PROVIDER}" STREQUAL "iODBC" AND PYTHON3_USES_IODBC)
    ))
        add_test(NAME "test.py-3-a-w" COMMAND ${PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/test.py "${TEST_DSN_W}" a)
        add_test(NAME "test.py-3-w-w" COMMAND ${PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/test.py "${TEST_DSN_W}" w)
        add_test(NAME "parametrized-regression.py-3-w" COMMAND ${CMAKE_COMMAND} -E env DSN="${TEST_DSN_W}" ${PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/parameterized/regression.py)
    endif()
    if(ODBC_UNIXODBC_ISQL AND "${ODBC_PROVIDER}" STREQUAL "UnixODBC")
        add_test(NAME "isql-w" COMMAND sh -c "echo select ${SQL_QUERY} | ${ODBC_UNIXODBC_ISQL} '${TEST_DSN_W}' | grep ${SQL_ANSWER}")
        add_test(NAME "test.sh-isql-w" COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${ODBC_UNIXODBC_ISQL} \"${TEST_DSN_W}\" -v -b)
    endif()
    if(ODBC_UNIXODBC_IUSQL AND "${ODBC_PROVIDER}" STREQUAL "UnixODBC")
        add_test(NAME "iusql-w" COMMAND sh -c "echo select ${SQL_QUERY} | ${ODBC_UNIXODBC_IUSQL} '${TEST_DSN_W}' | grep ${SQL_ANSWER}")
        add_test(NAME "test.sh-iusql-w" COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${ODBC_UNIXODBC_IUSQL} \"${TEST_DSN_W}\" -v -b)
    endif()
    if(ODBC_IODBC_IODBCTEST AND "${ODBC_PROVIDER}" STREQUAL "iODBC")
        add_test(NAME "iodbctest-w" COMMAND sh -c "echo select ${SQL_QUERY} | ${ODBC_IODBC_IODBCTEST} 'DSN=${TEST_DSN_W}' | grep ${SQL_ANSWER}")
        add_test(NAME "test.sh-iodbctest-w" COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${ODBC_IODBC_IODBCTEST} \"DSN=${TEST_DSN_W}\")
    endif()
    if(ODBC_IODBC_IODBCTESTW AND "${ODBC_PROVIDER}" STREQUAL "iODBC")
        add_test(NAME "iodbctestw-w" COMMAND sh -c "echo select ${SQL_QUERY} | ${ODBC_IODBC_IODBCTESTW} 'DSN=${TEST_DSN_W}' | grep ${SQL_ANSWER}")
        add_test(NAME "test.sh-iodbctestw-w" COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${ODBC_IODBC_IODBCTESTW} \"DSN=${TEST_DSN_W}\")
    endif()
endif()

if(SQLCMD)
    # MS SQL server need change server in file:
    add_test(NAME "sqlcmd" COMMAND ${SQLCMD} -i ${CMAKE_CURRENT_SOURCE_DIR}/mssql.linked.server.sql)
endif()
