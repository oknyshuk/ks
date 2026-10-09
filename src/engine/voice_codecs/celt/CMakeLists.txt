add_library(vaudio_celt OBJECT
    ../frame_encoder/voice_codec_frame.cpp
    voiceencoder_celt.cpp
)
target_link_libraries(vaudio_celt PRIVATE ks_common tier0 memoverride tier1 interfaces vstdlib celt)
target_include_directories(vaudio_celt PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../frame_encoder" "${CMAKE_CURRENT_SOURCE_DIR}/../.." "${CMAKE_CURRENT_SOURCE_DIR}/../../audio/public")
