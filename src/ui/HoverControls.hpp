#pragma once

#include "FadeContainer.hpp"
#include "LabeledButton.hpp"
#include "NClock.hpp"
#include <map>
#include "util/MenuInterface.h"
#include "SpeedSelect.hpp"

class HoverControls_t : public FadeContainer_t {
protected:
    SpeedSelect_t *hov_speed_con = nullptr;
    LabeledButton *hov_speed = nullptr;
    const std::map<int, int> speed_asset =  {
        {SPEED_FREE_RUN, MHzInfinityButton},
        {SPEED_1_0, MHz1_0Button},
        {SPEED_2_8, MHz2_8Button},
        {SPEED_7_1, MHz7_159Button},
        {SPEED_14_3, MHz14_318Button},
    };
    MenuInterface *mi = nullptr;

public:
    HoverControls_t(UIContext *ctx, const Style_t& initial_style = Style_t(), NClock *clock = nullptr);
    ~HoverControls_t();
    virtual void update() override;
};