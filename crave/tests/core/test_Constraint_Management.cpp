//****************************************************************************************
//	MIT License
//****************************************************************************************
//	Copyright (c) 2012-2020 University of Bremen, Germany. 
//  	Copyright (c) 2015-2020 DFKI GmbH Bremen, Germany.
//  	Copyright (c) 2020 Johannes Kepler University Linz, Austria.
//
//	Permission is hereby granted, free of charge, to any person obtaining a copy
//	of this software and associated documentation files (the "Software"), to deal
//	in the Software without restriction, including without limitation the rights
//	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//	copies of the Software, and to permit persons to whom the Software is
//	furnished to do so, subject to the following conditions:
//
//	The above copyright notice and this permission notice shall be included in all
//	copies or substantial portions of the Software.
//
//	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//	SOFTWARE.
//****************************************************************************************

#include <boost/test/unit_test.hpp>

#include <crave/backend/Generator.hpp>

#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/assign/list_of.hpp>

#include <set>
#include <iostream>
#include <memory>
#include <vector>

using std::ostream;
using boost::format;
using namespace crave;
using boost::assign::list_of;

void print_vec(std::ostream& out, std::vector<std::string> const& v) {
  out << "( ";
  BOOST_FOREACH (std::string u, v) { out << u << ' '; }
  out << ')';
}

void print_vec_vec(std::ostream& out, std::vector<std::vector<std::string> > const& v) {
  out << "( ";
  BOOST_FOREACH (std::vector<std::string> const& u, v) {
    print_vec(out, u);
    out << ' ';
  }
  out << ')';
}

bool cmp_vec(std::vector<std::string> const& a, std::vector<std::string> const& b) {
  if (a.size() == b.size()) {
    for (unsigned i = 0; i < a.size(); i++) {
      if (a[i].compare(b[i]) != 0) {
        return a[i].compare(b[i]) < 0;
      }
    }
  }
  return a.size() < b.size();
}

// sortieren
void sort_results(std::vector<std::vector<std::string> >& results) {
  for (unsigned i = 0; i < results.size(); i++) {
    sort(results[i].begin(), results[i].end());
  }
  sort(results.begin(), results.end(), cmp_vec);
}

BOOST_FIXTURE_TEST_SUITE(Constraint_Management_t, Context_Fixture)

class Item : public rand_obj {
 public:
  Item() : rand_obj(), a(this), b(this) {
    constraint("sum", a() + b() == 4);
    constraint("product", a() * b() == 4);
    constraint(a() < 10);
    constraint(b() < 10);
    constraint("x", a() != 2);
  }
  randv<unsigned> a;
  randv<unsigned> b;

  friend ostream& operator<<(ostream& os, const Item& it) {
    os << it.a << " " << it.b;
    return os;
  }
};

BOOST_AUTO_TEST_CASE(t1) {
  Item it;

  BOOST_REQUIRE(!it.next());
  BOOST_REQUIRE(it.is_constraint_enabled("sum"));
  BOOST_REQUIRE(it.is_constraint_enabled("product"));
  BOOST_REQUIRE(it.is_constraint_enabled("x"));

  BOOST_REQUIRE(it.disable_constraint("sum"));
  BOOST_REQUIRE(it.next());
  BOOST_REQUIRE(!it.is_constraint_enabled("sum"));
  BOOST_REQUIRE(it.is_constraint_enabled("product"));
  BOOST_REQUIRE(it.is_constraint_enabled("x"));
  std::cout << it << std::endl;

  BOOST_REQUIRE(it.disable_constraint("product"));
  BOOST_REQUIRE(it.next());
  BOOST_REQUIRE(!it.is_constraint_enabled("sum"));
  BOOST_REQUIRE(!it.is_constraint_enabled("product"));
  BOOST_REQUIRE(it.is_constraint_enabled("x"));
  std::cout << it << std::endl;

  BOOST_REQUIRE(it.enable_constraint("sum"));
  BOOST_REQUIRE(it.next());
  BOOST_REQUIRE(it.is_constraint_enabled("sum"));
  BOOST_REQUIRE(!it.is_constraint_enabled("product"));
  BOOST_REQUIRE(it.is_constraint_enabled("x"));
  std::cout << it << std::endl;

  BOOST_REQUIRE(it.enable_constraint("product"));
  BOOST_REQUIRE(!it.next());
  BOOST_REQUIRE(it.is_constraint_enabled("sum"));
  BOOST_REQUIRE(it.is_constraint_enabled("product"));
  BOOST_REQUIRE(it.is_constraint_enabled("x"));

  BOOST_REQUIRE(it.disable_constraint("x"));
  BOOST_REQUIRE(it.next());
  BOOST_REQUIRE(it.is_constraint_enabled("sum"));
  BOOST_REQUIRE(it.is_constraint_enabled("product"));
  BOOST_REQUIRE(!it.is_constraint_enabled("x"));

  std::cout << it << std::endl;
  BOOST_REQUIRE(it.a == 2 && it.b == 2);
}

class Item1 : public rand_obj {
 public:
  Item1() : rand_obj(), a(this) {
    constraint("abc", a() == 4);
    constraint("def", a() == 3);
    constraint("abc", a() == 3);
  }
  randv<unsigned> a;
};

BOOST_AUTO_TEST_CASE(t2) {
  BOOST_REQUIRE_THROW(new Item1, std::runtime_error);
}

class ItemPythagoras : public rand_obj {
 public:
  ItemPythagoras() : rand_obj(), a(this), b(this), c(this) {
    constraint("pythagoras", a() * a() + b() * b() == c() * c());
    constraint(a() > 0);
    constraint(b() > 0);
  }
  randv<unsigned char> a;
  randv<unsigned char> b;
  randv<unsigned char> c;

  friend ostream& operator<<(ostream& os, const ItemPythagoras& it) {
    os << it.a << " " << it.b << " " << it.c;
    return os;
  }
};

BOOST_AUTO_TEST_CASE(Pythagoras) {
  ItemPythagoras it;

  BOOST_REQUIRE(it.next());
  std::cout << it << std::endl;

  it.disable_constraint("pythagoras");
  BOOST_REQUIRE(it.next());

  it.enable_constraint("pythagoras");
  BOOST_REQUIRE(it.next());
  std::cout << it << std::endl;
}

class ItemPacketBaseConstraint : public rand_obj {
 public:
  ItemPacketBaseConstraint() : rand_obj(), i_(), src_addr(this), dest_addr(this), msg_length(this), msg(this) {
    constraint(msg_length() < 80);
    constraint(msg_length() > 2);
    constraint(src_addr() != dest_addr());
    constraint(msg().size() == msg_length());
    constraint(foreach (msg(), msg()[i_] >= ' ' && msg()[i_] <= 'z'));
  }

  placeholder i_;
  randv<unsigned> src_addr;
  randv<unsigned> dest_addr;
  randv<unsigned short> msg_length;
  rand_vec<char> msg;
};

class ItemPacketHierConstraint : public ItemPacketBaseConstraint {
 public:
  ItemPacketHierConstraint() : ItemPacketBaseConstraint(), dest_min(this), dest_max(this) {
    constraint((dest_addr() > dest_min()) && (dest_addr() < dest_max()));
    constraint(((src_addr() > (dest_addr() + 0x100000)) && (src_addr() < (dest_addr() + 0x200000))) ||
               ((src_addr() < (dest_addr() - 0x10000)) && (src_addr() > (dest_addr() - 0xfffff))));
  }
  randv<unsigned> dest_min;
  randv<unsigned> dest_max;

  friend ostream& operator<<(ostream& os, ItemPacketHierConstraint& it) {
    os << it.src_addr << " " << it.dest_addr << " " << it.msg_length << " \n";
    for (unsigned i = 0; i < it.msg.size(); ++i) {
      os << it.msg[i];
    }
    return os;
  }
};

BOOST_AUTO_TEST_CASE(PacketHierConstraint) {
  ItemPacketHierConstraint it;
  for (unsigned i = 0; i < 11; ++i) {
    BOOST_REQUIRE(it.next());
    std::cout << it << std::endl;
  }
}
BOOST_AUTO_TEST_CASE(no_conflict) {
  randv<unsigned short> a(0);
  randv<unsigned short> b(0);
  randv<unsigned short> c(0);
  Generator gen;
  gen("a", a() != b())("b", b() != c());

  BOOST_REQUIRE(gen.next());
  std::vector<std::vector<std::string> > result = gen.analyseContradiction();
  BOOST_REQUIRE_EQUAL(result.size(), 0);
}

BOOST_AUTO_TEST_CASE(one_conflict1) {
  randv<bool> a(0);
  randv<bool> b(0);
  randv<bool> c(0);
  Generator gen;
  gen("a", a() != b())("b", b() != c())("c", a() != c());

  BOOST_REQUIRE(!gen.next());
  std::vector<std::vector<std::string> > result = gen.analyseContradiction();
  sort_results(result);
  print_vec_vec(std::cout, result);
  std::cout << std::endl;

  BOOST_REQUIRE_EQUAL(result.size(), 1);
  std::vector<std::string> expected = list_of("a")("b")("c");
  std::sort(result[0].begin(), result[0].end());
  BOOST_REQUIRE_EQUAL_COLLECTIONS(result[0].begin(), result[0].end(), expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(one_conflict2) {
  randv<unsigned short> a(0);
  randv<unsigned short> b(0);
  Generator gen;
  gen("a", a() == 3)("b", a() > 4)("c", b() == 0);

  BOOST_REQUIRE(!gen.next());
  std::vector<std::vector<std::string> > result = gen.analyseContradiction();
  sort_results(result);
  print_vec_vec(std::cout, result);
  std::cout << std::endl;

  BOOST_REQUIRE_EQUAL(result.size(), 1);
  std::vector<std::string> expected = list_of("a")("b");
  std::sort(result[0].begin(), result[0].end());
  BOOST_REQUIRE_EQUAL_COLLECTIONS(result[0].begin(), result[0].end(), expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(two_conflicts1) {
  randv<unsigned short> a(0);
  randv<unsigned short> b(0);
  randv<unsigned short> c(0);
  Generator gen;
  gen("a", a() == 1)("b", a() > 5)("c", b() == 0)("d", c() == b() && c() != 0);

  BOOST_REQUIRE(!gen.next());
  std::vector<std::vector<std::string> > result = gen.analyseContradiction();
  sort_results(result);
  print_vec_vec(std::cout, result);
  std::cout << std::endl;

  {
    std::vector<std::string> expected = list_of("a")("b");
    BOOST_REQUIRE_EQUAL_COLLECTIONS(result[0].begin(), result[0].end(), expected.begin(), expected.end());
  }

  {
    std::vector<std::string> expected = list_of("c")("d");
    BOOST_REQUIRE_EQUAL_COLLECTIONS(result[1].begin(), result[1].end(), expected.begin(), expected.end());
  }

  BOOST_REQUIRE_EQUAL(result.size(), 2);
  BOOST_REQUIRE_EQUAL(result[0].size(), 2);
  BOOST_REQUIRE_EQUAL(result[1].size(), 2);
}

BOOST_AUTO_TEST_CASE(two_conflicts2) {
  randv<bool> a(0);
  randv<bool> b(0);
  randv<bool> c(0);
  randv<unsigned short> d(0);
  randv<unsigned short> e(0);

  Generator gen;
  gen("a", a() != b())("b", b() != c())("c", a() != c())("d", d() == 0)("e", e() == d() && e() != 0);

  BOOST_REQUIRE(!gen.next());
  std::vector<std::vector<std::string> > result = gen.analyseContradiction();
  sort_results(result);
  print_vec_vec(std::cout, result);
  std::cout << std::endl;

  {
    std::vector<std::string> expected = list_of("d")("e");
    BOOST_REQUIRE_EQUAL_COLLECTIONS(result[0].begin(), result[0].end(), expected.begin(), expected.end());
  }

  {
    std::vector<std::string> expected = list_of("a")("b")("c");
    BOOST_REQUIRE_EQUAL_COLLECTIONS(result[1].begin(), result[1].end(), expected.begin(), expected.end());
  }

  BOOST_REQUIRE_EQUAL(result.size(), 2);
  BOOST_REQUIRE_EQUAL(result[0].size(), 2);
  BOOST_REQUIRE_EQUAL(result[1].size(), 3);
}

BOOST_AUTO_TEST_CASE(two_conflicts3) {
  randv<unsigned short> a(0);
  randv<unsigned short> b(0);
  randv<unsigned short> c(0);
  randv<unsigned short> d(0);
  Generator gen;
  gen("c1", a() == 2)("c2", a() > 5)("c3", b() == d() * 2)("c4", c() % a() == 1)("c5", c() == b());

  BOOST_REQUIRE(!gen.next());
  std::vector<std::vector<std::string> > result = gen.analyseContradiction();
  sort_results(result);
  print_vec_vec(std::cout, result);
  std::cout << std::endl;

  BOOST_REQUIRE_EQUAL(result.size(), 2);
  BOOST_REQUIRE_EQUAL(result[0].size(), 2);
  BOOST_REQUIRE_EQUAL(result[1].size(), 4);

  {
    std::vector<std::string> expected = list_of("c1")("c2");
    BOOST_REQUIRE_EQUAL_COLLECTIONS(result[0].begin(), result[0].end(), expected.begin(), expected.end());
  }

  {
    std::vector<std::string> expected = list_of("c1")("c3")("c4")("c5");
    BOOST_REQUIRE_EQUAL_COLLECTIONS(result[1].begin(), result[1].end(), expected.begin(), expected.end());
  }
}

BOOST_AUTO_TEST_CASE(conflict_with_softs_t1) {
  randv<int> a(0);
  randv<int> b(0);
  randv<int> c(0);

  Generator gen;
  gen("c1", a() < 10)("c2", b() >= 10)("c3", c() != a() && c() != b());
  gen.soft("c4", c() == a());

  BOOST_REQUIRE(gen.next());

  std::vector<std::string> result = gen.getInactiveSofts();
  std::sort(result.begin(), result.end());
  print_vec(std::cout, result);
  std::cout << std::endl;

  BOOST_REQUIRE_EQUAL(result.size(), 1);

  std::vector<std::string> expected = list_of("c4");
  BOOST_REQUIRE_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(conflict_with_softs_t2) {
  randv<unsigned int> a(0), b(0), c(0);

  Generator gen;
  gen("h1", a() >= 100)("h2", b() > 50 && b() <= 100)("h3", c() % a() == 0);
  gen.soft("s1", a() == b());
  gen.soft("s2", a() == c());
  gen.soft("s3", b() != c());
  gen.soft("s4", c() > 0 && c() < 100);

  BOOST_REQUIRE(gen.next());

  std::vector<std::string> result = gen.getInactiveSofts();
  std::sort(result.begin(), result.end());
  print_vec(std::cout, result);
  std::cout << std::endl;

  std::vector<std::string> expected = list_of("s1")("s4");
  BOOST_REQUIRE_EQUAL_COLLECTIONS(result.begin(), result.end(), expected.begin(), expected.end());
}

class Item2 : public rand_obj {
 public:
  Item2() : rand_obj(), a(this), b(this) {
    constraint("d1", dist(a(), distribution<unsigned>::create(range<unsigned>(10, 20))));
    constraint("d2", dist(a(), distribution<unsigned>::create(range<unsigned>(30, 50))));
    constraint(b() == a() * 5);
  }
  randv<unsigned> a;
  randv<unsigned> b;

  friend ostream& operator<<(ostream& os, const Item2& it) {
    os << it.a << " " << it.b;
    return os;
  }
};

BOOST_AUTO_TEST_CASE(multi_distributions) {
  Item2 it;

  BOOST_REQUIRE(!it.next());

  BOOST_REQUIRE(it.disable_constraint("d1"));
  BOOST_REQUIRE(it.next());
  BOOST_REQUIRE(30 <= it.a && it.a <= 50);

  BOOST_REQUIRE(it.enable_constraint("d1"));
  BOOST_REQUIRE(it.disable_constraint("d2"));
  BOOST_REQUIRE(it.next());
  BOOST_REQUIRE(10 <= it.a && it.a <= 20);
}

class Item3 : public rand_obj {
 public:
  Item3(rand_obj* parent) : rand_obj(parent), a(this), b(this) {
    constraint(a() == b());
    constraint(a() <= 5);
    constraint("c1", b() != 5);
  }
  randv<unsigned> a;
  randv<unsigned> b;
};

class Item4 : public rand_obj {
 public:
  Item4(rand_obj* parent = 0) : rand_obj(parent), item(this), c(this) {
    constraint(c() >= 5);
    constraint("c2", item.a() == c());
  }
  Item3 item;
  randv<unsigned> c;
};

BOOST_AUTO_TEST_CASE(hier_constr_t) {
  Item4 it;

  BOOST_REQUIRE(!it.next());

  BOOST_REQUIRE(it.disable_constraint("c2"));
  BOOST_REQUIRE(it.next());
  BOOST_REQUIRE(it.c >= 5);
  BOOST_REQUIRE(it.item.a <= 5);
  BOOST_REQUIRE(it.item.a == it.item.b);
  BOOST_REQUIRE(it.item.b != 5);

  BOOST_REQUIRE(it.enable_constraint("c2"));
  BOOST_REQUIRE(it.item.disable_constraint("c1"));
  BOOST_REQUIRE(it.next());
  BOOST_REQUIRE(it.c == 5);
  BOOST_REQUIRE(it.item.a == 5);
  BOOST_REQUIRE(it.item.b == 5);

  BOOST_REQUIRE(it.item.enable_constraint("c1"));
  BOOST_REQUIRE(!it.next());
}

BOOST_AUTO_TEST_CASE(direct_metaSMTVisitor_last_wins) {
  struct BackendGuard {
    ~BackendGuard() { set_solver_backend("Z3"); }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    // 1. analyseSofts(): last-added soft wins (index 1 wins over index 0)
    {
      std::unique_ptr<metaSMTVisitor> visitor(FactoryMetaSMT::getNewInstance());
      NodePtr x(new VariableExpr(new_var_id(), 32, false));
      NodePtr s1(new EqualOpr(x, new Constant(10u, 32, false)));
      NodePtr s2(new EqualOpr(x, new Constant(20u, 32, false)));
      visitor->makeSoftAssertion(*s1);
      visitor->makeSoftAssertion(*s2);

      std::vector<unsigned int> rejected = visitor->analyseSofts();
      BOOST_REQUIRE_EQUAL(rejected.size(), 1u);
      BOOST_REQUIRE_EQUAL(rejected[0], 0u);
      BOOST_REQUIRE(visitor->solve(true));

      unsigned val = 0;
      AssignResultToRef<unsigned> ref(&val);
      BOOST_REQUIRE(visitor->read(*x, ref));
      BOOST_REQUIRE_EQUAL(val, 20u);
    }

    // 2. Dynamic makeSoftAssertion + makeAssumption + solve(false): last-wins & fallback
    {
      std::unique_ptr<metaSMTVisitor> visitor(FactoryMetaSMT::getNewInstance());
      NodePtr x(new VariableExpr(new_var_id(), 32, false));
      NodePtr y(new VariableExpr(new_var_id(), 32, false));
      NodePtr hard(new LessOpr(x, new Constant(100u, 32, false)));
      NodePtr s1(new EqualOpr(x, new Constant(10u, 32, false)));
      NodePtr s2(new EqualOpr(x, new Constant(20u, 32, false)));
      NodePtr s3(new EqualOpr(y, new Constant(7u, 32, false)));

      visitor->makeAssertion(*hard);
      visitor->makeSoftAssertion(*s1);
      visitor->makeSoftAssertion(*s2);
      visitor->makeSoftAssertion(*s3);

      // Block s2 transiently via assumption -> s1 (x == 10) and s3 (y == 7) hold
      NodePtr block_s2(new NotEqualOpr(x, new Constant(20u, 32, false)));
      visitor->makeAssumption(*block_s2);
      BOOST_REQUIRE(visitor->solve(false));

      unsigned xv = 0, yv = 0;
      AssignResultToRef<unsigned> xref(&xv), yref(&yv);
      BOOST_REQUIRE(visitor->read(*x, xref));
      BOOST_REQUIRE(visitor->read(*y, yref));
      BOOST_REQUIRE_EQUAL(xv, 10u);
      BOOST_REQUIRE_EQUAL(yv, 7u);

      // Next solve without blocking assumption -> s2 (x == 20) re-activates and wins over s1
      BOOST_REQUIRE(visitor->solve(false));
      BOOST_REQUIRE(visitor->read(*x, xref));
      BOOST_REQUIRE(visitor->read(*y, yref));
      BOOST_REQUIRE_EQUAL(xv, 20u);
      BOOST_REQUIRE_EQUAL(yv, 7u);
    }
  }
}

BOOST_AUTO_TEST_CASE(variable_default_solver_dynamic_reference_soft) {
  struct BackendGuard {
    ~BackendGuard() { set_solver_backend("Z3"); }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    Variable<unsigned> a, b;
    unsigned ext = 5;
    Generator gen(a < 100u);
    gen.soft("s1", a == 10u);
    gen.soft("s2", a == 20u);
    gen.soft("s3", b == reference(ext));
    gen.soft("s4", a == reference(ext));

    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_EQUAL(gen[a], 5u);
    BOOST_REQUIRE_EQUAL(gen[b], 5u);
    {
      std::vector<std::string> inact = gen.getInactiveSofts();
      std::sort(inact.begin(), inact.end());
      std::vector<std::string> expected = list_of("s1")("s2");
      BOOST_REQUIRE_EQUAL_COLLECTIONS(inact.begin(), inact.end(), expected.begin(), expected.end());
    }

    // Mutate reference without rebuilding: s4 conflicts with a < 100u -> drops, s2 wins over s1, s3 stays active
    ext = 200;
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_EQUAL(gen[a], 20u);
    BOOST_REQUIRE_EQUAL(gen[b], 200u);
    {
      std::vector<std::string> inact = gen.getInactiveSofts();
      std::sort(inact.begin(), inact.end());
      std::vector<std::string> expected = list_of("s1")("s4");
      BOOST_REQUIRE_EQUAL_COLLECTIONS(inact.begin(), inact.end(), expected.begin(), expected.end());
    }

    // Mutate reference back to compatible value: s4 re-activates!
    ext = 7;
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_EQUAL(gen[a], 7u);
    BOOST_REQUIRE_EQUAL(gen[b], 7u);
    {
      std::vector<std::string> inact = gen.getInactiveSofts();
      std::sort(inact.begin(), inact.end());
      std::vector<std::string> expected = list_of("s1")("s2");
      BOOST_REQUIRE_EQUAL_COLLECTIONS(inact.begin(), inact.end(), expected.begin(), expected.end());
    }
  }
}

BOOST_AUTO_TEST_CASE(bypass_constraint_analysis_with_softs) {
  struct Guard {
    bool prev_bypass;
    Guard() : prev_bypass(VariableDefaultSolver::bypass_constraint_analysis) {
      VariableDefaultSolver::bypass_constraint_analysis = true;
    }
    ~Guard() {
      VariableDefaultSolver::bypass_constraint_analysis = prev_bypass;
      set_solver_backend("Z3");
    }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    Variable<unsigned> a, b;
    unsigned ext = 9;
    Generator gen(a < 10u);
    gen.soft("s1", a == 5u);
    gen.soft("s2", a == 7u);
    gen.soft("s3", a == reference(ext));
    gen.soft("s4", b == 42u);

    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_EQUAL(gen[a], 9u);
    BOOST_REQUIRE_EQUAL(gen[b], 42u);

    ext = 50;
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_EQUAL(gen[a], 7u);
    BOOST_REQUIRE_EQUAL(gen[b], 42u);

    ext = 3;
    BOOST_REQUIRE(gen.next());
    BOOST_REQUIRE_EQUAL(gen[a], 3u);
    BOOST_REQUIRE_EQUAL(gen[b], 42u);
  }
}

BOOST_AUTO_TEST_CASE(randc_conflicting_softs_last_wins) {
  struct BackendGuard {
    ~BackendGuard() { set_solver_backend("Z3"); }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    Variable<unsigned char> x;
    Variable<unsigned> y;
    Generator gen(x < 4u);
    gen.soft("s1", y == 10u);
    gen.soft("s2", y == 20u);
    gen.soft("s3", x == 2u);
    gen.randc(x);

    std::set<unsigned> seen;
    for (int i = 0; i < 4; ++i) {
      BOOST_REQUIRE(gen.next());
      unsigned xv = gen[x];
      seen.insert(xv);
      BOOST_REQUIRE_EQUAL(gen[y], 20u);

      std::vector<std::string> inact = gen.getInactiveSofts();
      std::sort(inact.begin(), inact.end());
      if (xv == 2u) {
        std::vector<std::string> expected = list_of("s1");
        BOOST_REQUIRE_EQUAL_COLLECTIONS(inact.begin(), inact.end(), expected.begin(), expected.end());
      } else {
        std::vector<std::string> expected = list_of("s1")("s3");
        BOOST_REQUIRE_EQUAL_COLLECTIONS(inact.begin(), inact.end(), expected.begin(), expected.end());
      }
    }
    BOOST_REQUIRE_EQUAL(seen.size(), 4u);
  }
}

BOOST_AUTO_TEST_CASE(solve_before_with_conflicting_softs) {
  struct BackendGuard {
    ~BackendGuard() { set_solver_backend("Z3"); }
  } guard;

  for (const char* backend : {"Z3", "Cudd"}) {
    if (std::string(backend) == "Cudd" && !FactoryMetaSMT::isDefined(CUDD)) continue;
    set_solver_backend(backend);

    {
      Variable<bool> s;
      Variable<unsigned> d;
      Generator gen(if_then(s, d == 0u));
      gen.solve_before(s, d);
      gen.soft("s1", d == 1u);
      gen.soft("s2", s == true);

      BOOST_REQUIRE(gen.next());
      BOOST_REQUIRE_EQUAL(gen[s], true);
      BOOST_REQUIRE_EQUAL(gen[d], 0u);
      std::vector<std::string> inact = gen.getInactiveSofts();
      std::vector<std::string> expected = list_of("s1");
      BOOST_REQUIRE_EQUAL_COLLECTIONS(inact.begin(), inact.end(), expected.begin(), expected.end());
    }

    {
      unsigned ext = 1;
      Variable<unsigned> x, y;
      Generator gen(x < 10u && y == x + 1u);
      gen.solve_before(x, y);
      gen.soft("s1", x == 3u);
      gen.soft("s2", x == reference(ext));

      BOOST_REQUIRE(gen.next());
      BOOST_REQUIRE_EQUAL(gen[x], 1u);
      BOOST_REQUIRE_EQUAL(gen[y], 2u);
      BOOST_REQUIRE(gen.getInactiveSofts() == std::vector<std::string>({"s1"}));

      ext = 20;
      BOOST_REQUIRE(gen.next());
      BOOST_REQUIRE_EQUAL(gen[x], 3u);
      BOOST_REQUIRE_EQUAL(gen[y], 4u);
      BOOST_REQUIRE(gen.getInactiveSofts() == std::vector<std::string>({"s2"}));

      ext = 5;
      BOOST_REQUIRE(gen.next());
      BOOST_REQUIRE_EQUAL(gen[x], 5u);
      BOOST_REQUIRE_EQUAL(gen[y], 6u);
      BOOST_REQUIRE(gen.getInactiveSofts() == std::vector<std::string>({"s1"}));
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()  // Context

//  vim: ft=cpp:ts=2:sw=2:expandtab
