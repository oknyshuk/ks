add_library(tier0 OBJECT
    assert_dialog.cpp
    commandline.cpp
    fasttimer.cpp
    cpu.cpp
    cpumonitoring.cpp
    cputopology.cpp
    dbg.cpp
    logging.cpp
    mem.cpp
    mem_helpers.cpp
    memvirt.cpp
    memstd.cpp
    dynfunction.cpp
    minidump.cpp
    tier0_strtools.cpp
    miniprofiler.cpp
    perfstats.cpp
    platform_independent.cpp
    platwindow.cpp
    progressbar.cpp
    stackstats.cpp
    stacktools.cpp
    systeminformation.cpp
    threadtools.cpp
    tslist.cpp
    vatoms.cpp
    vprof.cpp
    vtuneinterface.cpp
    win32consoleio.cpp
    cpu_posix.cpp
    platform_posix.cpp
    pme_posix.cpp)

target_link_libraries(tier0 PRIVATE ks_common)
target_compile_definitions(tier0 PRIVATE TIER0_DLL_EXPORT=1 CROSS_PLATFORM_VERSION=1)
