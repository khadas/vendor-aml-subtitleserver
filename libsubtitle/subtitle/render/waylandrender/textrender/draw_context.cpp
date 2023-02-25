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

#include "draw_context.h"

namespace Cairo {
    DrawContext::DrawContext(Surface *surface)
            : surface_{surface}, ctx_{cairo_create(*surface)} {
    }

    DrawContext::~DrawContext() {
        cairo_destroy(ctx_);
    }

    void DrawContext::fill(color_space r, color_space g, color_space b, color_space a) {
        save();
        cairo_set_source_rgba(
                ctx_,
                static_cast <double> (r) / colorMax,
                static_cast <double> (g) / colorMax,
                static_cast <double> (b) / colorMax,
                static_cast <double> (a) / colorMax
        );
        cairo_set_operator(ctx_, CAIRO_OPERATOR_SOURCE);
        cairo_paint(ctx_);
        restore();
    }

    void DrawContext::fill(RGBA color) {
        fill(color.r, color.g, color.b, color.a);
    }

    void DrawContext::save() {
        cairo_save(ctx_);
    }

    void DrawContext::restore() {
        cairo_restore(ctx_);
    }
}
