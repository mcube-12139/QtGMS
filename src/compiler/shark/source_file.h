#ifndef SOURCEFILE_H
#define SOURCEFILE_H

#include <string>

namespace Shark {
    struct SourceFile {
        SourceFile(std::u16string name, std::u16string content)
            : name(name), content(content) { }
        std::u16string name;
        std::u16string content;
    };
}

#endif // SOURCEFILE_H
