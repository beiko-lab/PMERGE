// -*-mode:c++; c-style:k&r; c-basic-offset:4;-*-
//
// Copyright 2013-2015, Julian Catchen <jcatchen@illinois.edu>
//
// This file is part of Stacks.
//
// Stacks is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Stacks is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Stacks.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef __CATALOG_UTILS_H__
#define __CATALOG_UTILS_H__

#include <map>
using std::map;
#include <set>
using std::set;


#include "PopMap.h"

int check_whitelist_integrity(map<int, CSLocus *> &, map<int, set<int> > &);
int reduce_catalog(map<int, CSLocus *> &, set<int> &, set<int> &);
int reduce_catalog(map<int, CSLocus *> &, map<int, set<int> > &, set<int> &);
int reduce_catalog_snps(map<int, CSLocus *> &, map<int, set<int> > &, PopMap<CSLocus> *);

#endif // __CATALOG_UTILS_H__
