add_executable(sdh_cafeteria_congestion main.cpp)

target_link_libraries(sdh_cafeteria_congestion
  PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
)

# 빌드 후 실행 파일과 함께 serverkeys 디렉토리의 파일들을 복사 # <- letsencrypt 공인인증키로 변경함.
#add_custom_command(TARGET sdh_cafeteria_congestion POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:sdh_cafeteria_congestion>/serverkeys
#    COMMAND ${CMAKE_COMMAND} -E copy
#        ${CMAKE_SOURCE_DIR}/serverkeys/server.crt
#        $<TARGET_FILE_DIR:sdh_cafeteria_congestion>/serverkeys/server.crt
#    COMMAND ${CMAKE_COMMAND} -E copy
#        ${CMAKE_SOURCE_DIR}/serverkeys/server.key
#        $<TARGET_FILE_DIR:sdh_cafeteria_congestion>/serverkeys/server.key
#    COMMENT "Copying server SSL certificates to the build directory"
#)
