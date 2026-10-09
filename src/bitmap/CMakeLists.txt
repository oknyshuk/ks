add_library(bitmap STATIC
    bitmap.cpp
    colorconversion.cpp
    floatbitmap.cpp
    floatbitmap2.cpp
    floatbitmap3.cpp
    floatbitmap_bilateralfilter.cpp
    floatcubemap.cpp
    imageformat.cpp
    psd.cpp
    psheet.cpp
    resample.cpp
    tgaloader.cpp
    texturepacker.cpp
    tgawriter.cpp)

target_link_libraries(bitmap PRIVATE ks_common)
