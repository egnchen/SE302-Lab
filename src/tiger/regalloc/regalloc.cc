#include "tiger/regalloc/regalloc.h"
#include <iostream>
#include <map>

namespace RA {

typedef TEMP::TempList TL;
TEMP::Map* result;

inline bool inTempList(TEMP::TempList* head, TEMP::Temp* target)
{
  for (; head; head = head->tail)
    if (head->head == target)
      return true;
  return false;
}

// clear unnecessary move operations
AS::InstrList* sweepMove(AS::InstrList* il, TEMP::Map* temp_map)
{
  AS::InstrList* pre_head = new AS::InstrList(nullptr, il);
  AS::InstrList* pre = pre_head;
  for (; il; il = il->tail) {
    if (il->head->kind == AS::Instr::MOVE) {
      AS::MoveInstr* instr = (AS::MoveInstr*)il->head;
      const std::string *dst1 = temp_map->Look(instr->src->head),
                        *dst2 = temp_map->Look(instr->dst->head);
      if (dst1 == dst2) {
        pre->tail = il->tail;
        continue;
      }
    }
    pre = il;
  }
  return pre_head->tail;
}

AS::InstrList* rewriteProgram(F::Frame* f, AS::InstrList* il,
    TEMP::TempList* spilled)
{
  F::X64Frame* fr = (F::X64Frame*)f;
  AS::InstrList* prehead = new AS::InstrList(nullptr, il);
  AS::InstrList* pre = prehead;
  std::map<TEMP::Temp*, F::InFrameAccess*> acc;
  while (il) {
    AS::Instr* instr = il->head;
    TL *dst = nullptr, *src = nullptr;
    switch (instr->kind) {
    case AS::Instr::LABEL:
      break;
    case AS::Instr::MOVE:
      dst = ((AS::MoveInstr*)instr)->dst;
      src = ((AS::MoveInstr*)instr)->src;
      break;
    case AS::Instr::OPER:
      dst = ((AS::OperInstr*)instr)->dst;
      src = ((AS::OperInstr*)instr)->src;
      break;
    }
    for (; src; src = src->tail) {
      if (inTempList(spilled, src->head)) {
        // spill it
        int offset = 0;
        if (acc.find(src->head) == acc.end())
          acc[src->head] = (F::InFrameAccess*)(f->allocSpace(TR::word_size));
        offset = acc[src->head]->offset;
        // add move inst before
        AS::Instr* nins = new AS::OperInstr(
            CG::get_x8664_movq_temp_offset_fp(offset, f),
            new TL(src->head, nullptr),
            new TL(fr->getStackPointer(), nullptr), nullptr);
        pre = pre->tail = new AS::InstrList(nins, il);
      }
    }
    for (; dst; dst = dst->tail) {
      if (inTempList(spilled, dst->head)) {
        // spill it
        int offset = 0;
        if (acc.find(dst->head) == acc.end())
          acc[dst->head] = (F::InFrameAccess*)(f->allocSpace(TR::word_size));
        offset = acc[dst->head]->offset;
        // add move inst after
        AS::Instr* nins = new AS::OperInstr(
            CG::get_x8664_movq_mem_offset_fp(offset, f),
            nullptr,
            new TL(dst->head,
                new TL(fr->getStackPointer(), nullptr)),
            nullptr);
        il = il->tail = new AS::InstrList(nins, il->tail);
      }
    }
    pre = il;
    il = il->tail;
  }
  return prehead->tail;
}

Result RegAlloc(F::Frame* f, AS::InstrList* il)
{
  // lab6: real stuff
  do {
    // do actual color assignment
    FG::FlowGraph* flow_graph = FG::AssemFlowGraph(il, f);
    // showInterference(stdout, live_result);
    COL::Result col_result = COL::Color(flow_graph);
    if (col_result.spills != nullptr) {
      il = rewriteProgram(f, il, col_result.spills);
      std::cout << "Rewritten program:" << std::endl;
      il->Print(stdout, F::X64Frame::getTempMap());
    }
    else {
      // layer the colormap
      TEMP::Map* result_map = TEMP::Map::LayerMap(col_result.coloring, F::X64Frame::getTempMap());
      // remove all unnecessary move insts
      il = sweepMove(il, result_map);
      return Result(result_map, il);
    }
  } while (1);
}

} // namespace RA