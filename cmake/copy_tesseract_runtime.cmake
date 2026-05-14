file(TO_CMAKE_PATH "${src}" normalized_src)
file(TO_CMAKE_PATH "${dst}" normalized_dst)

if(NOT normalized_src STREQUAL normalized_dst)
    if(NOT EXISTS "${src}")
        message(FATAL_ERROR "Tesseract runtime not found at ${src}")
    endif()

    file(REMOVE_RECURSE "${dst}")
    file(MAKE_DIRECTORY "${dst}")
    file(COPY "${src}/" DESTINATION "${dst}")
endif()
