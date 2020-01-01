#include "tiger/liveness/flowgraph.h"
#include <iostream>

namespace FG {

TEMP::TempList* Def(InstrNode* n)
{
  AS::Instr* instr = n->NodeInfo();
  switch (instr->kind) {
  case AS::Instr::OPER:
    return ((AS::OperInstr*)instr)->dst;
  case AS::Instr::MOVE:
    return ((AS::MoveInstr*)instr)->dst;
  case AS::Instr::LABEL:
    break;
  default:
    assert(0);
  }
  return nullptr;
}

TEMP::TempList* Use(InstrNode* n)
{
  AS::Instr* instr = n->NodeInfo();
  switch (instr->kind) {
  case AS::Instr::OPER:
    return ((AS::OperInstr*)instr)->src;
  case AS::Instr::MOVE:
    return ((AS::MoveInstr*)instr)->src;
  case AS::Instr::LABEL:
    break;
  default:
    assert(0);
  }
  return nullptr;
}

bool IsMove(InstrNode* n)
{
  return n->NodeInfo()->kind == AS::Instr::MOVE;
}

FlowGraph* AssemFlowGraph(AS::InstrList* il, F::Frame* f)
{
  auto g = new FlowGraph();
  TAB::Table<TEMP::Label, InstrNode> label_table;
  std::vector<G::Node<AS::Instr>*> jmps;

  // walk through the instruction list
  // construct graph for basic blocks first
  G::Node<AS::Instr>* prev = nullptr;
  for (AS::InstrList* l = il; l; l = l->tail) {
    AS::Instr* instr = l->head;
    auto cur = g->NewNode(instr);
    if (prev)
      g->AddEdge(prev, cur);
    if (instr->kind == AS::Instr::OPER &&
      ((AS::OperInstr*)instr)->assem[0] == 'j') {
      jmps.push_back(cur);
      if(strncmp(((AS::OperInstr *)instr)->assem.c_str(), "jmp", 3) == 0) {
        prev = nullptr;
        continue;
      }
    }
    if (instr->kind == AS::Instr::LABEL)
      label_table.Enter(((AS::LabelInstr*)instr)->label, cur);
    prev = cur;
  }
  // connect jump & destinations
  for (auto jump_node : jmps) {
    AS::OperInstr* instr = (AS::OperInstr*)jump_node->NodeInfo();
    auto dst = instr->jumps->labels;
    for (; dst; dst = dst->tail) {
      auto dst_node = label_table.Look(dst->head);
      assert(dst_node != nullptr);
      g->AddEdge(jump_node, dst_node);
    }
  }
  return g;
}

} // namespace FG
