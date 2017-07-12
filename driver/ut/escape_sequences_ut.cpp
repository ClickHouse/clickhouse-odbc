#include <escaping/escape_sequences.h>
#include <gtest/gtest.h>

TEST(EscapeSequencesCase, ParseConvert) {
    ASSERT_EQ(
        replaceEscapeSequences("SELECT {fn CONVERT(1, SQL_BIGINT)}"),
        "SELECT toInt64(1)"
    );
}
