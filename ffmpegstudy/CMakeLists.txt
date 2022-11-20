# CMakeList.txt: ffmpegstudy 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

set(FFMPEG_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/ffmpeg-shared-5.1/include)
set(FFMPEG_LIB_DIR ${PROJECT_SOURCE_DIR}/ffmpeg-shared-5.1/lib)
set(FFMPEG_DLL_DIR ${PROJECT_SOURCE_DIR}/ffmpeg-shared-5.1/bin)

#头文件
include_directories(${FFMPEG_INCLUDE_DIR})
#编译需要链接的lib描述库文件
link_directories(${FFMPEG_LIB_DIR})

# 将源代码添加到此项目的可执行文件。
add_executable (ffmpegstudy  "ffmpegstudy.h" "rtsptortmp.cpp")

#添加链接库
target_link_libraries(${PROJECT_NAME} avutil avformat avcodec avdevice avfilter postproc swscale swresample)

#dll拷贝
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
file(COPY ${FFMPEG_DLL_DIR}/avcodec-59.dll DESTINATION ${EXECUTABLE_OUTPUT_PATH})
file(COPY ${FFMPEG_DLL_DIR}/avdevice-59.dll DESTINATION ${EXECUTABLE_OUTPUT_PATH})
file(COPY ${FFMPEG_DLL_DIR}/avfilter-8.dll DESTINATION ${EXECUTABLE_OUTPUT_PATH})
file(COPY ${FFMPEG_DLL_DIR}/avformat-59.dll DESTINATION ${EXECUTABLE_OUTPUT_PATH})
file(COPY ${FFMPEG_DLL_DIR}/avutil-57.dll DESTINATION ${EXECUTABLE_OUTPUT_PATH})
file(COPY ${FFMPEG_DLL_DIR}/postproc-56.dll DESTINATION ${EXECUTABLE_OUTPUT_PATH})
file(COPY ${FFMPEG_DLL_DIR}/swresample-4.dll DESTINATION ${EXECUTABLE_OUTPUT_PATH})
file(COPY ${FFMPEG_DLL_DIR}/swscale-6.dll DESTINATION ${EXECUTABLE_OUTPUT_PATH})