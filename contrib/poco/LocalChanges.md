Please note that the Poco project source code has been copied directly into the
project, instead of being included as a Git submodule. This was done due to
several problems we encountered with Poco, which could only be resolved by
modifying its source code.

This document aims to record these changes and their motivations. It is
especially important to track them, since reverting these modifications would
introduce regressions and might even break some functionality.

The base version of the library was taken from the Git tag poco-1.14.2-release.

Only the Foundation project is included. The HTTP client of the driver used to
be Poco as well (the Crypto, Net, NetSSL_OpenSSL, NetSSL_Win and Util projects),
but it required extensive changes, mostly for handling ClickHouse exceptions,
and has been replaced with libcurl (contrib/curl, unmodified upstream sources,
see contrib/curl-cmake and driver/http_client.cpp). Those projects, and the
description of the changes made to them, can be found in the Git history of this
directory.

From Foundation, tests, fuzzing, samples, and similar components were removed.
Only the include and src directories were preserved. Additionally, files with
platform prefixes for unsupported platforms (VX, DEC, SUN, QNX, Android) were
removed.

CMake files were generally preserved as they were, except that test-related code
and unsupported platforms were removed. Some compiler definitions were also
eliminated — for example, UNICODE and _UNICODE were removed from Foundation.

It is best to simply compare the contents of the poco-1.14.2-release tag from
the official Poco repository with the current contrib/poco directory to see
these details.

This document provides the rationale for the changes made to the Poco library
source code.

--------------------------------------------------------------------------------
commit fe0983a6361b7656838e5035edb5f4cfa5a9248e<br/>
Author: Andrew Slabko<br/>
Date:   Tue Sep 30 19:40:45 2025 +0200<br/>

This was not Poco’s fault but our own. For details, see
https://github.com/ClickHouse/clickhouse-odbc/pull/512/commits/900d0e0010a8a7769e103cba57f790d1d997242e
This should be considered a temporary fix. The permanent solution would be to
avoid linking `libclickhouse-odbc-impl` to the test target.
