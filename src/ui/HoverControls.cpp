#include "HoverControls.hpp"
#include "FadeContainer.hpp"
#include "LabeledButton.hpp"
#include "MainAtlas.hpp"
#include "NClock.hpp"
#include "SpeedSelect.hpp"
#include "util/MenuInterface.h"

HoverControls_t::HoverControls_t(UIContext *ctx, const Style_t& initial_style, NClock *clock) : 
    FadeContainer_t(ctx, initial_style) {
    mi = getMenuInterface();

    //hover_controls_con = new FadeContainer_t(&ui_ctx, HUD, 512);
    set_position(10, 100);
    size(65, 500);

    Style_t SB;
    SB.background_color = 0x00000000;
    SB.border_width = 0;
    SB.border_color = 0x000000FF;
    SB.padding = 0;

    {
        LabeledButton *b1 = new LabeledButton(ctx, ResetButton, "", 0);
        b1->size(60, 60);
        b1->on_click([this](const SDL_Event& event) -> bool {
            getMenuInterface()->machineReset();
            return true;
        });
        add(b1);

        LabeledButton *b3 = new LabeledButton(ctx, GreenDisplayButton, "Capture", 0);
        b3->size(60, 60);
        b3->on_click([this](const SDL_Event& event) -> bool {
            getMenuInterface()->machineCaptureMouse();
            return true;
        });
        add(b3);

        LabeledButton *b2 = new LabeledButton(ctx, GreenDisplayButton, "Debug", 0);
        b2->size(60, 60);
        b2->on_click([this](const SDL_Event& event) -> bool {
            getMenuInterface()->openDebugWindow();
            return true;
        });
        add(b2);

        hov_speed_con = new SpeedSelect_t(ctx, SB, clock);
        hov_speed_con->set_visible(false);

        add(hov_speed_con);
        //ncontainers.push_back(hover_controls_con);

        hov_speed = new LabeledButton(ctx, MHz1_0Button, "Speed", 0);
        hov_speed->size(60, 60);
        hov_speed->on_click([this](const SDL_Event& event) -> bool {
            // open the speed submenu container
            if (!hov_speed_con->is_visible()) {            
                // get position of b4
                float x,y;
                hov_speed_con->set_visible(true);
                hov_speed->get_tile_position(x, y);
                hov_speed_con->set_position(x+60, y);
                hov_speed_con->layout();
            } else hov_speed_con->set_visible(false);
        
            return true;
        });
        add(hov_speed);
        layout();
    }
}

void HoverControls_t::update() {
    if (mi->isMouseCaptured()) {
        set_visible(false);
    } else {
        set_visible(true);
    }

    if (frameCount == 0) { // if we're not visible, hide the submenu
        hov_speed_con->set_visible(false);
    }

    hov_speed->set_assetID(speed_asset.at(getMenuInterface()->getCurrentSpeed()));
}

HoverControls_t::~HoverControls_t() {

}

