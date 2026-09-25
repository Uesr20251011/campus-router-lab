cmake_minimum_required(VERSION 3.21)
project(CampusRouterLab VERSION 1.3.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Quick QuickControls2 Test)
qt_standard_project_setup()

add_library(router_core STATIC
    src/GraphAlgorithms.cpp
    src/GraphBackend.cpp
)
target_include_directories(router_core PUBLIC src)
target_link_libraries(router_core PUBLIC Qt6::Core)

set(app_sources src/main.cpp)
if(WIN32)
    list(APPEND app_sources assets/app.rc)
    set_source_files_properties(assets/app.rc PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/app-icon.ico")
endif()
qt_add_executable(campus_router ${app_sources})
set_target_properties(campus_router PROPERTIES WIN32_EXECUTABLE TRUE)
qt_add_resources(campus_router "app_icons" PREFIX "/" FILES assets/app-icon.png)
qt_add_qml_module(campus_router
    URI CampusRouter
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/GraphCanvas.qml
        qml/SoftButton.qml
        qml/SoftSpinBox.qml
        qml/SoftComboBox.qml
        qml/SoftTextField.qml
        qml/ToolIconButton.qml
        qml/WindowControl.qml
)
target_link_libraries(campus_router PRIVATE router_core Qt6::Quick Qt6::QuickControls2)

enable_testing()
qt_add_executable(router_tests tests/tst_graph.cpp)
target_link_libraries(router_tests PRIVATE router_core Qt6::Test)
add_test(NAME router_tests COMMAND router_tests)

qt_add_executable(canvas_tests tests/tst_canvas.cpp)
target_link_libraries(canvas_tests PRIVATE Qt6::Quick Qt6::Test)
target_compile_definitions(canvas_tests PRIVATE QML_CANVAS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/qml/GraphCanvas.qml")
target_compile_definitions(canvas_tests PRIVATE QML_SPINBOX_PATH="${CMAKE_CURRENT_SOURCE_DIR}/qml/SoftSpinBox.qml")
add_test(NAME canvas_tests COMMAND canvas_tests)
set_tests_properties(canvas_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")

qt_add_executable(layout_tests tests/tst_layout.cpp)
target_link_libraries(layout_tests PRIVATE router_core Qt6::Quick Qt6::QuickControls2 Qt6::Test)
target_compile_definitions(layout_tests PRIVATE QML_MAIN_PATH="${CMAKE_CURRENT_SOURCE_DIR}/qml/Main.qml")
add_test(NAME layout_tests COMMAND layout_tests)
set_tests_properties(layout_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
