set(APP_NAME uber_resolver_obfuscate_pass)

add_executable(${APP_NAME} ${Z85_SRC} main.cpp)

install(TARGETS ${APP_NAME}
        DESTINATION bin)
