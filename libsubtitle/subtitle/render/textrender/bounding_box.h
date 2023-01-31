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

namespace Cairo {
    struct BoundingBox {
        double x;
        double y;
        double x2;
        double y2;

        bool isEmpty() {
            return getHeight() <= 0 || getWidth() <= 0;
        }

        double getWidth() const {
            return x2 - x;
        }

        double getHeight() const {
            return y2 - y;
        }

        double xCenter() const {
            return x + (x2 - x) / 2.;
        }

        double yCenter() const {
            return y + (y2 - y) / 2.;
        }

        void move(double x, double y) {
            this->x = x;
            this->y = y;
            x2 = x2 + x;
            y2 = y2 + y;
        }

        void additiveMove(double x, double y) {
            move(this->x + x, this->y + y);
        }

        void setWidth(double width) {
            x2 = x + width;
        }

        void setHeight(double height) {
            y2 = y + height;
        }
    };
}
