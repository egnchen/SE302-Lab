#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include "tiger/codegen/assem.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/flowgraph.h"
#include "tiger/liveness/liveness.h"
#include <set>

namespace RA {

class Result {
 public:
  TEMP::Map* coloring;
  AS::InstrList* il;
  Result(TEMP::Map *c, AS::InstrList *i)
    :coloring(c), il(i) {}
};

std::set<TEMP::Temp *> *getSpilledTemps(AS::InstrList *);

AS::InstrList *rewriteProgram(F::Frame *, AS::InstrList *, std::set<TEMP::Temp *> *);

Result RegAlloc(F::Frame* f, AS::InstrList* il);

}  // namespace RA

#endif