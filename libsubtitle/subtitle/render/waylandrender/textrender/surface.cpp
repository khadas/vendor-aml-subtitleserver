#include "surface.h"

namespace Cairo {
    Surface::Surface(int width, int height)
            : surface_{cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height)} {

    }

    Surface::~Surface() {
        cairo_surface_destroy(surface_);
    }

    void Surface::saveToFile(std::string const &fileName) const {
        cairo_surface_write_to_png(surface_, fileName.c_str());
    }

    uint8_t *Surface::data() {
        return cairo_image_surface_get_data(surface_);
    }

    int Surface::getWidth() {
        return cairo_image_surface_get_width(surface_);
    }

    int Surface::getHeight() {
        return cairo_image_surface_get_height(surface_);
    }

    int Surface::getFormat() {
        return cairo_image_surface_get_format(surface_);
    }

}
