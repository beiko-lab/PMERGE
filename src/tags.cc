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

#include "tags.h"

Tag::Tag()  { 
    this->id         = 0;
    this->seq        = "";
}

Tag::~Tag() { 

}
int Tag::add_dist(const int id, const int dist) {
    //
    // Store the ID and distance as a pair, ID in the first position,
    // dist in the second.
    //
    pair<int, int> p(id, dist);
    this->dist.push_back(p);

    return 0;
}

