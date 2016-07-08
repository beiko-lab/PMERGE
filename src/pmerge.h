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

#ifndef __PMERGE_H__
#define __PMERGE_H__

#ifdef _OPENMP
#include <omp.h>    // OpenMP library
#endif
#include <getopt.h> // Process command-line options
#include <dirent.h> // Open/Read contents of a directory
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <utility>
using std::pair;
using std::make_pair;
#include <string>
using std::string;
#include <iostream>
#include <fstream>
using std::ifstream;
using std::ofstream;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;
#include <iomanip>
using std::setw;
using std::setprecision;
using std::fixed;
#include <sstream>
using std::stringstream;
#include <vector>
using std::vector;
#include <map>
using std::map;
#include <set>
using std::set;
#include <list>
#include <algorithm>        
#include <cstdlib> 

#include "PopMap.h"
#include "PopSum.h"
#include "catalog_utils.h"
#include "sql_utilities.h"
#include "utils.h"
#include "tags.h"


void    help( void );
void    version( void );
int     parse_command_line(int, char**);
int     build_file_list(vector<pair<int, string> > &, map<int, pair<int, int> > &, map<int, vector<int> > &);
int     load_marker_list(string, set<int> &);
int     load_marker_column_list(string, map<int, set<int> > &);
int     apply_locus_constraints(map<int, CSLocus *> &, PopMap<CSLocus> *, map<int, pair<int, int> > &);
int     prune_polymorphic_sites(map<int, CSLocus *> &, PopMap<CSLocus> *, PopSum<CSLocus> *, map<int, pair<int, int> > &, map<int, set<int> > &, set<int> &, ofstream &, string);
int     cluster_filter(map<int, CSLocus *> &,set<int> &,ofstream &, string);
bool    order_unordered_loci(map<int, CSLocus *> &);
bool    compare_pop_map(pair<int, string>, pair<int, string>);



int
init_log(ofstream &fh, int argc, char **argv)
{
    //
    // Obtain the current date.
    //
    time_t     rawtime;
    struct tm *timeinfo;
    char       date[32];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(date, 32, "%F %T", timeinfo);

    //
    // Write the command line that was executed.
    //
    for (int i = 0; i < argc; i++) {
	fh << argv[i]; 
	if (i < argc - 1) fh << " ";
    }
    fh << "\n" << argv[0] << " version " << VERSION << " executed " << date << "\n\n";

    return 0;
}


#endif // __PMERGE_H__
