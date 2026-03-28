cmake_minimum_required(VERSION 3.16)

project(sentriface_enroll_app LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets Core Gui)
find_package(Qt5 5.15 QUIET COMPONENTS LinguistTools Multimedia TextToSpeech)

add_executable(sentriface_enroll_app
  include/qt_enroll_app/capture_plan.hpp
  include/qt_enroll_app/enrollment_artifact.hpp
  include/qt_enroll_app/enrollment_session.hpp
  include/qt_enroll_app/frame_source.hpp
  include/qt_enroll_app/guidance_engine.hpp
  include/qt_enroll_app/main_window.hpp
  include/qt_enroll_app/observation_source.hpp
  include/qt_enroll_app/preview_widget.hpp
  src/capture_plan.cpp
  src/enrollment_artifact.cpp
  src/enrollment_session.cpp
  src/frame_source.cpp
  src/main.cpp
  src/main_window.cpp
  src/observation_source.cpp
  src/preview_widget.cpp
  src/guidance_engine.cpp
  resources/qt_enroll_app.qrc
)

target_include_directories(sentriface_enroll_app
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(sentriface_enroll_app
  PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    sentriface_camera
    sentriface_detector
    sentriface_logging
)

if(TARGET Qt5::TextToSpeech)
  target_link_libraries(sentriface_enroll_app PRIVATE Qt5::TextToSpeech)
  target_compile_definitions(sentriface_enroll_app PRIVATE SENTRIFACE_HOST_HAS_QT_TTS=1)
endif()

if(TARGET Qt5::Multimedia)
  target_link_libraries(sentriface_enroll_app PRIVATE Qt5::Multimedia)
  target_compile_definitions(sentriface_enroll_app PRIVATE SENTRIFACE_HOST_HAS_QT_MULTIMEDIA=1)
endif()

target_compile_features(sentriface_enroll_app PRIVATE cxx_std_17)

target_compile_options(sentriface_enroll_app
  PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)

if(TARGET Qt5::LinguistTools)
  set(SENTRIFACE_QT_ENROLL_TS_FILES
    translations/sentriface_qt_enroll_zh_TW.ts
    translations/sentriface_qt_enroll_en.ts
  )
  qt5_create_translation(SENTRIFACE_QT_ENROLL_QM_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main_window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/preview_widget.cpp
    ${SENTRIFACE_QT_ENROLL_TS_FILES}
  )
  add_custom_target(sentriface_qt_enroll_translations
    DEPENDS ${SENTRIFACE_QT_ENROLL_QM_FILES}
  )
endif()
