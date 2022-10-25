#include "cJSON.h"
#include "CCJsonParser.h"

#include <utils/Log.h>

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
