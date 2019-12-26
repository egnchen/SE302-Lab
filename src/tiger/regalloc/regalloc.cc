#include "tiger/regalloc/regalloc.h"
#include "tiger/frame/x64frame.h"
#include <set>
#include <map>

namespace RA {

typedef TEMP::TempList TL;

std::set<TEMP::Temp *> *getSpilledTemps(AS::InstrList *il)
{
  std::set<TEMP::Temp *> *ret = new std::set<TEMP::Temp *>();
  TEMP::Map *colored = F::X64Frame::getTempMap();
  while(il)
  {
    AS::Instr *instr = il->head;
    TEMP::TempList *src_list, *dst_list;
    switch(instr->kind)
    {
      case AS::Instr::LABEL:
        break;
      case AS::Instr::MOVE:
      {
        src_list = ((AS::MoveInstr *)instr)->src;
        dst_list = ((AS::MoveInstr *)instr)->dst;
        break;
      }
      case AS::Instr::OPER:
      {
        src_list = ((AS::OperInstr *)instr)->src;
        dst_list = ((AS::OperInstr *)instr)->dst;
        break;
      }
    }
    while(src_list)
    {
      if(colored->Look(src_list->head) == nullptr
        && ret->find(src_list->head) == ret->end())
        ret->insert(src_list->head);
      src_list = src_list->tail;
    }
    while(dst_list)
    {
      if(colored->Look(dst_list->head) == nullptr
        && ret->find(dst_list->head) == ret->end())
        ret->insert(dst_list->head);
      dst_list = dst_list->tail;
    }
    il = il->tail;
  }
  return ret;
}


AS::InstrList *rewriteProgram(F::Frame *f, AS::InstrList *il,
  std::set<TEMP::Temp *> *spilled)
{
  // for now we put escaped dst in r14
  // escaped src in r10, r11, r12, r13
  F::X64Frame *fr = (F::X64Frame *)f;
  static TL *esc_dst_head = new TL(fr->r14, nullptr);
  static TL *esc_src_head = new TL(fr->r10, new TL(fr->r11, new TL(fr->r12, new TL(fr->r13, nullptr))));
  AS::InstrList *prehead = new AS::InstrList(nullptr, il);
  AS::InstrList *pre = prehead;
  std::map<TEMP::Temp *, F::InFrameAccess *> acc;
  while(il)
  {
    AS::Instr *instr = il->head;
    TL *dst = nullptr, *src = nullptr;
    switch(instr->kind) {
      case AS::Instr::LABEL:
        break;
      case AS::Instr::MOVE:
        dst = ((AS::MoveInstr *)instr)->dst;
        src = ((AS::MoveInstr *)instr)->src;
        break;
      case AS::Instr::OPER:
        dst = ((AS::OperInstr *)instr)->dst;
        src = ((AS::OperInstr *)instr)->src;
        break;
    }
    TL *esc_dst = esc_dst_head;
    TL *esc_src = esc_src_head;
    while(src) {
      if(spilled->find(src->head) != spilled->end()) {
        // spill it
        int offset = 0;
        if(acc.find(src->head) == acc.end())
          acc[src->head] = (F::InFrameAccess *)(f->allocSpace(TR::word_size));
        offset = acc[src->head]->offset;
        assert(esc_src);
        // do the substitution
        src->head = esc_src->head;
        // add mov instruction before
        AS::Instr *nins = new AS::OperInstr(
          CG::get_x8664_movq_temp_offset_fp(offset, f),
          new TL(esc_src->head, nullptr),
          new TL(fr->getStackPointer(), nullptr), nullptr);
        pre = pre->tail = new AS::InstrList(nins, il);
        esc_src = esc_src->tail;
      }
      src = src->tail;
    }
    while(dst) {
      if(spilled->find(dst->head) != spilled->end()) {
        // spill it
        int offset = 0;
        if(acc.find(dst->head) == acc.end())
          acc[dst->head] = (F::InFrameAccess *)(f->allocSpace(TR::word_size));
        offset = acc[dst->head]->offset;
        assert(esc_dst);
        // substitude the destination
        dst->head = esc_dst->head;
        // do moving after
        AS::Instr *nins = new AS::OperInstr(
          CG::get_x8664_movq_mem_offset_fp(offset, f),
          nullptr,
          new TL(esc_dst->head,
            new TL(fr->getStackPointer(), nullptr)),
          nullptr);
        il->tail = new AS::InstrList(nins, il->tail);
        esc_dst = esc_dst->tail;
      }
      dst = dst->tail;
    }
    pre = il;
    il = il->tail;
  }
  return prehead->tail;
}

Result RegAlloc(F::Frame* f, AS::InstrList* il) {
  // lab6: real stuff
  AS::InstrList *ret = il;
  do
  {
    std::set<TEMP::Temp *> *spilled = getSpilledTemps(il);
    if(!spilled->empty()) {
      ret = rewriteProgram(f, il, spilled);
    }
    else {
      break;
    }
  } while(1);
  return Result(F::X64Frame::getTempMap(), ret);
}

}  // namespace RA