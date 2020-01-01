#ifndef __TIGER_FRAME_X64FRAME_H_
#define __TIGER_FRAME_X64FRAME_H_

#include "tiger/frame/frame.h"
#include "tiger/translate/translate.h"
#include "tiger/codegen/assem.h"
#include "tiger/frame/frame.h"
#include <string>
#include <set>

namespace F {

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
  T::TempExp *exp;

  InRegAccess(TEMP::Temp* reg) : Access(INREG), reg(reg) {
    exp = new T::TempExp(reg);
  }
  T::Exp *toExp(T::Exp *framePtr) const {
    return exp;
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
public:
  static const int param_reg_count;
  // registers for function calling
  static TEMP::Temp *const rbp;
  static TEMP::Temp *const rsp;
  static TEMP::Temp *const rdi;
  static TEMP::Temp *const rsi;
  static TEMP::Temp *const rdx;
  static TEMP::Temp *const rcx;
  static TEMP::Temp *const r8;
  static TEMP::Temp *const r9;
  static TEMP::Temp *const param_regs[6];

  static TEMP::Temp *const rax; // for return value
  // others
  static TEMP::Temp *const rbx;
  static TEMP::Temp *const r10;
  static TEMP::Temp *const r11;
  static TEMP::Temp *const r12;
  static TEMP::Temp *const r13;
  static TEMP::Temp *const r14;
  static TEMP::Temp *const r15;

  // caller saved registers
  static TEMP::TempList *const caller_saved;

  // for register saving
  static TEMP::Temp *srbx, *srbp, *srdi, *srsi, *sr12, *sr13, *sr14, *sr15;

  // for register naming
  static TEMP::Map *getTempMap();

  // for utilities
  static const std::set<TEMP::Temp *> all_regs;
  static const std::set<TEMP::Temp *> gp_regs;
  static const int gp_regs_count;

  X64Frame(TEMP::Label *name): Frame(name) {}
  X64Frame(TEMP::Label *name, U::BoolList *formals);
  ~X64Frame() {}
  Access *allocSpace(unsigned byte_count, bool in_frame=true) override {
    if(in_frame) {
      size += byte_count;
      return new InFrameAccess(-size);
    } else 
      return new InRegAccess(TEMP::Temp::NewTemp());
  }
  
  T::Exp *getFramePointerExp() const override { return new T::TempExp(rbp); }
  TEMP::Temp *getFramePointer() const override { return rbp; }

  T::Exp *getStackPointerExp() const override { return new T::TempExp(rsp); }
  TEMP::Temp *getStackPointer() const override { return rsp; }
  
  T::Exp *getReturnValueExp() const override { return new T::TempExp(rax); }
  TEMP::Temp *getReturnValue() const override { return rax; }

  // r0 is r15
  // which means r15 is always zero
  TEMP::Temp *getR0() const override { return r15; }
  
  T::CallExp *externalCall(std::string name, T::ExpList *args) override {
    return new T::CallExp(
      new T::NameExp(TEMP::NamedLabel(name)), args);
  }

  void doProcEntryExit1(T::Exp *body) override;
  AS::InstrList *doProcEntryExit2(AS::InstrList *instr) override;
  AS::Proc *doProcEntryExit3(AS::InstrList *instr) override;

  void onEnter(CG::ASManager &) const override;
  void onReturn(CG::ASManager &) const override;

  unsigned getSize() { return (unsigned)size; }
  AccessList *getFormals() { return formals; }
};

} // namespace F
#endif