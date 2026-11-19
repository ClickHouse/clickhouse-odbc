#pragma once

#include <string>
#include <string_view>

namespace runner {

// How the shim writes a result set.
struct OutputFormat {
    std::string name = "TabSeparated";
    bool names = false;   // a header line with the column names
    bool types = false;   // a header line with the column types
    bool raw = false;     // top-level strings are not escaped
    bool discard = false; // the `Null` format
    bool supported = true;
};

// Tells if a statement returns a result set, by its leading keyword. The shim removes the FORMAT
// clause of these statements only: in an INSERT it names the input format.
bool returnsRows(std::string_view sql);

// Removes a top-level `FORMAT <name>` clause from a result-set statement, and returns the
// format. The driver reads only its own formats (it fails on any other output format), so the
// shim always lets the driver read the result, and writes it itself in the TabSeparated family.
// A statement with another output format fails. (The runner sends a `FORMAT Null` statement
// unchanged when it expects no error: see isNullFormatResult.)
OutputFormat splitOutputFormat(std::string_view sql, std::string & rest);

// Tells if an SQLExecDirect error is the driver's answer to a result in the `Null` format. The
// driver reads the format from the response header, and the server sends that header after it ran
// the statement (a `Null` result has no body). So the statement succeeded, and has no rows.
bool isNullFormatResult(std::string_view sqlstate, std::string_view message);

} // namespace runner
