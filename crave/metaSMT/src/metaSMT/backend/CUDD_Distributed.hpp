/****************************************************************************************
//	MIT License
//***************************************************************************************
//	Copyright (c) 2011-2016 University of Bremen, Germany.
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
****************************************************************************************/

#include "CUDD_Context.hpp"
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>

#include <boost/format.hpp>
#include <boost/random/uniform_01.hpp>
#include <boost/random/mersenne_twister.hpp>

namespace metaSMT {
  namespace solver {

class CUDD_Distributed : public CUDD_Context
{
			
public: 

	explicit CUDD_Distributed (unsigned cache_slots = CUDD_CACHE_SLOTS)
  : CUDD_Context(cache_slots)
  , previous(_manager.bddZero())
  {
	  reset();
	}

	virtual ~CUDD_Distributed() {}

	bool solve() {
 	 	BDD complete =_assertions & _assumptions;  
    if ( previous != complete) {
      //std::cout << "resetting distribution cache" << std::endl;
			reset();
      previous = complete;
    } else {
      //std::cout << "keeping distribution cache" << std::endl;
    }
	 	bool  ret =  complete != _manager.bddZero();
         	_assumptions = _manager.bddOne();
        	//printDD(complete.getNode(),"BDD.dot");
	        if(ret) {
			store_solution( complete.getNode() );
		}
		return ret;
	}

protected:
	// random number in [0, 1) to choose a branch, can be overridden to use another generator
	virtual double uniform01() {
		return boost::uniform_01<double>()(gen);
	}

private:
	// For each node the fraction of the assignments to the variables below it which lead to the
	// 0 and to the 1 terminal, as logarithm to base 2. Fractions instead of the numbers of
	// assignments are used, because these numbers do not fit into an integer for more than
	// 64 variables (a fraction also does not depend on the levels skipped by an edge), and
	// logarithms keep small fractions from underflowing.
	std::map<DdNode *, double> map_0;
	std::map<DdNode *, double> map_1;

	static double log2_zero() { return -std::numeric_limits<double>::infinity(); }

	// log2(2^a + 2^b)
	static double log2_add(double a, double b) {
		if (a < b) std::swap(a, b);
		if (b == log2_zero()) return a;
		return a + std::log2(1.0 + std::exp2(b - a));
	}

	void reset() {

		map_0.clear();
		map_1.clear();

		map_0 [_manager.bddOne().getNode()] = log2_zero();
		map_1 [_manager.bddOne().getNode()] = 0;
	}

	double fraction_0 (DdNode *root) {
		if( Cudd_IsComplement(root)) {
			return fraction_1(Cudd_Regular(root));
		}
		std::map<DdNode *, double>::iterator iter = map_0.find(root); 
	 	if(iter != map_0.end()) {
			return iter->second;
		} else {
			double ret = log2_add( fraction_0( Cudd_T(root) ), fraction_0( Cudd_E(root) ) ) - 1;
		   	map_0.insert( std::make_pair(root,ret) );
		   	return ret;
		}			
	}

	double fraction_1 (DdNode *root) {
		if( Cudd_IsComplement(root)) {
			return fraction_0(Cudd_Regular(root));
		}
		std::map<DdNode *, double>::iterator iter = map_1.find(root); 
		if(iter != map_1.end()) {
			return iter->second;
		} else {
			double ret = log2_add( fraction_1( Cudd_T(root) ), fraction_1( Cudd_E(root) ) ) - 1;
			map_1.insert( std::make_pair(root,ret) );
			return ret;
		}
	}


	void store_solution ( DdNode * root) {

	   // clear solution
            unsigned size = _manager.ReadSize();
	    std::vector< boost::logic::tribool > (size, boost::logic::indeterminate).swap(_solution);

	    bool negated = false;
	   
            while( ! Cudd_IsConstant(root) ) {
         	
		negated ^= Cudd_IsComplement( root );
		root = Cudd_Regular(root);
		DdNode* child_t = Cudd_T(root);
		DdNode* child_e = Cudd_E(root);
		double frac_t = negated ? fraction_0(child_t) : fraction_1(child_t);
		double frac_e = negated ? fraction_0(child_e) : fraction_1(child_e);

		// the probability of the then branch is its share of the solutions
		double prob_t = std::exp2( frac_t - log2_add(frac_t, frac_e) );

		if( uniform01() < prob_t )
		{			
			_solution[Cudd_NodeReadIndex(root)] = true;
			root = child_t;
		}	
		else
		{
			_solution[Cudd_NodeReadIndex(root)] = false;
			root = child_e;
		}
	    }
	}
	
  void printDD(DdNode *root, std::string fileName) {
    FILE *file = fopen(fileName.c_str(),"w");

    char **iname = (char**) malloc(sizeof(char *)*_manager.ReadSize());
    for(unsigned i = 0; i < (unsigned)_manager.ReadSize(); ++i)
    {
      char *name = (char*) malloc(256);
      sprintf(name,"v%d", _manager.bddVar(i).NodeReadIndex());
      iname [i] = name;
    }
    //extern int Cudd_DumpDot (DdManager *dd, int n, DdNode **f, char **inames, char **onames, FILE *fp);
    Cudd_DumpDot (_manager.getManager(),1,&root,iname, NULL,file);
    fclose(file);	
  }
    protected:
      boost::mt19937 gen;

    private:
      BDD previous;
    }; // class CUDD_Distribuded

  }//  namespace solver
} //namespace metaSMT
