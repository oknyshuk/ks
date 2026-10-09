add_library(bzip2 STATIC
    blocksort.c
    bzip2.c
    bzlib.c
    compress.c
    crctable.c
    decompress.c
    huffman.c
    randtable.c
)
target_link_libraries(bzip2 PRIVATE ks_common )
