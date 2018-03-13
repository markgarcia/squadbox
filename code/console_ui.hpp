#ifndef SQUADBOX_CONSOLE_UI_HPP
#define SQUADBOX_CONSOLE_UI_HPP

#pragma once

namespace squadbox {

class console_ui {
public:
    void show() { m_is_visible = true; }
    void hide() { m_is_visible = false; }
    void toggle_visibility() { m_is_visible = !m_is_visible; }
    bool visible() const { return m_is_visible; }

    void update();

private:
    void show_test_scene_cube();
    void show_test_scene_duck();

    bool m_is_visible = false;
};

}

#endif