add_library(auroramon SHARED auroramon.cc)
target_link_libraries(auroramon maxscale-common)
add_dependencies(auroramon pcre2)
set_target_properties(auroramon PROPERTIES VERSION "1.0.0" LINK_FLAGS -Wl,-z,defs)
install_module(auroramon core)
