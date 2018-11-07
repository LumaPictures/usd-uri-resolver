set(APP_NAME uri_resolver_obfuscate_pass)

add_executable(${APP_NAME} ${Z85_SRC} main.cpp)
target_include_directories(${APP_NAME} SYSTEM PRIVATE "${EXTERNAL_INCLUDE_DIR}")

install(
    TARGETS ${APP_NAME}
    DESTINATION bin)
