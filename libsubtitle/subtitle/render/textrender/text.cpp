/*
 * Copyright (C) 2014-2019 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 *
 * This software is provided to you pursuant to Software License Agreement
 * (SLA) with Amlogic Inc ("Amlogic"). This software may be used
 * only in accordance with the terms of this agreement.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification is strictly prohibited without prior written permission from
 * Amlogic.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "text.h"

namespace Cairo {
//#####################################################################################################################
    Text::Text(DrawContext *ctx, double x, double y, std::string const &text, Font const &font)
            : Shape{ctx, x, y}, text_{text}, font_{font} {

    }

//---------------------------------------------------------------------------------------------------------------------
    void Text::draw(Pen const &line, Pen const &fill) {
        cairo_select_font_face(*ctx_, font_.family.c_str(), font_.slant, font_.weight);
        cairo_set_font_size(*ctx_, font_.size);
        applyPen(ctx_, line);
        cairo_text_extents_t te;
        cairo_text_extents(*ctx_, text_.c_str(), &te);
        cairo_move_to(*ctx_, x_ - te.x_bearing, y_ - te.y_bearing);
        cairo_show_text(*ctx_, text_.c_str());
    }

    void Text::drawCenter(BoundingBox &baseRect, const Pen &line, const Pen &fill) {
        cairo_select_font_face(*ctx_, font_.family.c_str(), font_.slant, font_.weight);
        cairo_set_font_size(*ctx_, font_.size);
        applyPen(ctx_, line);

        cairo_text_extents_t te;
        cairo_text_extents(*ctx_, text_.c_str(), &te);
        double x = (baseRect.getWidth() - te.width) / 2;
        double y = (baseRect.getHeight() - te.height) / 2;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        cairo_move_to(*ctx_, x - te.x_bearing, y - te.y_bearing);

        cairo_show_text(*ctx_, text_.c_str());
    }

//---------------------------------------------------------------------------------------------------------------------
    BoundingBox Text::calculateBounds(Pen const &line) const {
        BoundingBox box = {};

        ctx_->save();
        cairo_select_font_face(*ctx_, font_.family.c_str(), font_.slant, font_.weight);
        cairo_set_font_size(*ctx_, font_.size);
        applyPen(ctx_, line);
        cairo_text_extents_t te;
        cairo_text_extents(*ctx_, text_.c_str(), &te);
        ctx_->restore();

        box.x = x_ - te.x_bearing;
        box.y = y_ - te.y_bearing;
        box.x2 = x_ - te.x_bearing + te.width;
        box.y2 = y_ - te.y_bearing + te.height;
        box.additiveMove(0, -box.getHeight());

        return box;
    }

//---------------------------------------------------------------------------------------------------------------------
    double Text::getBaselineDisplacement(Pen const &line) const {
        ctx_->save();
        cairo_select_font_face(*ctx_, font_.family.c_str(), font_.slant, font_.weight);
        cairo_set_font_size(*ctx_, font_.size);
        applyPen(ctx_, line);
        cairo_text_extents_t te;
        cairo_font_extents_t fe;
        cairo_text_extents(*ctx_, text_.c_str(), &te);
        cairo_font_extents(*ctx_, &fe);
        ctx_->restore();

        return (y_ - te.y_bearing + te.height) - (y_ - te.y_bearing) + te.y_bearing;
    }

//---------------------------------------------------------------------------------------------------------------------
    void Text::setFont(Font const &font) {
        font_ = font;
    }

//---------------------------------------------------------------------------------------------------------------------
    Font &Text::getFont() {
        return font_;
    }
//#####################################################################################################################
}
