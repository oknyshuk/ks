# SPDX-License-Identifier: MIT
# Copyright (c) 2026 oknyshuk

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
    sparsematrix.cpp
    qsort_s.cpp)

target_link_libraries(tier1 PRIVATE ks_common)
target_compile_definitions(tier1 PRIVATE TIER1_STATIC_LIB=1)

add_library(ks_tier1 STATIC)
target_sources(ks_tier1 PUBLIC
    FILE_SET CXX_MODULES
    FILES
          ks.tier1-interfaces.cppm
          ks.tier1-iappsystem.cppm
          ks.tier1-icvar.cppm
          ks.tier1-convar.cppm
          ks.tier1-cvar.cppm
          ks.tier1-commandbuffer.cppm
          ks.tier1-convar_serverbounded.cppm
          ks.tier1-tier1.cppm
          ks.tier1-reflect_wirevar.cppm
          ks.tier1-rangecheckedvar.cppm
          ks.tier1-lerp_functions.cppm
          ks.tier1-interpolatedvar.cppm
          ks.tier1-appinstance.cppm
          ks.tier1-bidirectional_set.cppm
          ks.tier1-bit_ops.cppm
          ks.tier1-bitbuf.cppm
          ks.tier1-strtools.cppm
          ks.tier1-buffer_strider.cppm
          ks.tier1-byte_reader.cppm
          ks.tier1-byte_writer.cppm
          ks.tier1-characterset.cppm
          ks.tier1-checksum_crc.cppm
          ks.tier1-checksum_md5.cppm
          ks.tier1-checksum_sha1.cppm
          ks.tier1-circularbuffer.cppm
          ks.tier1-null_mutex.cppm
          ks.tier1-datamanager.cppm
          ks.tier1-delegates.cppm
          ks.tier1-diff.cppm
          ks.tier1-small_vector.cppm
          ks.tier1-string_ops.cppm
          ks.tier1-envelope.cppm
          ks.tier1-exprevaluator.cppm
          ks.tier1-utlstring.cppm
          ks.tier1-fileio.cppm
          ks.tier1-map_ops.cppm
          ks.tier1-filename_table.cppm
          ks.tier1-fixed_pool.cppm
          ks.tier1-fmtstr.cppm
          ks.tier1-refcount.cppm
          ks.tier1-functors.cppm
          ks.tier1-generichash.cppm
          ks.tier1-handle_table.cppm
          ks.tier1-heapsort.cppm
          ks.tier1-hierarchicalbitvec.cppm
          ks.tier1-iconvar.cppm
          ks.tier1-interface.cppm
          ks.tier1-registry.cppm
          ks.tier1-intern_pool.cppm
          ks.tier1-intrusive_list.cppm
          ks.tier1-intrusive_ptr.cppm
          ks.tier1-vector_ops.cppm
          ks.tier1-utlvector.cppm
          ks.tier1-keyvalues.cppm
          ks.tier1-keyvaluesjson.cppm
          ks.tier1-kvpacker.cppm
          ks.tier1-lzmadecoder.cppm
          ks.tier1-lzss.cppm
          ks.tier1-memstack.cppm
          ks.tier1-murmurhash3.cppm
          ks.tier1-netadr.cppm
          ks.tier1-ns_address.cppm
          ks.tier1-playerid.cppm
          ks.tier1-qsort_s.cppm
          ks.tier1-string_escapes.cppm
          ks.tier1-text_reader.cppm
          ks.tier1-text_writer.cppm
          ks.tier1-serialization.cppm
          ks.tier1-slot_pool.cppm
          ks.tier1-sparsematrix.cppm
          ks.tier1-string_token.cppm
          ks.tier1-strtools_inlines.cppm
          ks.tier1-tier1_logging.cppm
          ks.tier1-timeutils.cppm
          ks.tier1-tokenset.cppm
          ks.tier1-tracked_ref.cppm
          ks.tier1-uniqueid.cppm
          ks.tier1-utlsoacontainer.cppm)
target_link_libraries(ks_tier1 PUBLIC ks_core ks_math ks_simd PRIVATE ks_common_base)
set_property(TARGET ks_tier1 PROPERTY CXX_SCAN_FOR_MODULES ON)
