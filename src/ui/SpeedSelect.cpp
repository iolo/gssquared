#include "SpeedSelect.hpp"
#include "SelectButton.hpp"
#include "MainAtlas.hpp"
#include "NClock.hpp"

SpeedSelect_t::SpeedSelect_t(UIContext *ctx, const Style_t& initial_style, NClock *clock) : Container_t(ctx, initial_style), clock(clock) {
    Style_t CB = {
        .background_color = 0x00000000,
        .border_color = 0x000000FF,
        .hover_color = 0x00C0C0FF,
        .padding = 2,
        .border_width = 1,        
    };

    // don't set position yet, we'll do that when we open the submenu.
    size(360, 65);
    add(new SelectButton_t(ctx, MHz1_0Button, CB, CLOCK_1_024MHZ));
    add(new SelectButton_t(ctx, MHz2_8Button, CB, CLOCK_2_8MHZ));
    add(new SelectButton_t(ctx, MHz7_159Button, CB, CLOCK_7_159MHZ));
    add(new SelectButton_t(ctx, MHz14_318Button, CB, CLOCK_14_3MHZ));
    add(new SelectButton_t(ctx, MHzInfinityButton, CB, CLOCK_FREE_RUN));
    set_visible(false);

    // iterate tiles and set onclick for each
    for (size_t i = 0; i < tiles.size(); i++) {
        Tile_t *tile = tiles[i];
        tile->on_click([this,tile](const SDL_Event& event) -> bool {
            this->selected_value(tile->value());
            this->clock->set_clock_mode((clock_mode_t)tile->value());
            this->set_visible(false);
            return true;
        });
    }

    style.hover_color = 0xEE0000FF;
}

// whenever we go from invisible to visible, set the selected value to the current clock mode
void SpeedSelect_t::set_visible(bool visible) {
    Container_t::set_visible(visible);
    //printf("SpeedSelect_t::set_visible: %d\n", visible);
    /* if (!visible) {
        assert(true);
    } */
    if (visible) {
        this->selected_value(this->clock->get_clock_mode());
    }
}

void SpeedSelect_t::render() {
    if (!visible) return;
    Container_t::render();
}

bool SpeedSelect_t::handle_mouse_event(const SDL_Event& event) {
    if (!visible) return false;
    bool consumed = Container_t::handle_mouse_event(event);
    //printf("SpeedSelect_t::handle_mouse_event: %d\n", consumed);
    if (consumed && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        return true;
    }
    return false;
}

SpeedSelect_t::~SpeedSelect_t() {
    // deallocate all tiles
    for (size_t i = 0; i < tiles.size(); i++) {
        delete tiles[i];
    }
    tiles.clear();
}