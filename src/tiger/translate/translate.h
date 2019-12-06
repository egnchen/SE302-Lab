#ifndef TIGER_TRANSLATE_TRANSLATE_H_
#define TIGER_TRANSLATE_TRANSLATE_H_

#include "tiger/absyn/absyn.h"
#include "tiger/frame/frame.h"

/* Forward Declarations */
namespace A {
class Exp;
}  // namespace A

namespace F {
class FragList;
class Frame;
class Access;
}

namespace U {
class BoolList;
}

namespace TR {

// forward declarations
class Exp;
class ExpAndTy;
class ExExp;
class Access;
class AccessList;

// size decls
const unsigned word_size = 8;
const unsigned int_size = 4;

// translate program - main() for phase 1
F::FragList* TranslateProgram(A::Exp *);

class Level {
 public:
  F::Frame *frame;
  Level *parent;

  Level(F::Frame *frame, Level *parent) : frame(frame), parent(parent) {}
  AccessList *getFormals();

  static Level *NewLevel(Level *parent, TEMP::Label *name,
                         U::BoolList *formals);
  
  TR::Access *AllocateLocal(bool escape, unsigned byte_count=word_size);
  void doProcEntryExit(TR::Exp *func_body);
};

Level* Outermost();

class Access {
 public:
  Level *level;
  F::Access *access;

  Access(Level *level, F::Access *access) : level(level), access(access) {}
  TR::Exp *toExp();
};

class AccessList {
 public:
  Access *head;
  AccessList *tail;

  AccessList(Access *head, AccessList *tail) : head(head), tail(tail) {}
};

class PatchList {
 public:
  TEMP::Label **head;
  PatchList *tail;

  PatchList(TEMP::Label **head, PatchList *tail) : head(head), tail(tail) {}
};

// patch functions
void do_patch(PatchList *, TEMP::Label *);
PatchList *join_patch(PatchList *, PatchList *);
// offset and size are in WORDS, which means every unit is 8 bytes.
T::Stm *makeRecordArray(T::ExpList *fields, T::Exp *r, int offset, int size);

// TR expressions

class Cx {
 public:
  PatchList *trues;
  PatchList *falses;
  T::Stm *stm;

  Cx(PatchList *trues, PatchList *falses, T::Stm *stm)
      : trues(trues), falses(falses), stm(stm) {}
};

class Exp {
 public:
  enum Kind { EX, NX, CX };

  Kind kind;

  Exp(Kind kind) : kind(kind) {}

  virtual T::Exp *UnEx() const = 0;
  virtual T::Stm *UnNx() const = 0;
  virtual Cx UnCx() const = 0;
};

class ExpAndTy {
 public:
  TR::Exp *exp;
  TY::Ty *ty;

  ExpAndTy(TR::Exp *exp, TY::Ty *ty) : exp(exp), ty(ty) {}
};

class ExExp : public Exp {
 public:
  T::Exp *exp;

  ExExp(T::Exp *exp) : Exp(EX), exp(exp) {}

  T::Exp *UnEx() const override;
  T::Stm *UnNx() const override;
  Cx UnCx() const override;
};

class NxExp : public Exp {
 public:
  T::Stm *stm;

  NxExp(T::Stm *stm) : Exp(NX), stm(stm) {}

  T::Exp *UnEx() const override;
  T::Stm *UnNx() const override;
  Cx UnCx() const override;
};

class CxExp : public Exp {
 public:
  Cx cx;

  CxExp(struct Cx cx) : Exp(CX), cx(cx) {}
  CxExp(PatchList *trues, PatchList *falses, T::Stm *stm)
      : Exp(CX), cx(trues, falses, stm) {}

  T::Exp *UnEx() const override;
  T::Stm *UnNx() const override;
  Cx UnCx() const override;
};

}  // namespace TR

#endif
