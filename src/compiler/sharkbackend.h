#ifndef SHARKBACKEND_H
#define SHARKBACKEND_H

#include <QVector>
#include "gmlcompiler.h"
#include "datawriter.h"

#include "shark/intermediate/program.h"

void generateProgram(const Shark::Intermediate::Program &program, QVector<VmCode> &codes, DataWriter &file);

#endif // SHARKBACKEND_H
