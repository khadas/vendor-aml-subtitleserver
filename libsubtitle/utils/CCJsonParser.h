
#ifndef CCJSONPARSER_H
#define CCJSONPARSER_H

#include <string>
#include <vector>

typedef struct _CCData {
    std::string type;

}CCData;


class CCJsonParser {
public:
    static bool populateData(std::string& inputText, std::vector<std::string> &output);
};


#endif //CCJSONPARSER_H
