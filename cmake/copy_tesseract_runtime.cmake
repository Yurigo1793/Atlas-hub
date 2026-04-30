file(TO_CMAKE_PATH "${src}" normalized_src)
file(TO_CMAKE_PATH "${dst}" normalized_dst)

if(NOT normalized_src STREQUAL normalized_dst)
    file(COPY "${src}/" DESTINATION "${dst}")
endif()
