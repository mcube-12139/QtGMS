#include "sharkbackend.h"

using Shark::Intermediate::Program;

enum class ValueType: quint8 {
    INTEGER = 2,
    LONG = 3,
    VARIABLE = 5
};

class SegmentGenerator {
public:
    SegmentGenerator(VmCode &code, DataWriter &file)
        : code(code), file(file) { }

    void testGenerate() {
        addLocal(QStringLiteral("arguments"));
        pushInt(114514);
        call(QStringLiteral("show_message"), 1);
        popUnused();
        exitSegment();
    }
private:
    VmCode &code;
    DataWriter &file;

    void convert(ValueType from, ValueType to) {
        if (from != to) {
            code.bytecode.u32(0x07000000 | (static_cast<qint32>(to) << 20) | (static_cast<qint32>(from) << 16));
        }
    }

    int reference(const QString &name, int scope, bool function) {
        VmReference reference;
        reference.name = name;
        reference.scope = scope;
        reference.function = function;
        reference.offset = code.bytecode.position();
        code.references.append(reference);

        return file.stringId(name);
    }

    void pushInt(qint32 value) {
        code.bytecode.u32(0xc0020000);
        code.bytecode.u32(value);
        convert(ValueType::INTEGER, ValueType::VARIABLE);
    }

    void call(const QString &name, quint16 argumentCount) {
        qint32 id = reference(name, -1, true);
        code.bytecode.u32(0xd9020000 | argumentCount);
        code.bytecode.u32(id);
    }

    void popUnused() {
        code.bytecode.u32(0x9e050000);
    }

    void exitSegment() {
        code.bytecode.u32(0x9d020000);
    }

    void addLocal(const QString &name) {
        code.localIds.insert(name, code.locals.size());
        code.locals.append(name);
    }
};

class ProgramGenerator {
public:
    ProgramGenerator(const Program &program, QVector<VmCode> &codes, DataWriter &file)
        : program(program), codes(codes), file(file) {
    }

    void generate() {
        // optimizable
        for (VmCode &code: codes) {
            SegmentGenerator generator{code, file};
            generator.testGenerate();
        }
    }
private:
    const Program &program;
    QVector<VmCode> &codes;
    DataWriter &file;
};

void generateProgram(const Program &program, QVector<VmCode> &codes, DataWriter &file) {
    ProgramGenerator{program, codes, file}.generate();
}
