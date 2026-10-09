add_library(tier1 STATIC
    appinstance.cpp
    bitbuf.cpp
    newbitbuf.cpp
    characterset.cpp
    checksum_crc.cpp
    checksum_md5.cpp
    checksum_sha1.cpp
    circularbuffer.cpp
    commandbuffer.cpp
    convar.cpp
    datamanager.cpp
    diff.cpp
    exprevaluator.cpp
    generichash.cpp
    interface.cpp
    keyvalues.cpp
    keyvaluesjson.cpp
    kvpacker.cpp
    lzmaDecoder.cpp
    lzss.cpp
    fixed_pool.cpp
    memstack.cpp
    murmurhash3.cpp
    NetAdr.cpp
    rangecheckedvar.cpp
    reflect_test.cpp
    strtools.cpp
    strtools_unicode.cpp
    tier1.cpp
    tier1_logging.cpp
    timeutils.cpp
    uniqueid.cpp
    text_reader.cpp
    serialization.cpp
    utlsoacontainer.cpp
    miniprofiler_hash.cpp
    sparsematrix.cpp
    qsort_s.cpp)

target_link_libraries(tier1 PRIVATE ks_common)
target_compile_definitions(tier1 PRIVATE TIER1_STATIC_LIB=1)
