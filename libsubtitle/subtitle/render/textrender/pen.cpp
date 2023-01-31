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

#include "pen.h"

namespace Cairo {
//#####################################################################################################################
    Pen &Pen::operator=(RGBA const &color) {
        this->color = color;
        return *this;
    }

//#####################################################################################################################
    void applyPen(DrawContext *ctx, Pen const &pen) {
        cairo_set_line_width(*ctx, pen.width);
        cairo_set_source_rgba(
                *ctx,
                static_cast <double> (pen.color.r) / colorMax,
                static_cast <double> (pen.color.g) / colorMax,
                static_cast <double> (pen.color.b) / colorMax,
                static_cast <double> (pen.color.a) / colorMax
        );
    }

//---------------------------------------------------------------------------------------------------------------------
    void stroke(DrawContext *ctx, Pen const &pen, bool preserve) {
        applyPen(ctx, pen);
        if (preserve)
            cairo_stroke_preserve(*ctx);
        else
            cairo_stroke(*ctx);
    }

//---------------------------------------------------------------------------------------------------------------------
    void fill(DrawContext *ctx, Pen const &pen, bool preserve) {
        applyPen(ctx, pen);
        if (preserve)
            cairo_fill_preserve(*ctx);
        else
            cairo_fill(*ctx);
    }
//#####################################################################################################################
}
