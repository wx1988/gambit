//#
//# FILE: efgdbl.cc -- Instantiation of double-precision extensive forms
//#
//# $Id$
//#

#include "rational.h"
#include "efg.h"
#include "glist.h"
#include "glistit.h"

#ifdef __GNUG__
#define TEMPLATE template
#elif defined __BORLANDC__
class BehavProfile<gRational>;
class Efg<gRational>;
class gList<Node *>;
class gListIter<Node *>;

#define TEMPLATE
#pragma option -Jgd
#endif   // __GNUG__, __BORLANDC__

#include "efg.imp"
TEMPLATE class Efg<double>;
DataType Efg<double>::Type(void) const    { return DOUBLE; }

TEMPLATE class TypedNode<double>;
TEMPLATE class ChanceInfoset<double>;
TEMPLATE class OutcomeVector<double>;
TEMPLATE class BehavProfile<double>;
TEMPLATE gOutput &operator<<(gOutput &, const BehavProfile<double> &);
//TEMPLATE bool operator==(const gArray<double> &, const gArray<double> &);
//TEMPLATE bool operator!=(const gArray<double> &, const gArray<double> &);

#include "readefg.imp"

TEMPLATE class EfgFile<double>;
TEMPLATE int ReadEfgFile(gInput &, Efg<double> *&);

#include "glist.imp"

TEMPLATE class gList<BehavProfile<double> >;
TEMPLATE class gNode<BehavProfile<double> >;



