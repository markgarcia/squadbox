#include "console_ui.hpp"

#include <imgui.h>

namespace squadbox {

void console_ui::update() {
    if (!visible()) return;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Test scenes")) {
            if (ImGui::MenuItem("Cube")) show_test_scene_cube();
            if (ImGui::MenuItem("Rubber duck")) show_test_scene_duck();

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void console_ui::show_test_scene_cube() {

}

void console_ui::show_test_scene_duck() {

}

}