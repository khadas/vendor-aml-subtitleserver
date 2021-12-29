#pragma once

#include "core.h"

#include <string>

namespace Cairo {
    class Surface {
    public:
        Surface(int width, int height);

        ~Surface();

        operator cairo_surface_t *() {
            return surface_;
        }

        int getWidth();
        int getHeight();
        int getFormat();

        void saveToFile(std::string const &fileName) const;

        uint8_t *data();

    private:
        cairo_surface_t *surface_;
    };
}
