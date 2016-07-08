// -*-mode:c++; c-style:k&r; c-basic-offset:4;-*-
//
// Copyright 2016, Praveen Nadukkalam Ravindran <pravindran@dal.ca>
//
// This file is part of Pmerge.
//
// Pmerge is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Pmerge is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Stacks.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef __TAGS_H__
#define __TAGS_H__
#include <string.h>
#include <string>
using std::string;
#include <vector>
using std::vector;
#include <utility>
using std::pair;
using std::make_pair;
#include<iostream>
using std::cerr;
#include "tags.h"

class Tag{
public:
string seq;
int id;
int len;
vector<pair<int, int> > dist;
Tag();
~Tag();
int  add_dist(const int id, const int dist);
};

#endif
