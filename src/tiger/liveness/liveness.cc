#include "tiger/liveness/liveness.h"
#include "tiger/frame/x64frame.h"
#include <map>
#include <iostream>

namespace LIVE {

bool LiveList::flag = true;

// code snippets

// determine whether one temp is in def
inline bool inDef(FG::InstrNode* src, TEMP::Temp* target)
{
  for (auto defs = FG::Def(src); defs; defs = defs->tail) {
    if (defs->head == target)
      return true;
  }
  return false;
}

// determine whether one temp is in use
inline bool inUse(FG::InstrNode* src, TEMP::Temp* target)
{
  for (auto uses = FG::Use(src); uses; uses = uses->tail) {
    if (uses->head == target)
      return true;
  }
  return false;
}

inline bool inDst(FG::InstrNode* src, TEMP::Temp* target)
{
  assert(src->NodeInfo()->kind == AS::Instr::MOVE);
  for (auto i = ((AS::MoveInstr*)src->NodeInfo())->dst; i; i = i->tail) {
    if (i->head == target)
      return true;
  }
  return false;
}

// do liveness analysis on flow graph
// generate conflict graph and move relationship list
LiveGraph Liveness(G::Graph<AS::Instr>* flowgraph)
{
  LiveTable live_table;
  LiveGraph lg;

  // prep: put all instrs into live table
  for (auto nl = flowgraph->Nodes(); nl; nl = nl->tail) {
    FG::InstrNode* in = nl->head;
    live_table.Enter(in, new LiveList());
  }

  // calculate in & out sets for each instruction
  do {
    LiveList::reset(); // reset change flag
    for (auto nl = flowgraph->Nodes(); nl; nl = nl->tail) {
      FG::InstrNode* in = nl->head;
      LiveList* ll = live_table.Look(in);
      /*
       * in[n] = use[n] \cup (out[n] - def[n])
       * out[n] = \cup{\forall s \in succ[n] in[s]}
       */
      // use rule
      for (auto uses = FG::Use(in); uses; uses = uses->tail) {
        ll->insertIn(uses->head);
      }
      // def rule
      for (auto t : ll->getOut())
        if (!inDef(in, t))
          ll->insertIn(t);
      // backprop
      for (auto it = in->Pred(); it; it = it->tail) {
        LiveList* prev_ll = live_table.Look(it->head);
        for (TEMP::Temp* t : ll->getIn())
          prev_ll->insertOut(t);
      }
    }
  } while (LiveList::changed());

  // construct interference graph
  auto graph = new G::Graph<TEMP::Temp>();

  // add hard register to graph & add inteference between them
  // TODO is this really necessary?

  // add all temps to graph
  std::map<TEMP::Temp*, LIVE::TNode*> temp_map;
  for (auto nl = flowgraph->Nodes(); nl; nl = nl->tail) {
    FG::InstrNode* in = nl->head;
    for (auto defs = FG::Def(in); defs; defs = defs->tail)
      if (temp_map.find(defs->head) == temp_map.end())
        temp_map[defs->head] = graph->NewNode(defs->head);
    for (auto uses = FG::Use(in); uses; uses = uses->tail)
      if (temp_map.find(uses->head) == temp_map.end())
        temp_map[uses->head] = graph->NewNode(uses->head);
  }
  // remove rsp, it is not a gpreg
  temp_map.erase(F::X64Frame::rsp);

  // add inteference edges
  auto hard_temp = F::X64Frame::getTempMap();
  for (auto nl = flowgraph->Nodes(); nl; nl = nl->tail) {
    FG::InstrNode* in = nl->head;
    LiveList* ll = live_table.Look(in);
    bool is_move = FG::IsMove(in);
    for (auto defs = FG::Def(in); defs; defs = defs->tail) {
      if(defs->head == F::X64Frame::rsp)
        continue;
      for (TEMP::Temp* t : ll->getOut()) {
        if (defs->head == t || (is_move && inDst(in, t)) ||
          t == F::X64Frame::rsp)
          continue;
        if (hard_temp->Look(defs->head) && hard_temp->Look(t))
          // two hard registers, cannot merge anyway
          continue;
        graph->AddEdge(temp_map[defs->head], temp_map[t]);
      }
    }
  }
  lg.graph = graph;

  // construct move list
  MoveList* pre_head = new MoveList(nullptr, nullptr, nullptr);
  MoveList* tail = pre_head;
  for (auto nl = flowgraph->Nodes(); nl; nl = nl->tail) {
    FG::InstrNode* in = nl->head;
    if (in->NodeInfo()->kind != AS::Instr::MOVE)
      continue;
    AS::MoveInstr* instr = (AS::MoveInstr*)in->NodeInfo();
    // assume that src & dst are both single element lists
    assert(instr->src->tail == nullptr);
    assert(instr->dst->tail == nullptr);
    tail = tail->tail = new MoveList(temp_map[instr->src->head], 
      temp_map[instr->dst->head], nullptr);
  }
  lg.moves = pre_head->tail;

  return lg;
}

} // namespace LIVE