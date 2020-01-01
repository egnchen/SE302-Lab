#ifndef TIGER_REGALLOC_COLOR_H_
#define TIGER_REGALLOC_COLOR_H_

#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/util/graph.h"
#include "tiger/frame/x64frame.h"
#include "tiger/util/table.h"

namespace COL {

class Result {
 public:
  TEMP::Map* coloring;
  TEMP::TempList *spills;
};

Result Color(FG::FlowGraph *flow_graph);

}  // namespace COL

#endif