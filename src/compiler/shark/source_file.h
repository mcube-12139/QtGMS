#ifndef SOURCEFILE_H
#define SOURCEFILE_H

#include <string>

namespace Shark {
    using std::string;

    struct SourceFile {
        SourceFile(string name, string content)
            : name(name), content(content) { }
        string name;
        string content;
    };
}

#endif // SOURCEFILE_H
