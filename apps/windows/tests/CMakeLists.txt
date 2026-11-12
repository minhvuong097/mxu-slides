set(MXU_TESTS
    tst_document
    tst_presentation_file
    tst_library
    tst_renderer
)

if(WIN32)
    # The offscreen platform has no system font database on Windows. Use the
    # regular Windows platform so text rendering matches the real app.
    set(MXU_TEST_QPA_PLATFORM windows)
else()
    set(MXU_TEST_QPA_PLATFORM offscreen)
endif()

foreach(test_name IN LISTS MXU_TESTS)
    qt_add_executable(${test_name} ${test_name}.cpp)
    if(WIN32)
        # Keep the test executables attached to the console so CTest and CI can
        # capture QTest assertion output.
        set_target_properties(${test_name} PROPERTIES WIN32_EXECUTABLE FALSE)
    endif()
    target_link_libraries(${test_name} PRIVATE mxu_core Qt6::Test Qt6::Widgets)
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=${MXU_TEST_QPA_PLATFORM}"
    )
endforeach()
