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

#pragma once

#include "draw_context.h"

namespace Cairo {
    struct Pen {
        double width;
        RGBA color;

        constexpr Pen(Pen const &) = default;

        constexpr Pen(Pen &&) = default;

        Pen &operator=(Pen const &) = default;

        Pen &operator=(Pen &&) = default;

        Pen &operator=(RGBA const &color);

        Pen() = default;

        constexpr Pen(double width, RGBA color)
                : width{width}, color{std::move(color)} {
        }

        constexpr Pen(RGBA color)
                : width{1}, color{std::move(color)} {
        }

        constexpr Pen(RGB color)
                : width{1}, color{color} {
        }
    };

#define PRESERVE true

    void applyPen(DrawContext *ctx, Pen const &pen);

    void stroke(DrawContext *ctx, Pen const &pen, bool preserve = false);

    void fill(DrawContext *ctx, Pen const &pen, bool preserve = false);
}
