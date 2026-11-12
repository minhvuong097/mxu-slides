set(MXU_TESTS
    tst_document
    tst_presentation_file
    tst_library
    tst_renderer
)

foreach(test_name IN LISTS MXU_TESTS)
    qt_add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE mxu_core Qt6::Test Qt6::Widgets)
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endforeach()
