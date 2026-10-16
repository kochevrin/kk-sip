cmake_minimum_required(VERSION 3.22)
project(kksip LANGUAGES C CXX)

# KKSIP_PJSIP_ROOT/<abi> is the static PJSIP from android/scripts/build-pjsip.sh.
set(PJ "${KKSIP_PJSIP_ROOT}/${ANDROID_ABI}")
if(NOT EXISTS "${PJ}/include/pjsua2.hpp")
    message(FATAL_ERROR "PJSIP for ${ANDROID_ABI} not found in ${PJ}: run android/scripts/build-pjsip.sh ${ANDROID_ABI}")
endif()

add_library(kksip SHARED kksip_jni.cpp)
target_include_directories(kksip PRIVATE "${PJ}/include")
target_compile_definitions(kksip PRIVATE PJ_AUTOCONF=1 PJ_IS_BIG_ENDIAN=0 PJ_IS_LITTLE_ENDIAN=1)

# Link order matters for static libraries: users before what they use.
set(PJ_LIBS pjsua2 pjsua pjsip-ua pjsip-simple pjsip pjmedia-codec pjmedia-videodev
    pjmedia-audiodev pjmedia pjnath pjlib-util srtp resample gsmcodec speex ilbccodec
    g7221codec pj)
foreach(name ${PJ_LIBS})
    file(GLOB lib "${PJ}/lib/lib${name}-*-linux-android.a")
    if(NOT lib)
        message(FATAL_ERROR "lib${name} missing in ${PJ}/lib")
    endif()
    target_link_libraries(kksip PRIVATE ${lib})
endforeach()

target_link_libraries(kksip PRIVATE OpenSLES log android m)
# Only the JNI entry points need to be visible; lets the linker drop unused PJSIP code.
target_link_options(kksip PRIVATE -Wl,--gc-sections -Wl,--exclude-libs,ALL)
