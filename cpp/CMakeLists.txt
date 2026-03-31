cmake_minimum_required(VERSION 3.20)
project(ImageTransition VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Landmark model auto-download ──────────────────────────────────────────
set(_MODEL_DIR "${CMAKE_SOURCE_DIR}/models")
set(_MODEL_DAT "${_MODEL_DIR}/shape_predictor_68_face_landmarks.dat")
set(_MODEL_URL "http://dlib.net/files/shape_predictor_68_face_landmarks.dat.bz2")

if(NOT EXISTS "${_MODEL_DAT}")
    set(_MODEL_BZ2 "${_MODEL_DIR}/shape_predictor_68_face_landmarks.dat.bz2")
    message(STATUS "Downloading shape_predictor_68_face_landmarks.dat (~100 MB) ...")
    file(DOWNLOAD "${_MODEL_URL}" "${_MODEL_BZ2}"
         SHOW_PROGRESS
         TLS_VERIFY ON
         STATUS _dl_status)
    list(GET _dl_status 0 _dl_code)
    if(NOT _dl_code EQUAL 0)
        file(REMOVE "${_MODEL_BZ2}")
        message(FATAL_ERROR "Failed to download landmark model: ${_dl_status}")
    endif()
    message(STATUS "Decompressing landmark model ...")
    find_program(_BZIP2 bzip2 REQUIRED)
    execute_process(
        COMMAND "${_BZIP2}" -d "${_MODEL_BZ2}"
        RESULT_VARIABLE _bz2_result
    )
    if(NOT _bz2_result EQUAL 0)
        message(FATAL_ERROR "bzip2 decompression failed (exit code ${_bz2_result})")
    endif()
    if(NOT EXISTS "${_MODEL_DAT}")
        message(FATAL_ERROR "Decompression failed — ${_MODEL_DAT} not found after extraction.")
    endif()
    message(STATUS "Landmark model ready: ${_MODEL_DAT}")
endif()

# Copy the model next to the built executables so they can find it without --model
set(_BUILD_MODEL_DIR "${CMAKE_BINARY_DIR}/models")
file(COPY "${_MODEL_DAT}" DESTINATION "${_BUILD_MODEL_DIR}")

# ── Dependencies ─────────────────────────────────────────────────────────
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs videoio)
find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

# dlib -- fetched from source if not already installed
include(FetchContent)
FetchContent_Declare(
    dlib
    GIT_REPOSITORY https://github.com/davisking/dlib.git
    GIT_TAG        v19.24.6
    GIT_SHALLOW    TRUE
)
set(DLIB_NO_GUI_SUPPORT ON CACHE BOOL "Disable dlib GUI (we use wxWidgets)" FORCE)
FetchContent_MakeAvailable(dlib)

# ── Core library (shared by GUI and CLI) ─────────────────────────────────
add_library(image_transition_core STATIC
    src/core/face_align.cpp
    src/core/landmarks.cpp
    src/core/delaunay.cpp
    src/core/morph.cpp
    src/core/pipeline.cpp
)
target_include_directories(image_transition_core PUBLIC src)
target_link_libraries(image_transition_core PUBLIC
    ${OpenCV_LIBS}
    dlib::dlib
)

# ── CLI executable ───────────────────────────────────────────────────────
add_executable(image_transition_cli src/cli/main.cpp)
target_link_libraries(image_transition_cli PRIVATE image_transition_core)

# ── GUI executable ───────────────────────────────────────────────────────
add_executable(image_transition_gui WIN32 MACOSX_BUNDLE
    src/gui/App.cpp
    src/gui/MainFrame.cpp
)
target_link_libraries(image_transition_gui PRIVATE
    image_transition_core
    ${wxWidgets_LIBRARIES}
)

# macOS .app bundle
if(APPLE)
    # Embed the landmark model inside the bundle as a resource
    target_sources(image_transition_gui PRIVATE "${_MODEL_DAT}")
    set_source_files_properties("${_MODEL_DAT}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources/models")

    set_target_properties(image_transition_gui PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/packaging/macos/Info.plist"
        MACOSX_BUNDLE_BUNDLE_NAME "Image Transition"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.imagetransition.app"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )

    target_link_libraries(image_transition_gui PRIVATE "-framework CoreFoundation")
endif()

# Windows: embed a manifest for high-DPI and visual-style support
if(WIN32)
    set_target_properties(image_transition_gui PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# ── Install rules ────────────────────────────────────────────────────────
install(TARGETS image_transition_cli image_transition_gui
        RUNTIME DESTINATION bin
        BUNDLE  DESTINATION .)
install(FILES "${_MODEL_DAT}" DESTINATION models)
