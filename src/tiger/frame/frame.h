#ifndef TIGER_FRAME_FRAME_H_
#define TIGER_FRAME_FRAME_H_

#include <string>

#include "tiger/codegen/assem.h"
#include "tiger/translate/tree.h"
#include "tiger/translate/translate.h"
#include "tiger/util/util.h"
#include "tiger/codegen/codegen.h"

// forward declaration
namespace AS {
class Proc;
class InstrList;
}

namespace CG {
class ASManager;
}

namespace F {

class Frame;

class Access {
 public:
  enum Kind { INFRAME, INREG };

  Kind kind;

  Access(Kind kind) : kind(kind) {}

  virtual T::Exp *toExp(T::Exp *framePtr) const = 0;
};

class AccessList {
 public:
  Access *head;
  AccessList *tail;

  AccessList(Access *head, AccessList *tail) : head(head), tail(tail) {}
};


class Frame {
  // Base class
public:
  TEMP::Label *label;

  Frame(TEMP::Label *name):label(name) {}
  virtual ~Frame() {}

  // default to allocate a word
  virtual Access *allocSpace(unsigned byte_count, bool in_frame=true) = 0;
  // frame pointer
  virtual T::Exp *getFramePointerExp() const = 0;
  virtual TEMP::Temp *getFramePointer() const = 0;
  // stack pointer
  virtual T::Exp *getStackPointerExp() const = 0;
  virtual TEMP::Temp *getStackPointer() const = 0;
  // return value
  virtual TEMP::Temp *getReturnValue() const = 0;
  virtual T::Exp *getReturnValueExp() const = 0;

  virtual TEMP::Temp *getR0() const = 0;
  // function calls
  
  // proc entry exit 1
  // 1st phase in ir translation
  // only translate return value
  // others will be done during following phases
  virtual void doProcEntryExit1(T::Exp *body) = 0;
  virtual AS::InstrList *doProcEntryExit2(AS::InstrList *instr) = 0;
  virtual AS::Proc *doProcEntryExit3(AS::InstrList *instr) = 0;
  virtual T::CallExp *externalCall(std::string, T::ExpList *) = 0;
  virtual unsigned getSize() = 0;
  virtual AccessList *getFormals() = 0;

  virtual void onEnter(CG::ASManager &) const = 0;
  virtual void onReturn(CG::ASManager &) const = 0;
};

// default frame allocator
// insert platform-specific frame into here
Frame *NewFrame(TEMP::Label *name, U::BoolList *formals);

/*
 * Fragments
 */

class Frag {
 public:
  enum Kind { STRING, PROC };

  Kind kind;

  Frag(Kind kind) : kind(kind) {}
};

class StringFrag : public Frag {
 public:
  TEMP::Label *label;
  std::string str;

  StringFrag(TEMP::Label *label, std::string str)
      : Frag(STRING), label(label), str(str) {}
};

class ProcFrag : public Frag {
 public:
  T::Stm *body;
  Frame *frame;

  ProcFrag(T::Stm *body, Frame *frame) : Frag(PROC), body(body), frame(frame) {}
};

class FragList {
 public:
  Frag *head;
  FragList *tail;

  FragList(Frag *head, FragList *tail) : head(head), tail(tail) {}
};


class FragAllocator {
  static F::FragList *frag_head;
public:
  static void appendFrag(Frag *frag) {
    static F::FragList *frag_tail = nullptr;
    if(frag_head == nullptr) {
      frag_head = new F::FragList(frag, nullptr);
      frag_tail = frag_head;
    }
    else
      frag_tail = frag_tail->tail = new FragList(frag, nullptr);
  }

  static F::FragList *getFragListHead() {
    return frag_head;
  }
};

// proc entry exit 3 - done after register allocation
AS::Proc *F_procEntryExit3(Frame *frame, AS::InstrList *alloc);

}  // namespace F

#endif