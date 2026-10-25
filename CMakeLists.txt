cmake_minimum_required(VERSION 3.24)
project(moba_trainer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# exe 1 つで動くように、MSVC のランタイムを静的リンクする
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(USE_MSVC_RUNTIME_LIBRARY_DLL OFF CACHE BOOL "" FORCE)

# ---------------------------------------------------------------------------
# 外部ライブラリ（バージョンはここで固定する）
# ---------------------------------------------------------------------------
include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 6.0
  GIT_SHALLOW TRUE)

FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.92.7
  GIT_SHALLOW TRUE)

# rlImGui: raylib 6.0 + ImGui 1.92.07 用ブランチのコミットで固定
FetchContent_Declare(rlimgui
  GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
  GIT_TAG 3bc5731c4216bb8caa67fbea24aa85ce80d57ccb)

FetchContent_Declare(implot
  GIT_REPOSITORY https://github.com/epezent/implot.git
  GIT_TAG v1.0
  GIT_SHALLOW TRUE)

FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.5.3
  GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(raylib imgui rlimgui implot nlohmann_json doctest)

# Dear ImGui / rlImGui / ImPlot（上流に CMakeLists が無いので、ここで静的ライブラリにする）
add_library(thirdparty_ui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${rlimgui_SOURCE_DIR}/rlImGui.cpp
  ${implot_SOURCE_DIR}/implot.cpp
  ${implot_SOURCE_DIR}/implot_items.cpp)
target_include_directories(thirdparty_ui PUBLIC
  ${imgui_SOURCE_DIR}
  ${rlimgui_SOURCE_DIR}
  ${implot_SOURCE_DIR})
target_compile_definitions(thirdparty_ui PUBLIC NO_FONT_AWESOME)
target_link_libraries(thirdparty_ui PUBLIC raylib)

# ---------------------------------------------------------------------------
# ゲーム本体（main 以外はライブラリにして、テストからも使えるようにする）
# ---------------------------------------------------------------------------
add_library(moba_lib STATIC
  src/core/Log.cpp
  src/core/LogRetention.cpp
  src/core/Paths.cpp
  src/core/Platform.cpp
  src/core/JsonFile.cpp
  src/data/Settings.cpp
  src/data/EffectSpec.cpp
  src/data/CharacterDef.cpp
  src/data/AiConfig.cpp
  src/data/SandboxConfig.cpp
  src/data/StageDef.cpp
  src/data/GameData.cpp
  src/game/StatusEffects.cpp
  src/game/Entities.cpp
  src/game/CastStats.cpp
  src/game/Terrain.cpp
  src/game/World.cpp
  src/game/SkillCasting.cpp
  src/game/PlayerController.cpp
  src/game/DummyController.cpp
  src/game/Sandbox.cpp
  src/game/Match.cpp
  src/game/ResultStore.cpp
  src/game/ResultStats.cpp
  src/game/ai/Aim.cpp
  src/game/ai/PlayerObserver.cpp
  src/game/ai/EnemyAI.cpp
  src/game/ai/SkillShotTarget.cpp
  src/game/skills/SkillRegistry.cpp
  src/game/skills/SkillHelpers.cpp
  src/game/skills/CommonSkills.cpp
  src/game/skills/EzrealSkills.cpp
  src/game/skills/EmmaSkills.cpp
  src/game/skills/BrandSkills.cpp
  src/input/LiveInput.cpp
  src/input/ScriptInput.cpp
  src/render/Renderer.cpp
  src/render/Hud.cpp
  src/render/CameraControl.cpp
  src/ui/Fonts.cpp
  src/ui/UiScale.cpp
  src/ui/MainMenu.cpp
  src/ui/SandboxPanel.cpp
  src/ui/DebugPanel.cpp
  src/ui/ResultScreen.cpp
  src/ui/PauseDialog.cpp
  src/ui/RecordsPanel.cpp
  src/ui/StatsDashboard.cpp
  src/app/CommandLine.cpp
  src/app/App.cpp)
target_include_directories(moba_lib PUBLIC src)
target_link_libraries(moba_lib PUBLIC raylib thirdparty_ui nlohmann_json::nlohmann_json)
target_compile_definitions(moba_lib PUBLIC MOBA_VERSION="${PROJECT_VERSION}")
if(MSVC)
  target_compile_options(moba_lib PUBLIC /utf-8 /W4 /permissive-)
  target_link_libraries(moba_lib PUBLIC imm32)
endif()

add_executable(moba_trainer src/main.cpp)
target_link_libraries(moba_trainer PRIVATE moba_lib)
if(MSVC)
  # Release ではコンソール窓を出さない
  target_link_options(moba_trainer PRIVATE
    $<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>
    $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>)
endif()

# ---------------------------------------------------------------------------
# 自動テスト
# ---------------------------------------------------------------------------
add_executable(moba_tests
  tests/test_main.cpp
  tests/test_geometry.cpp
  tests/test_log_retention.cpp
  tests/test_character_data.cpp
  tests/test_world.cpp
  tests/test_dodge.cpp
  tests/test_shot.cpp
  tests/test_results.cpp
  tests/test_stats.cpp
  tests/test_ui_scale.cpp
  tests/test_camera.cpp
  tests/test_sandbox.cpp
  tests/test_stage.cpp)
target_link_libraries(moba_tests PRIVATE moba_lib doctest::doctest)
target_compile_definitions(moba_tests PRIVATE MOBA_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
