#include "tiger/regalloc/color.h"
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stack>

namespace COL {

std::map<LIVE::TNode*, LIVE::TNode*> temp_aliases;
std::stack<LIVE::TNode*> spill;

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

void removeTNode(G::Graph<TEMP::Temp>* graph, LIVE::TNode* tnode)
{
  G::NodeList<TEMP::Temp>*succ, *pred;
  while (succ = tnode->Succ())
    graph->RmEdge(tnode, succ->head);
  while (pred = tnode->Pred())
    graph->RmEdge(pred->head, tnode);
}

// visualize inteference graph
void showInterference(FILE* out, LIVE::LiveGraph live_result)
{
  fprintf(out, "digraph outputed_graph {\n");
  live_result.graph->Show(out, live_result.graph->Nodes(), [](LIVE::TNode* n) {
    std::string* s = F::X64Frame::getTempMap()->Look(n->NodeInfo());
    std::ostringstream ss;
    ss << n->Key() << "[label=\"";
    if (s)
      ss << *s << ' ';
    ss << n->NodeInfo()->Int() << "\"]" << std::endl;
    return ss.str();
  });
  for (auto m = live_result.moves; m; m = m->tail)
    if (m->valid) {
      std::cout << m->src->Key() << " -> " << m->dst->Key() << "[style=dashed]\n";
    }
  for (auto n = live_result.graph->Nodes(); n; n = n->tail) {
    if (temp_aliases.find(n->head) != temp_aliases.end())
      std::cout << n->head->Key() << " -> " << temp_aliases[n->head]->Key() << "[color=red]\n";
  }
  fprintf(out, "}\n");
}

// visualize flow graph
void showFlowGraph(FILE* out, FG::FlowGraph* flow_graph)
{
  fputs("graph flow_graph {\n", out);
  flow_graph->Show(stdout, flow_graph->Nodes(), [](G::Node<AS::Instr>* i) {
    std::ostringstream ss;
    ss << i->Key() << "[label=\"";
    switch (i->NodeInfo()->kind) {
    case AS::Instr::LABEL:
      ss << ((AS::LabelInstr*)i->NodeInfo())->label->Name();
      break;
    case AS::Instr::OPER:
      ss << ((AS::OperInstr*)i->NodeInfo())->assem;
      break;
    case AS::Instr::MOVE:
      ss << ((AS::MoveInstr*)i->NodeInfo())->assem;
      break;
    default:
      assert(0);
    }
    ss << "\"]" << std::endl;
    return ss.str();
  });
  fputs("}\n", out);
}

// merge two nodes, perform union-find
void mergeTNode(G::Graph<TEMP::Temp>* graph, LIVE::TNode* src, LIVE::TNode* dst)
{
  static TEMP::Map* hard_regs = F::X64Frame::getTempMap();

  // preconditions
  assert(hard_regs->Look(src->NodeInfo()) == nullptr);
  assert(src->NodeInfo()->Int() > dst->NodeInfo()->Int());
  // merge two nodes on graph
  // remember that inteference between hard registers are unnecessary
  if (hard_regs->Look(dst->NodeInfo())) {
    for (auto succ = src->Succ(); succ; succ = succ->tail) {
      if (!dst->GoesTo(succ->head) && !hard_regs->Look(succ->head->NodeInfo()))
        graph->AddEdge(dst, succ->head);
    }
    for (auto pred = src->Pred(); pred; pred = pred->tail) {
      if (!pred->head->GoesTo(dst) && !hard_regs->Look(pred->head->NodeInfo()))
        graph->AddEdge(pred->head, dst);
    }
  } else {
    for (auto succ = src->Succ(); succ; succ = succ->tail) {
      if (!dst->GoesTo(succ->head))
        graph->AddEdge(dst, succ->head);
    }
    for (auto pred = src->Pred(); pred; pred = pred->tail) {
      if (!pred->head->GoesTo(dst))
        graph->AddEdge(pred->head, dst);
    }
  }
  removeTNode(graph, src);

  // perform union-find
  // the impl can be lazy, but here a simpler but low perf impl
  while (temp_aliases.find(dst) != temp_aliases.end())
    dst = temp_aliases[dst];
  temp_aliases[src] = dst;
  for (auto& it : temp_aliases)
    if (it.second == src)
      it.second = dst;
}

inline LIVE::TNode* findTNode(G::NodeList<TEMP::Temp>* list, TEMP::Temp* target)
{
  for (; list; list = list->tail)
    if (list->head->NodeInfo() == target)
      return list->head;
  return nullptr;
}

inline bool cannotMove(LIVE::MoveList* ml, LIVE::TNode* t)
{
  for (; ml; ml = ml->tail)
    if (ml->valid && !ml->frozen && (ml->dst == t || ml->src == t))
      return false;
  return true;
}

bool simplify(LIVE::LiveGraph graph)
{
  std::cout << "Begin simplify" << std::endl;
  bool flag;
  bool ret = false;
  do {
    flag = false;
    for (auto nl = graph.graph->Nodes(); nl; nl = nl->tail) {
      if (F::X64Frame::getTempMap()->Look(nl->head->NodeInfo()) == nullptr && nl->head->Degree() > 0 && nl->head->Degree() < F::X64Frame::gp_regs_count && cannotMove(graph.moves, nl->head)) {
        // remove it
        flag = ret = true;
        std::cout << "Simplifying t" << (nl->head->NodeInfo()->Int()) << std::endl;
        spill.push(nl->head);
        removeTNode(graph.graph, nl->head);
      }
    }
  } while (flag);
  return ret;
}

bool coalesce(LIVE::LiveGraph graph, Result result)
{
  std::cout << "Begin coalesce" << std::endl;
  bool ret = false;
  auto hard_regs = F::X64Frame::getTempMap();
  for (LIVE::MoveList* m = graph.moves; m; m = m->tail)
    if (m->valid && !m->frozen) {
      std::cout << m->src->NodeInfo()->Int() << " -> " << m->dst->NodeInfo()->Int() << ' ';
      LIVE::TNode *s = m->src, *d = m->dst;
      while(temp_aliases.find(s) != temp_aliases.end())
        s = temp_aliases[s];
      while(temp_aliases.find(d) != temp_aliases.end())
        d = temp_aliases[d];
      if (s == d) {
        m->valid = false;
        std::cout << "already merged" << std::endl;
        continue;
      }
      if (s->GoesTo(d) || d->GoesTo(s)) {
        std::cout << "interference" << std::endl;
        continue;
      }
      if (hard_regs->Look(s->NodeInfo()) && hard_regs->Look(d->NodeInfo())) {
        std::cout << "Both hard regs" << std::endl;
        continue;
      }
      if (s->NodeInfo()->Int() < d->NodeInfo()->Int())
        std::swap(s, d);
      if (hard_regs->Look(s->NodeInfo()) || hard_regs->Look(d->NodeInfo())) {
        // george
        for (auto adj_list = s->Adj(); adj_list; adj_list = adj_list->tail) {
          if (adj_list->head->Degree() < F::X64Frame::gp_regs_count)
            continue;
          if (d->GoesTo(adj_list->head) || adj_list->head->GoesTo(d))
            continue;
          if (hard_regs->Look(d->NodeInfo()) && hard_regs->Look(s->NodeInfo()))
            continue;
          s = d = nullptr; // invalidate
          break;
        }
      } else {
        // briggs
        int cnt = 0;
        for (auto adj_list = s->Adj(); adj_list; adj_list = adj_list->tail) {
          if (!d->GoesTo(adj_list->head) && !adj_list->head->GoesTo(d))
            cnt++;
        }
        for (auto adj_list = d->Adj(); adj_list; adj_list = adj_list->tail) {
          if (!s->GoesTo(adj_list->head) && !adj_list->head->GoesTo(s))
            cnt++;
        }
        if (cnt >= F::X64Frame::gp_regs_count)
          s = d = nullptr;  // invalidate
      }
      if (s) {
        std::cout << "merging";
        ret = true;
        mergeTNode(graph.graph, s, d);
        m->valid = false;
      } else
        std::cout << "cannot merge";
      std::cout << std::endl;
    }
  return ret;
}

// freeze a low-degree move-variant node
bool freeze(LIVE::LiveGraph& graph)
{
  std::cout << "Selecting nodes to freeze" << std::endl;
  LIVE::MoveList* target = nullptr;
  int prio = 0x7f7f7f7f; // random big number
  for (auto m = graph.moves; m; m = m->tail)
    if (m->valid && !m->frozen) {
      int cur = m->dst->Degree() + m->src->Degree();
      if (cur < prio) {
        prio = cur;
        target = m;
      }
    }
  if (target == nullptr)
    return false;
  else {
    std::cout << "freezing " << target->src->NodeInfo()->Int() << " -> " << target->dst->NodeInfo()->Int() << std::endl;
    // freeze both ways
    for (auto m = graph.moves; m; m = m->tail) {
      if (m->valid && !m->frozen && m->dst == target->src && m->src == target->dst) {
        std::cout << "freezing both ways" << std::endl;
        m->frozen = true;
        break;
      }
    }
    return target->frozen = true;
  }
}

bool potentialSpill(LIVE::LiveGraph graph, FG::FlowGraph* flow_graph)
{
  std::cout << "Begin potential spill" << std::endl;
  LIVE::TNode* target = nullptr;
  float prio = 1e10; // random big number
  for (auto nl = graph.graph->Nodes(); nl; nl = nl->tail) {
    if (F::X64Frame::getTempMap()->Look(nl->head->NodeInfo()) == nullptr && nl->head->Degree() > 0 && cannotMove(graph.moves, nl->head)) {
      // calculate its priority
      // I'm too lazy to determine which one is loop, so just count def+use count
      int acc = 0;
      for (auto bb_node = flow_graph->Nodes(); bb_node; bb_node = bb_node->tail) {
        acc += inDef(bb_node->head, nl->head->NodeInfo());
        acc += inUse(bb_node->head, nl->head->NodeInfo());
      }
      float cur_prio = float(acc) / nl->head->Degree();
      std::cout << "Node " << nl->head->NodeInfo()->Int() << " priority " << cur_prio << std::endl;
      if (cur_prio < prio) {
        prio = cur_prio;
        target = nl->head;
      }
    }
  }
  if (target == nullptr)
    return false;
  std::cout << "Potential spill t" << (target->NodeInfo()->Int()) << std::endl;
  spill.push(target);
  removeTNode(graph.graph, target);
  return true;
}

void selectColor(LIVE::LiveGraph graph_copy, Result& result)
{
  G::Graph<TEMP::Temp>* graph = graph_copy.graph;
  G::NodeList<TEMP::Temp>* nodes = graph->Nodes();
  LIVE::MoveList* move_list = graph_copy.moves;

  std::cout << "Begin color selection." << std::endl;
  const std::set<TEMP::Temp*>& gp_regs = F::X64Frame::gp_regs;
  TEMP::Map* hard_temp_map = F::X64Frame::getTempMap();

  TEMP::TempList* actual_spill_prehead = new TEMP::TempList(nullptr, nullptr);
  TEMP::TempList* actual_spill_tail = actual_spill_prehead;
  std::map<TEMP::Temp*, TEMP::Temp*> color;

  // iterate through temp aliases
  // generate color mapping based on it
  // redo node merging operations on copied graph
  for (auto& it : temp_aliases) {
    color[it.first->NodeInfo()] = it.second->NodeInfo();
    LIVE::TNode *src = findTNode(nodes, it.first->NodeInfo()),
                *dst = findTNode(nodes, it.second->NodeInfo());
    assert(src && dst);
    std::cout << "Merging " << src->NodeInfo()->Int() << "->" << dst->NodeInfo()->Int() << std::endl;
    mergeTNode(graph, src, dst);
  }
  // showInterference(stdout, graph_copy);

  // pop the stack
  std::cout << "Stack have " << spill.size() << " nodes." << std::endl;
  while (!spill.empty()) {
    auto n = spill.top();
    spill.pop();
    auto t = n->NodeInfo();

    std::cout << "Coloring " << t->Int() << ' ';
    if (color.find(t) != color.end()) {
      // already spilled / coalesced
      std::cout << "already spilled or coalesced" << std::endl;
      continue;
    }
    if (gp_regs.find(t) != gp_regs.end()) {
      std::cout << "skipping hardware register" << std::endl;
      continue;
    }
    // use the corresponding TNode in copied graph
    n = findTNode(nodes, t);
    // copy construction the available register set
    // remove interferenced ones and select the first one
    std::set<TEMP::Temp*> avail = std::set<TEMP::Temp*>(gp_regs);
    for (auto a = n->Adj(); a; a = a->tail) {
      // std::cout << "remove adj " << a->head->NodeInfo()->Int() << std::endl;
      if (avail.find(a->head->NodeInfo()) != avail.end())
        avail.erase(a->head->NodeInfo());
      if (color.find(a->head->NodeInfo()) != color.end())
        avail.erase(color[a->head->NodeInfo()]);
    }
    if (avail.empty()) {
      // do actual spill
      std::cout << "spilled";
      actual_spill_tail = actual_spill_tail->tail = new TEMP::TempList(n->NodeInfo(), nullptr);
      removeTNode(graph, n);
    } else {
      // assign color
      std::cout << "assigned " << (*avail.begin())->Int();
      color[t] = *avail.begin();
    }
    std::cout << std::endl;
  }

  // assert that all nodes have color, or they have been spilled
  for (auto m = nodes; m; m = m->tail) {
    // skip hardware registers
    if (hard_temp_map->Look(m->head->NodeInfo()))
      continue;
    if (color.find(m->head->NodeInfo()) == color.end()) {
      // find in spill list
      bool flag = false;
      for (auto l = actual_spill_prehead->tail; l; l = l->tail) {
        if (l->head == m->head->NodeInfo()) {
          flag = true;
          break;
        }
      }
      assert(flag);
    }
  }

  // return the result
  // need to further union-find since we've assigned new colors
  if (result.coloring == nullptr)
    result.coloring = TEMP::Map::Empty();
  for (auto& it : color) {
    // union-find
    while (hard_temp_map->Look(it.second) == nullptr && color.find(it.second) != color.end())
      it.second = color[it.second];
    if (hard_temp_map->Look(it.second)) {
      std::cout << "Entering " << it.first->Int() << " -> " << *(hard_temp_map->Look(it.second)) << std::endl;
      result.coloring->Enter(it.first, hard_temp_map->Look(it.second));
    }
  }
  result.spills = actual_spill_prehead->tail;
  return;
}

Result Color(FG::FlowGraph* flow_graph)
{
  Result result;
  result.coloring = TEMP::Map::Empty();
  result.spills = nullptr;
  temp_aliases.clear();

  spill = std::stack<LIVE::TNode*>();
  bool flag = false;
  // showFlowGraph(stdout, flow_graph);
  LIVE::LiveGraph graph, graph_copy;
  graph_copy = LIVE::Liveness(flow_graph);
  graph = LIVE::Liveness(flow_graph);
  std::cout << "completed liveness analysis" << std::endl;
  // showInterference(stdout, graph);
  std::cout << "coloring phase begin" << std::endl;
  do {
    do {
      do
        simplify(graph);
      while (coalesce(graph, result));
    } while (freeze(graph));
  } while (potentialSpill(graph, flow_graph));
  showInterference(stdout, graph);
  selectColor(graph_copy, result);
  return result;
}

} // namespace COL