cmake_minimum_required(VERSION 3.5.1)

project(cjlogger)

find_library(RT_LIBRARY           NAMES rt                    HINTS ${CMAKE_SHARED_LIB_PATH})
find_library(IPC_LIBRARY          NAMES ipc                   HINTS ${CMAKE_SHARED_LIB_PATH})
find_library(FMT_LIBRARY          NAMES fmt                   HINTS ${CMAKE_SHARED_LIB_PATH})
find_library(PTHREAD_LIBRARY      NAMES pthread               HINTS ${CMAKE_SHARED_LIB_PATH})
find_library(Z_LIBRARY            NAMES z                     HINTS ${CMAKE_SHARED_LIB_PATH})
find_library(VIZIOS_SENTRY_LOG    NAMES vizios-sentry-log     HINTS ${CMAKE_SHARED_LIB_PATH})
find_library(CURL_LIBRARY         NAMES curl                  HINTS ${CMAKE_SHARED_LIB_PATH})
find_library(SSL_LIBRARY          NAMES ssl                   HINTS ${CMAKE_SHARED_LIB_PATH})
find_library(CRYPTO_LIBRARY       NAMES crypto                HINTS ${CMAKE_SHARED_LIB_PATH})
find_library(INOTIFY_LIBRARY      NAMES inotify-cpp           HINTS ${CMAKE_SHARED_LIB_PATH})
find_library(BOOST_LIBRARY        NAMES boost                 HINTS ${CMAKE_SHARED_LIB_PATH})
find_library(AWS_SDK_S3_LIBRARY   NAMES aws-cpp-sdk-s3        HINTS ${CMAKE_SHARED_LIB_PATH})
find_library(AWS_SDK_CORE_LIBRARY NAMES aws-cpp-sdk-core      HINTS ${CMAKE_SHARED_LIB_PATH})
find_library(AWS_SDK_FIREHOSE_LIBRARY NAMES aws-cpp-sdk-firehose      HINTS ${CMAKE_SHARED_LIB_PATH})
find_library(AWS_SDK_KINESIS_LIBRARY NAMES aws-cpp-sdk-kinesis      HINTS ${CMAKE_SHARED_LIB_PATH})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS_DEBUG "-gdwarf-4 -O0")

add_executable(${CMAKE_PROJECT_NAME}
  ${PROJECT_SOURCE_DIR}/analytics/cjlogger.cpp
  ${PROJECT_SOURCE_DIR}/analytics/main.cpp
)

 target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC
  ${IPC_LIBRARY}
  ${FMT_LIBRARY}
  ${PTHREAD_LIBRARY}
  ${Z_LIBRARY}
  # ${VIZIOS_SENTRY_LOG}
  ${CURL_LIBRARY}
  ${SSL_LIBRARY}
  ${CRYPTO_LIBRARY}
  # ${INOTIFY_LIBRARY}
  ${AWS_SDK_S3_LIBRARY}
  ${AWS_SDK_CORE_LIBRARY}
  ${AWS_SDK_FIREHOSE_LIBRARY}
  ${AWS_SDK_KINESIS_LIBRARY}
  # ${RT_LIBRARY}
 )

target_include_directories(${CMAKE_PROJECT_NAME}
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include/vizio
)

target_include_directories(${CMAKE_PROJECT_NAME}
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src/logger/include
    ${PROJECT_SOURCE_DIR}/src/utils/include
)

target_compile_definitions(${CMAKE_PROJECT_NAME}
  PRIVATE
    USE_STDOUT_FOR_LOGGING
)

# install(TARGETS ${ANALYTICS_PROJECT_NAME}
#     EXPORT ${ANALYTICS_PROJECT_NAME}
#   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
# )