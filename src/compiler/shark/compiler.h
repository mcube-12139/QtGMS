#ifndef COMPILER_H
#define COMPILER_H

#include <vector>
#include <memory>

#include "source_file.h"
#include "intermediate/program.h"

namespace Shark {
    using std::vector;
    using std::unique_ptr;

    using Intermediate::Program;

    unique_ptr<Program> compile(const vector<SourceFile> &files);
}

#endif // COMPILER_H
