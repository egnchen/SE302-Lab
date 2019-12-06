#include "tiger/frame/frame.h"
#include "tiger/translate/translate.h"
#include "tiger/codegen/assem.h"
#include <string>

namespace F {


// frag allocator
FragList *FragAllocator::frag_head = nullptr;

class InFrameAccess : public Access {
 public:
  int offset;

  InFrameAccess(int offset) : Access(INFRAME), offset(offset) {}
  T::Exp *toExp(T::Exp *framePtr) const {
    return new T::MemExp(new T::BinopExp(T::PLUS_OP,
      framePtr, new T::ConstExp(offset)));
  }
};

class InRegAccess : public Access {
 public:
  TEMP::Temp* reg;

  InRegAccess(TEMP::Temp* reg) : Access(INREG), reg(reg) {}
  T::Exp *toExp(T::Exp *framePtr) const {
    return new T::TempExp(reg);
  }
};


//caller_saved register: rax rdi rsi rdx rcx r8 r9 r10 r11
//callee_saved register: rbx rbp r12 r13 r14 r15 

class X64Frame : public Frame {
  // frame implemented in lab5
private:
  int size = 0;
  AccessList *formals = nullptr;
  T::StmList *view_shift;
  static const int param_reg_count;
  // registers for function calling
  static TEMP::Temp *const rsp;
  static TEMP::Temp *const rdi;
  static TEMP::Temp *const rsi;
  static TEMP::Temp *const rdx;
  static TEMP::Temp *const rcx;
  static TEMP::Temp *const r8;
  static TEMP::Temp *const r9;
  static TEMP::Temp *const param_regs[6];

  static TEMP::Temp *const rax; // for return value
public:

  X64Frame(TEMP::Label *name): Frame(name) {}
  X64Frame(TEMP::Label *name, U::BoolList *formals);
  ~X64Frame() {}
  Access *allocSpace(unsigned byte_count) {
    int ret = size;
    size += byte_count;
    return new InFrameAccess(ret);
  }
  
  T::Exp *getFramePointerExp() { return new T::TempExp(rsp); }
  TEMP::Temp *getFramePointer() { return rsp; }
  
  T::Exp *getReturnValueExp() { return new T::TempExp(rax); }
  TEMP::Temp *getReturnValue() { return rax; }
  
  T::CallExp *externalCall(std::string name, T::ExpList *args) {
    return new T::CallExp(
      new T::NameExp(TEMP::NamedLabel(name)), args);
  }

  void doProcEntryExit1(T::Exp *body) {
    // save every passed in parameter to their desired location
    // in regfile or on stack
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

  AS::InstrList *doProcEntryExit2(AS::InstrList *instr) {
    return nullptr;
  }

  AS::Proc *doProcEntryExit3(AS::InstrList *instr) {
    return nullptr;
  }

  void onEnter() {}
  void onReturn() {}

  unsigned getSize() { return (unsigned)size; }
  AccessList *getFormals() { return formals; }
};

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

Frame *NewFrame(TEMP::Label *name, U::BoolList *formals)
{
  return new X64Frame(name, formals);
}

// pre-allocate all these registers
TEMP::Temp *const X64Frame::rsp = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::rdi = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::rsi = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::rdx = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::rcx = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::r8  = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::r9  = TEMP::Temp::NewTemp();
TEMP::Temp *const X64Frame::rax = TEMP::Temp::NewTemp();

TEMP::Temp *const X64Frame::param_regs[6] = {
  rdi, rsi, rdx, rcx, r8, r9
};

constexpr int X64Frame::param_reg_count = sizeof(param_regs) / sizeof(param_regs[0]);

AS::Proc *F_procEntryExit3(Frame *frame, AS::InstrList *alloc)
{
  return frame->doProcEntryExit3(alloc);
}

}  // namespace F