#include "tiger/frame/frame.h"
#include "tiger/translate/translate.h"
#include "tiger/codegen/assem.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/x64frame.h"
#include "tiger/codegen/assem.h"
#include <string>
#include <sstream>

namespace F {

// frag allocator
FragList *FragAllocator::frag_head = nullptr;

// pre-allocate all these registers
TEMP::Temp *const X64Frame::rsp = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::rbp = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::rdi = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::rsi = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::rdx = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::rcx = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::r8  = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::r9  = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::rax = TEMP::Temp::NewTemp();

TEMP::Temp *const X64Frame::rbx = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::r10 = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::r11 = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::r12 = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::r13 = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::r14 = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::r15 = TEMP::Temp::NewTemp();

TEMP::Temp *X64Frame::srbx,
  *X64Frame::srbp,
  *X64Frame::sr12,
  *X64Frame::sr13,
  *X64Frame::sr14,
  *X64Frame::sr15;

// for passing parameters
TEMP::Temp *const X64Frame::param_regs[6] = {
  rdi, rsi, rdx, rcx, r8, r9
};

const TEMP::Temp *const X64Frame::all_regs[16] = 
{
  rdi, rsi, rdx, rcx, rax, rbx, rbp, rsp,
  r8, r9, r10, r11, r12, r13, r14, r15
};

// caller saved registers
typedef TEMP::TempList TL;
TL *const X64Frame::caller_saved =
  new TL(rax, new TL(rdi, new TL(rsi,
    new TL(rdx, new TL(rcx,
      new TL(r8, new TL(r9, nullptr)))))));

// temp map
TEMP::Map *X64Frame::getTempMap()
{
  static TEMP::Map *temp_map = nullptr;
  if(temp_map == nullptr)
  {
    temp_map = TEMP::Map::Empty();
    temp_map->Enter(rsp, new std::string("%rsp"));
    temp_map->Enter(rbp, new std::string("%rbp"));
    temp_map->Enter(rdi, new std::string("%rdi"));
    temp_map->Enter(rsi, new std::string("%rsi"));
    temp_map->Enter(rdx, new std::string("%rdx"));
    temp_map->Enter(rcx, new std::string("%rcx"));
    temp_map->Enter(r8, new std::string("%r8"));
    temp_map->Enter(r9, new std::string("%r9"));
    temp_map->Enter(rax, new std::string("%rax"));
    temp_map->Enter(rbx, new std::string("%rbx"));
    temp_map->Enter(r10, new std::string("%r10"));
    temp_map->Enter(r11, new std::string("%r11"));
    temp_map->Enter(r12, new std::string("%r12"));
    temp_map->Enter(r13, new std::string("%r13"));
    temp_map->Enter(r14, new std::string("%r14"));
    temp_map->Enter(r15, new std::string("%r15"));
  }
  return temp_map;
}

constexpr int X64Frame::param_reg_count =
  sizeof(param_regs) / sizeof(param_regs[0]);

X64Frame::X64Frame(TEMP::Label *name, U::BoolList *formals)
  :Frame(name)
{
  // construct formal list
  AccessList *prehead = new AccessList(nullptr, nullptr);
  AccessList *tail = prehead;
  // construct view shift statements
  T::StmList *view_shift_prehead = new T::StmList(nullptr, nullptr);
  T::StmList *view_shift_tail = view_shift_prehead;

  int param_cnt = 0;
  int formal_mem_offset = TR::word_size;  // skip return address
  while(formals) {
    // for now, all params are passed in stack
    assert(formals->head == true);
    if(formals->head == true) {
      // escaped, allocate new space on stack
      F::Access *acc = allocSpace(TR::word_size);
      tail = tail->tail = new AccessList(acc, nullptr);
      // move it to desired location
      if(param_cnt < param_reg_count)
      {
        // in reg
        view_shift_tail = view_shift_tail->tail = 
          new T::StmList(
            new T::MoveStm(
              acc->toExp(this->getFramePointerExp()),
              new T::TempExp(param_regs[param_cnt])),
          nullptr);
      }
      else
      {
        // in previous frame
        view_shift_tail = view_shift_tail->tail = 
          new T::StmList(
            new T::MoveStm(
              acc->toExp(this->getFramePointerExp()),
              new T::MemExp(new T::BinopExp(
                T::PLUS_OP,
                this->getFramePointerExp(),
                new T::ConstExp(formal_mem_offset)))),
          nullptr);
        formal_mem_offset += TR::word_size;
      }
    }
    else
    {
      // don't escape, give new temps and leave it to regalloc...
      if(param_cnt < param_reg_count) {
        TEMP::Temp *temp = TEMP::Temp::NewTemp();
        Access *acc = new InRegAccess(temp);
        tail = tail->tail = new AccessList(acc, nullptr);
        view_shift_tail = view_shift_tail->tail = 
          new T::StmList(
            new T::MoveStm(
              new T::TempExp(temp),
              new T::TempExp(param_regs[param_cnt])),
            nullptr
          );
      } else {
        fprintf(stdout, "Frame: the 7-th formal should be escaped no matter what.");
      }
    }

    formals = formals->tail;
    param_cnt++;
  }
  this->formals = prehead->tail;
  this->view_shift = view_shift_prehead->tail;
}


void X64Frame::doProcEntryExit1(T::Exp *body) {
  // this phase is done during IR translation
  // save every passed in parameter to their desired location
  // already constructed, so just concat it

  // construct SeqStm from StmList
  T::StmList *f = view_shift;
  T::SeqStm *prehead = new T::SeqStm(nullptr, nullptr, false);
  T::SeqStm *t = prehead;
  while(f) {
    t->right = new T::SeqStm(f->head, nullptr);
    t = static_cast<T::SeqStm *>(t->right);
    f = f->tail;
  }
  // save return value in %rax
  T::Stm *ret_stm = new T::MoveStm(getReturnValueExp(), body);
  t->right = ret_stm;
  F::FragAllocator::appendFrag(new ProcFrag(prehead->right, this));
}

AS::InstrList *X64Frame::doProcEntryExit2(AS::InstrList *instr)
{
  // this phase is right after codegen to prepare for liveness analysis
  // add a psudo instruction and modify liveness of registers
  // at the end of all assembly code
  static TEMP::TempList *return_sink =
    new TEMP::TempList(getR0(), new TEMP::TempList(getReturnValue(),
      new TEMP::TempList(getFramePointer(), nullptr)));
  return AS::InstrList::Splice(instr,
    new AS::InstrList(
      new AS::OperInstr("", nullptr, return_sink, nullptr), nullptr));
}

AS::Proc *X64Frame::doProcEntryExit3(AS::InstrList *instr)
{
  // this phase is after register allocation
  // at this time the frame size is determined
  // add code to expand/shrink frame here
  std::ostringstream pro_ss, epi_ss;
  pro_ss << ".set " << CG::get_framesize(this) << ", " << this->size << std::endl;
  pro_ss << this->label->Name() << ':' << std::endl;
  pro_ss << "subq $" << this->size << ", %rsp" << std::endl;
  epi_ss << "addq $" << this->size << ", %rsp" << std::endl;
  epi_ss << "ret" << std::endl << std::endl;
  return new AS::Proc(pro_ss.str(), instr, epi_ss.str());
}

void X64Frame::onEnter(CG::ASManager &a) const
{
  // save callee-save registers
  srbx = TEMP::Temp::NewTemp();
  // srbp = TEMP::Temp::NewTemp();
  sr12 = TEMP::Temp::NewTemp();
  sr13 = TEMP::Temp::NewTemp();
  sr14 = TEMP::Temp::NewTemp();
  sr15 = TEMP::Temp::NewTemp();
  a.emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(srbx, nullptr),new TEMP::TempList(rbx, nullptr)));
  // a.emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(srbp, nullptr),new TEMP::TempList(rbp, nullptr)));
  a.emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(sr12, nullptr),new TEMP::TempList(r12, nullptr)));
  a.emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(sr13, nullptr),new TEMP::TempList(r13, nullptr)));
  a.emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(sr14, nullptr),new TEMP::TempList(r14, nullptr)));
  a.emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(sr15, nullptr),new TEMP::TempList(r15, nullptr)));
}

void X64Frame::onReturn(CG::ASManager &a) const
{
  // restore them
  a.emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(rbx,NULL), new TEMP::TempList(srbx,NULL)));
  // a.emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(rbp,NULL), new TEMP::TempList(srbp,NULL)));
  a.emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(r12,NULL), new TEMP::TempList(sr12,NULL)));
  a.emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(r13,NULL), new TEMP::TempList(sr13,NULL)));
  a.emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(r14,NULL), new TEMP::TempList(sr14,NULL)));
  a.emit(new AS::MoveInstr("movq `s0, `d0", new TEMP::TempList(r15,NULL), new TEMP::TempList(sr15,NULL)));
};

Frame *NewFrame(TEMP::Label *name, U::BoolList *formals)
{
  return new X64Frame(name, formals);
}

AS::Proc *F_procEntryExit3(Frame *frame, AS::InstrList *alloc)
{
  return frame->doProcEntryExit3(alloc);
}

}  // namespace F