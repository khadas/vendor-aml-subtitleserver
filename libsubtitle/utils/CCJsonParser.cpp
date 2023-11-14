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

#include "cJSON.h"
#include "CCJsonParser.h"

#include "SubtitleLog.h"

bool CCJsonParser::populateData(std::string &inputText, std::vector<std::string> &output) {
    if (inputText.empty()) {
        return false;
    }

    const char *input = inputText.c_str();
    cJSON *verificationJson = cJSON_Parse(input);
    if (!verificationJson) {
        return false;
    }

    cJSON *windows = cJSON_GetObjectItem(verificationJson, "windows");
    if (!windows) {
        return false;
    }

    int size = cJSON_GetArraySize(windows);
    for (int i = 0; i < size; i++) {
        if (size > 1 && i > 0) {
            output.emplace_back("[NEXT]");
        }

        cJSON *arrItem = cJSON_GetArrayItem(windows, i);
        if (arrItem) {
            cJSON *rows = cJSON_GetObjectItem(arrItem, "rows");
            if (rows) {
                int rowCount = cJSON_GetArraySize(rows);
                for (int row = 0; row < rowCount; ++row) {
                    cJSON *rowItem = cJSON_GetArrayItem(rows, row);
                    if (rowItem) {
                        cJSON *contentArray = cJSON_GetObjectItem(rowItem, "content");
                        if (contentArray) {
                            int contentSize = cJSON_GetArraySize(contentArray);
                            for (int content = 0; content < contentSize; ++content) {
                                cJSON *contentItem = cJSON_GetArrayItem(contentArray, content);
                                if (contentItem) {
                                    cJSON *data = cJSON_GetObjectItem(contentItem, "data");
                                    if (data) {
                                        output.emplace_back(data->valuestring);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return !output.empty();
}
