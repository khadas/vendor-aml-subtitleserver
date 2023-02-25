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

#include <cstdint>
#include <limits>

namespace Cairo {
    using color_space = uint8_t;
    constexpr auto colorMax = std::numeric_limits<color_space>::max();
    constexpr auto colorMin = std::numeric_limits<color_space>::min();

    struct RGB {
        color_space r;
        color_space g;
        color_space b;
    };

    struct RGBA : public RGB {
        color_space a;

        RGBA &operator=(RGBA const &) = default;

        RGBA &operator=(RGBA &&) = default;

        RGBA() = default;

        constexpr RGBA(RGB const &color)
                : RGB{color}, a{255} {

        }

        constexpr RGBA(RGBA const &color) = default;

        constexpr RGBA(RGBA &&color) = default;

        constexpr RGBA(color_space r, color_space g, color_space b, color_space a)
                : RGB{r, g, b}, a{a} {

        }

        RGBA &operator=(RGB const &color) noexcept {
            r = color.r;
            g = color.g;
            b = color.b;
            a = colorMax;
            return *this;
        }
    };

    namespace Colors {
        constexpr auto Red = RGB{colorMax, colorMin, colorMin};
        constexpr auto Green = RGB{colorMin, colorMax, colorMin};
        constexpr auto Blue = RGB{colorMin, colorMin, colorMax};
        constexpr auto Yellow = RGB{colorMax, colorMax, colorMin};
        constexpr auto Cyan = RGB{colorMin, colorMax, colorMax};
        constexpr auto Magenta = RGB{colorMax, colorMin, colorMax};
        constexpr auto White = RGB{colorMax, colorMax, colorMax};
        constexpr auto Black = RGB{colorMin, colorMin, colorMin};
        constexpr auto Gray = RGB{colorMax / 2, colorMax / 2, colorMax / 2};
        constexpr auto GrayTransparent = RGBA{0, 0, 0, 200};
        constexpr auto Transparent = RGBA{0, 0, 0, 0};
    }
}
