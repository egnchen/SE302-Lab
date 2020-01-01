#ifndef TIGER_LIVENESS_LIVENESS_H_
#define TIGER_LIVENESS_LIVENESS_H_

#include "tiger/codegen/assem.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/flowgraph.h"
#include "tiger/util/graph.h"
#include "tiger/util/table.h"
#include <set>

namespace LIVE {

typedef G::Node<TEMP::Temp> TNode;

struct LiveList {
private:
  static bool flag;
  std::set<TEMP::Temp *> in, out;
public:
  static void reset() { flag = false; }
  static bool changed() { return flag; }
  inline bool inIn(TEMP::Temp *t) { return in.find(t) != in.end(); }
  inline bool inOut(TEMP::Temp *t) { return out.find(t) != out.end(); }
  inline void insertIn(TEMP::Temp *t) { if(in.insert(t).second) flag = true; }
  inline void insertOut(TEMP::Temp *t) { if(out.insert(t).second) flag = true; }
  inline const std::set<TEMP::Temp *> getIn() { return in; }
  inline const std::set<TEMP::Temp *> getOut() { return out; }
};

// log in/out set for each instruction
typedef TAB::Table<FG::InstrNode, LiveList> LiveTable;

class MoveList {
 public:
  TNode *src, *dst;
  MoveList* tail;
  bool valid;
  bool frozen;

  MoveList(TNode *src, TNode* dst, MoveList* tail)
      : src(src), dst(dst), tail(tail), valid(true), frozen(false) {}
};

class LiveGraph {
 public:
  G::Graph<TEMP::Temp>* graph;
  MoveList* moves;
};

LiveGraph Liveness(G::Graph<AS::Instr>* flowgraph);

}  // namespace LIVE

#endif