#pragma once

#include "core.h"
#include "surface.h"
#include "color.h"

namespace Cairo {
    class DrawContext {
    public:
        DrawContext(Surface *surface);

        ~DrawContext();

        operator cairo_t *() {
            return ctx_;
        }

        void fill(color_space r, color_space g, color_space b, color_space a = 0xFF);

        void fill(RGBA color);

        void save();

        void restore();


    private:
        Surface *surface_;
        cairo_t *ctx_;
    };
}
