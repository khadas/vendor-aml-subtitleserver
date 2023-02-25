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

#include "round_rectangle.h"

#include <cmath>

#ifndef M_PI
#   define M_PI 3.14159265358979323846264338327950288
#endif

namespace Cairo {
    RoundRectangle::RoundRectangle(DrawContext *ctx, double x, double y, double width,
                                   double height, double radius)
            : Rectangle{ctx, x, y, width, height}, radius_{radius} {

    }

    void RoundRectangle::draw(Pen const &line, Pen const &fill) {
        double degrees = M_PI / 180.;

        cairo_new_sub_path(*ctx_);
        cairo_arc(*ctx_, x_ + width_ - radius_, y_ + radius_, radius_, -90 * degrees, 0 * degrees);
        cairo_arc(*ctx_, x_ + width_ - radius_, y_ + height_ - radius_, radius_, 0 * degrees,
                  90 * degrees);
        cairo_arc(*ctx_, x_ + radius_, y_ + height_ - radius_, radius_, 90 * degrees,
                  180 * degrees);
        cairo_arc(*ctx_, x_ + radius_, y_ + radius_, radius_, 180 * degrees, 270 * degrees);
        cairo_close_path(*ctx_);

        Cairo::fill(ctx_, fill, PRESERVE);
        Cairo::stroke(ctx_, line);
    }
}
