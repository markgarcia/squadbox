#ifndef SQUADBOX_TEST_SCENES_CUBE_HPP
#define SQUADBOX_TEST_SCENES_CUBE_HPP

#pragma once

#include "../gfx/render_techniques/flat_shading.hpp"
#include "../gfx/render_manager.hpp"

namespace squadbox::test_scenes {

class cube {
public:
    cube(const gfx::render_manager& render_manager);

    void render();
    
private:
    gfx::render_techniques::flat_shading::render_data m_render_data;
};

}

#endif