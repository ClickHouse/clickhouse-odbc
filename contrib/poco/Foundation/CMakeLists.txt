file (GLOB SRCS_PCRE src/pcre_*.c)

add_library (_poco_foundation_pcre ${SRCS_PCRE})
add_library (Poco::Foundation::PCRE ALIAS _poco_foundation_pcre)

# TODO: remove these warning exclusions
if (NOT MSVC)
    target_compile_options (_poco_foundation_pcre
        PRIVATE
            -Wno-cast-align
            -Wno-cast-qual
            -Wno-comma
            -Wno-conditional-uninitialized
            -Wno-extra-semi-stmt
            -Wno-implicit-fallthrough
            -Wno-reserved-identifier
            -Wno-sign-compare
            -Wno-unknown-pragmas
            -Wno-unreachable-code-break
            -Wno-unused-macros
    )
endif()

set(SRCS 
    src/ASCIIEncoding.cpp
    src/AbstractObserver.cpp
    src/ActiveDispatcher.cpp
    src/ArchiveStrategy.cpp
    src/Ascii.cpp
    src/AsyncChannel.cpp
    src/AtomicCounter.cpp
    src/Base64Decoder.cpp
    src/Base64Encoder.cpp
    src/BinaryReader.cpp
    src/BinaryWriter.cpp
    src/Bugcheck.cpp
    src/ByteOrder.cpp
    src/Channel.cpp
    src/Checksum.cpp
    src/Clock.cpp
    src/Condition.cpp
    src/Configurable.cpp
    src/ConsoleChannel.cpp
    src/CountingStream.cpp
    src/DateTime.cpp
    src/DateTimeFormat.cpp
    src/DateTimeFormatter.cpp
    src/DateTimeParser.cpp
    src/Debugger.cpp
    src/DeflatingStream.cpp
    src/DigestEngine.cpp
    src/DigestStream.cpp
    src/DirectoryIterator.cpp
    src/DirectoryIteratorStrategy.cpp
    src/DirectoryWatcher.cpp
    src/Environment.cpp
    src/Error.cpp
    src/ErrorHandler.cpp
    src/Event.cpp
    src/EventArgs.cpp
    src/EventChannel.cpp
    src/EventLogChannel.cpp
    src/Exception.cpp
    src/FIFOBufferStream.cpp
    src/FPEnvironment.cpp
    src/File.cpp
    src/FileChannel.cpp
    src/FileStream.cpp
    src/FileStreamFactory.cpp
    src/Format.cpp
    src/Formatter.cpp
    src/FormattingChannel.cpp
    src/Glob.cpp
    src/Hash.cpp
    src/HashStatistic.cpp
    src/HexBinaryDecoder.cpp
    src/HexBinaryEncoder.cpp
    src/InflatingStream.cpp
    src/JSONString.cpp
    src/Latin1Encoding.cpp
    src/Latin2Encoding.cpp
    src/Latin9Encoding.cpp
    src/LineEndingConverter.cpp
    src/LocalDateTime.cpp
    src/LogFile.cpp
    src/LogStream.cpp
    src/Logger.cpp
    src/LoggingFactory.cpp
    src/LoggingRegistry.cpp
    src/MD4Engine.cpp
    src/MD5Engine.cpp
    src/Manifest.cpp
    src/MemoryPool.cpp
    src/MemoryStream.cpp
    src/Message.cpp
    src/Mutex.cpp
    src/NamedEvent.cpp
    src/NamedMutex.cpp
    src/NestedDiagnosticContext.cpp
    src/Notification.cpp
    src/NotificationCenter.cpp
    src/NotificationQueue.cpp
    src/NullChannel.cpp
    src/NullStream.cpp
    src/NumberFormatter.cpp
    src/NumberParser.cpp
    src/NumericString.cpp
    src/Path.cpp
    src/PatternFormatter.cpp
    src/Pipe.cpp
    src/PipeImpl.cpp
    src/PipeStream.cpp
    src/PriorityNotificationQueue.cpp
    src/Process.cpp
    src/PurgeStrategy.cpp
    src/RWLock.cpp
    src/Random.cpp
    src/RandomStream.cpp
    src/RefCountedObject.cpp
    src/RegularExpression.cpp
    src/RotateStrategy.cpp
    src/Runnable.cpp
    src/SHA1Engine.cpp
    src/Semaphore.cpp
    src/SharedLibrary.cpp
    src/SharedMemory.cpp
    src/SignalHandler.cpp
    src/SimpleFileChannel.cpp
    src/SortedDirectoryIterator.cpp
    src/SplitterChannel.cpp
    src/SyslogChannel.cpp
    src/Stopwatch.cpp
    src/StreamChannel.cpp
    src/StreamConverter.cpp
    src/StreamCopier.cpp
    src/StreamTokenizer.cpp
    src/String.cpp
    src/StringTokenizer.cpp
    src/SynchronizedObject.cpp
    src/Task.cpp
    src/TaskManager.cpp
    src/TaskNotification.cpp
    src/TeeStream.cpp
    src/TemporaryFile.cpp
    src/TextBufferIterator.cpp
    src/TextConverter.cpp
    src/TextEncoding.cpp
    src/TextIterator.cpp
    src/Thread.cpp
    src/ThreadLocal.cpp
    src/ThreadPool.cpp
    src/ThreadTarget.cpp

    src/TimedNotificationQueue.cpp
    src/Timer.cpp
    src/Timespan.cpp
    src/Timestamp.cpp
    src/Timezone.cpp

    src/Token.cpp
    src/URI.cpp
    src/URIStreamFactory.cpp
    src/URIStreamOpener.cpp
    src/UTF16Encoding.cpp
    src/UTF32Encoding.cpp
    src/UTF8Encoding.cpp
    src/UTF8String.cpp
    src/UUID.cpp
    src/UUIDGenerator.cpp
    src/Unicode.cpp
    src/UnicodeConverter.cpp
    src/Var.cpp
    src/VarHolder.cpp
    src/VarIterator.cpp
    src/Void.cpp
    src/Windows1250Encoding.cpp
    src/Windows1251Encoding.cpp
    src/Windows1252Encoding.cpp
    src/WindowsConsoleChannel.cpp
)

if(WIN32)
    set(SYSLIBS ${SYSLIBS} iphlpapi)
endif(WIN32)

add_library(_poco_foundation ${SRCS})

target_include_directories (_poco_foundation SYSTEM PUBLIC "include")

target_compile_definitions (_poco_foundation
    PRIVATE
        POCO_UNBUNDLED
        POCO_UNBUNDLED_ZLIB
    PUBLIC
        POCO_ENABLE_CPP11
)

if (OS_DARWIN OR OS_LINUX)
    target_compile_definitions (_poco_foundation PUBLIC POCO_OS_FAMILY_UNIX)
    if (OS_DARWIN)
        target_compile_definitions (_poco_foundation PUBLIC POCO_NO_STAT64)
    endif()
elseif(OS_WINDOWS)
    target_compile_definitions (_poco_foundation PUBLIC POCO_OS_FAMILY_WINDOWS Foundation_EXPORTS)
endif()

if (NOT MSVC)
    target_compile_options (_poco_foundation
        PRIVATE
            -Wno-array-bounds
            -Wno-atomic-implicit-seq-cst
            -Wno-cast-align
            -Wno-cast-qual
            -Wno-class-varargs
            -Wno-covered-switch-default
            -Wno-deprecated
            -Wno-extra-semi-stmt
            -Wno-implicit-fallthrough
            -Wno-implicit-int-float-conversion
            -Wno-misleading-indentation
            -Wno-missing-noreturn
            -Wno-old-style-cast
            -Wno-redundant-parens
            -Wno-reserved-identifier
            -Wno-reserved-macro-identifier
            -Wno-shadow
            -Wno-shorten-64-to-32
            -Wno-sign-compare
            -Wno-suggest-destructor-override
            -Wno-suggest-override
            -Wno-tautological-unsigned-zero-compare
            -Wno-thread-safety-analysis
            -Wno-thread-safety-negative
            -Wno-undef
            -Wno-unreachable-code-return
            -Wno-unused-exception-parameter
            -Wno-unused-macros
            -Wno-unused-parameter
            -Wno-zero-as-null-pointer-constant
    )
endif()

add_definitions( -DPCRE_STATIC)

target_link_libraries (_poco_foundation
    PRIVATE
        Poco::Foundation::PCRE
        ch_contrib::zlib
        ch_contrib::lz4
        ch_contrib::double_conversion
        ${SYSLIBS})

add_library(Poco::Foundation ALIAS _poco_foundation)

target_include_directories(_poco_foundation
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
	)

