Please note that the Poco project source code has been copied directly into the
project, instead of being included as a Git submodule. This was done due to
several problems we encountered with Poco, which could only be resolved by
modifying its source code.

This document aims to record these changes and their motivations. It is
especially important to track them, since reverting these modifications would
introduce regressions and might even break some functionality.

Given the number of issues we faced with Poco, we hope there will be enough time
in the future to replace it with a more suitable alternative, such as libcurl.

The base version of the library was taken from the Git tag poco-1.14.2-release.

Only a few projects were included from the original distribution:

 - Crypto
 - Foundation
 - Net
 - NetSSL_OpenSSL
 - NetSSL_Win
 - Util

From each of these projects, tests, fuzzing, samples, and similar components
were removed. Only the include and src directories were preserved. Additionally,
files with platform prefixes for unsupported platforms (VX, DEC, SUN, QNX,
Android) were removed.

CMake files were generally preserved as they were, except that test-related code
and unsupported platforms were removed. Some compiler definitions were also
eliminated — for example, UNICODE and _UNICODE were removed from Foundation.

It is best to simply compare the contents of the poco-1.14.2-release tag from
the official Poco repository with the current contrib/poco directory to see
these details.

This document provides the rationale for the changes made to the Poco library
source code.

--------------------------------------------------------------------------------
commit b8e496a412be63350d53bdebf03f0d26586eb591<br/>
Author: Andrew Slabko<br/>
Date:   Tue Sep 30 14:34:02 2025 +0200<br/>

While testing the library with the cloud, the SSL handshake would fail because
the implementation of Poco::Net::SecureSocketImpl “forgot” the server hostname
as soon as it connected to the server, before performing the handshake. This
broke SNI, which is essential for working with the cloud.

See also https://github.com/pocoproject/poco/issues/5042

--------------------------------------------------------------------------------
commit fe0983a6361b7656838e5035edb5f4cfa5a9248e<br/>
Author: Andrew Slabko<br/>
Date:   Tue Sep 30 19:40:45 2025 +0200<br/>

This was not Poco’s fault but our own. For details, see
https://github.com/ClickHouse/clickhouse-odbc/pull/512/commits/900d0e0010a8a7769e103cba57f790d1d997242e
This should be considered a temporary fix. The permanent solution would be to
avoid linking `libclickhouse-odbc-impl` to the test target.

--------------------------------------------------------------------------------
commit 7fb613793a1ddcda1c7cc2504be8f5f1e6093e7d<br/>
Author: Andrew Slabko<br/>
Date:   Fri Oct 3 11:26:39 2025 +0200<br/>

While testing the library with SSL configured on a local stand-alone version of
ClickHouse (as described in
[Configuring SSL-TLS](https://clickhouse.com/docs/guides/sre/configuring-ssl))
, I found that during the handshake the server requested a client certificate,
and the client provided a default one.

Although the server was not configured for client certificate verification, it
still attempted to verify it, resulting in an “unknown client certificate”
error.

`curl` and `Invoke-WebRequest` worked correctly, as did the Linux version of the
driver. They all ignored the client certificate request. Only the Windows
version of the driver attempted to locate and provide a client certificate. To
disable this, I changed the Windows Secure Channel configuration flags.

--------------------------------------------------------------------------------
commit 1370b0a9f40801801d433aa4bab03f06e5b26710<br/>
Author: Andrew Slabko<br/>
Date:   Fri Oct 17 18:05:58 2025 +0200<br/>

commit 1ba8988bd54f30d31114e7959c4ac0350511d044<br/>
Author: Andrew Slabko<br/>
Date:   Fri Oct 17 20:59:04 2025 +0200<br/>

commit a0b391b829f670e06f9b67ce92bab45c75bfd983<br/>
Author: Andrew Slabko<br/>
Date:   Fri Oct 18 19:58:04 2025 +0200<br/>

Poco::Net::HTTPChunkedStreamBuf was largely rewritten due to incorrect handling
of chunked-encoded streams, limitations in Poco and std::istream, and specific
requirements for handling mid-stream exceptions in ClickHouse.

One of the issues is described in
https://github.com/pocoproject/poco/issues/5032. HTTPChunkedStreamBuf
incorrectly assumed that the stream had ended properly as soon as it encountered
either a terminating zero-sized chunk or a closed connection. The second
assumption was incorrect — there are many reasons a connection might close
prematurely, and the absence of a terminating zero-sized chunk indicates
incomplete data.

We encountered several such cases:

- ClickHouse threw an exception while sending data.
- A reverse proxy (e.g., nginx, ingress) closed the connection due to a timeout.
- ClickHouse or the proxy restarted, closing the connection unexpectedly.

This caused situations where the parser expected more data but the stream closed
as if complete, resulting in either incomplete data or “Incomplete input stream,
expected at least XXX more bytes” errors. Examples include:

 - https://github.com/ClickHouse/clickhouse-odbc/issues/419
 - https://github.com/ClickHouse/clickhouse-odbc/issues/440
 - https://github.com/ClickHouse/clickhouse-odbc/issues/522

The fix is not extremely complicated and covered in
https://github.com/ClickHouse/clickhouse-odbc/commit/1370b0a9f40801801d433aa4bab03f06e5b26710

Another issue arises when ClickHouse throws an exception after already sending
some data (a mid-stream exception). In such cases, ClickHouse sends an exception
message starting with the `__exception__\r\n` marker and then closes the socket
without sending the terminating zero-sized chunk. These exceptions must also be
handled by HTTPChunkedStreamBuf; otherwise, std::istream closes the stream
immediately, discarding the remaining buffered data and losing the exception
message (or part of it). To handle this, HTTPChunkedStreamBuf now always fetches
extra data to verify whether the stream closed correctly and searches for the
exception marker if not.

The situation becomes even more complex with compression enabled, since
`HTTPChunkedStreamBuf` must also handle compressed data — including compressed
exception messages.

Due to these issues, HTTPChunkedStreamBuf was effectively rewritten but remains
in the Poco::Net target. The class is a friend of several others in the library,
and creating a new one would not be practical. The original class was already
broken, so it was modified in place.

--------------------------------------------------------------------------------
commit ac982b98c52f7a65cb699d2017dadb16490df82d<br/>
Author: Andrew Slabko<br/>
Date:   Sat Nov 1 19:00:21 2025 +0100<br/>

Implemented Zstd decompression for input data received from the ClickHouse
server. Most of the work still resides in the already rewritten
HTTPChunkedStreamBuf. However, since the parameter indicating whether
compression should be enabled or not must be read from the response headers and
passed to the HTTPChunkedStreamBuf instance, the HTTPClientSession class was
also slightly modified.
