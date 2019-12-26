#ifndef TIGER_LIVENESS_FLOWGRAPH_H_
#define TIGER_LIVENESS_FLOWGRAPH_H_

#include "tiger/codegen/assem.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/util/graph.h"
#include "tiger/util/table.h"
#include "tiger/translate/tree.h"
#include <cstring>
#include <vector>

namespace FG {

typedef G::Node<AS::Instr> InstrNode;
typedef G::Graph<AS::Instr> FlowGraph;

TEMP::TempList* Def(InstrNode* n);
TEMP::TempList* Use(InstrNode* n);

bool IsMove(InstrNode* n);

FlowGraph* AssemFlowGraph(AS::InstrList* il, F::Frame* f);

}  // namespace FG

#endif