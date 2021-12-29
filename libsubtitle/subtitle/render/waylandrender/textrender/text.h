#pragma once

#include "shape.h"
#include "font.h"
#include "bounding_box.h"

namespace Cairo
{
    class Text : public Shape
    {
    public:
        Text(DrawContext* ctx, double x, double y, std::string const& text, Font const& font);
        void draw(Pen const& line, Pen const& fill = {}) override;
        void drawCenter(BoundingBox &baseRect, Pen const& line, Pen const& fill = {});
        BoundingBox calculateBounds(Pen const& line) const;
        double getBaselineDisplacement(Pen const& line) const;
        void setFont(Font const& font);
        Font& getFont();

    private:
        std::string text_;
        Font font_;
    };
}
