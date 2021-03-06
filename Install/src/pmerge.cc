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

//
// pmerge -- filter for Stacks program
//

#include "pmerge.h"


// Global variables to hold command-line options.
int       num_threads =  1;
int       batch_id    = -1;
string    in_path;
string    out_path;
string    pmap_path;
string    wl_path;
double    sample_limit        = 0.0;
int       population_limit    = 1;
bool      loci_ordered        = false;
bool      log_fst_comp        = false;
bool      verbose             = true;
bool      filter_lnl          = false;
double    lnl_limit           = 0.0;
int       min_stack_depth     = 0;
double    merge_prune_lim     = 1.0;
double    merge_minor_freq    = 0.0;
double    minor_allele_freq   = 0.0;
double    p_value_cutoff      = 0.05;
double    max_obs_het         = 1.0;
double    cluster_similarity  = 0.0;

map<int, string>          pop_key, grp_key;
map<int, pair<int, int> > pop_indexes;
map<int, vector<int> >    grp_members;
map<int, int>   psv_counter;
set<int> blacklist;
map<int, set<int> > whitelist;

int dist(Tag *tag_1,Tag *tag_2, int distance) {
    int   dist  = 0;
    const char *p     = tag_1->seq.c_str();
    const char *q     = tag_2->seq.c_str();
    const char *p_end = p + tag_1->len;
    const char *q_end = q + tag_1->len;


   if (tag_1->len != tag_2->len) {
        if (tag_1->len < tag_2->len)
            dist += tag_2->len - tag_1->len;
        else if (tag_1->len > tag_2->len)
            dist += tag_1->len - tag_2->len;
    }

    //
    // Count the number of characters that are different
    // between the two sequences.
    //
    while (p < p_end && q < q_end) {
	dist += (*p == *q) ? 0 : 1;
	p++; 
	q++;
	if (dist > distance)
		return -1;
    }

    return dist;
}

int main (int argc, char* argv[]) {

    //initialize_renz(renz, renz_cnt, renz_len);
    //initialize_renz_olap(renz_olap);

    parse_command_line(argc, argv);

    cerr
	<< "Percent samples limit per population: " << sample_limit << "\n"
	<< "Locus Population limit: " << population_limit << "\n"
	<< "Minimum stack depth: " << min_stack_depth << "\n"
	<< "Log liklihood filtering: " << (filter_lnl == true ? "on"  : "off") << "; threshold: " << lnl_limit << "\n"
	<< "Minor allele frequency cutoff: " << minor_allele_freq << "\n"
        << "Maximum observed heterozygosity cutoff: " << max_obs_het << "\n"
        << "Minimum percentage of similarity between loci to cluster: " << cluster_similarity << "\n";

    //
    // Set the number of OpenMP parallel threads to execute.
    //
    #ifdef _OPENMP
    omp_set_num_threads(num_threads);
    #endif
   
    //
    // Seed the random number generator
    //
    srandom(time(NULL));

    vector<pair<int, string> > files;
    if (!build_file_list(files, pop_indexes, grp_members))
	exit(1);

    
    //
    // Open the log file.
    //
    stringstream log, wl;
    log << "batch_" << batch_id << ".pmerge.log";
    string log_path = in_path + log.str();
    ofstream log_fh(log_path.c_str(), ofstream::out);
    if (log_fh.fail()) {
        cerr << "Error opening log file '" << log_path << "'\n";
	exit(1);
    }
    init_log(log_fh, argc, argv);
    
     //
     // open Whitelist file
     //
     wl << "batch_" << batch_id << ".WL";
    string wl_path = in_path + wl.str();
    
    //
    // Load the catalog
    //
    stringstream catalog_file;
    map<int, CSLocus *> catalog;
    bool compressed = false;
    int  res;
    catalog_file << in_path << "batch_" << batch_id << ".catalog";
    if ((res = load_loci(catalog_file.str(), catalog, false, false, compressed)) == 0) {
    	cerr << "Unable to load the catalog '" << catalog_file.str() << "'\n";
     	return 0;
    }

    //
    // Check the whitelist.
    //
    check_whitelist_integrity(catalog, whitelist);

    //
    // Implement the black/white list
    //
    reduce_catalog(catalog, whitelist, blacklist);

    //
    // If the catalog is not reference aligned, assign an arbitrary ordering to catalog loci.
    //
    loci_ordered = order_unordered_loci(catalog);

    //
    // Load matches to the catalog
    //
    vector<vector<CatMatch *> > catalog_matches;
    map<int, string>            samples;
    vector<int>                 sample_ids;
     map<int, CSLocus *>::iterator it;
     CSLocus *loc;
    for (int i = 0; i < (int) files.size(); i++) {
	vector<CatMatch *> m;
	load_catalog_matches(in_path + files[i].second, m);

	if (m.size() == 0) {
	    cerr << "Warning: unable to find any matches in file '" << files[i].second << "', excluding this sample from population analysis.\n";
	    //
	    // This case is generated by an existing, but empty file.
	    // Remove this sample from the population index which was built from 
	    // existing files, but we couldn't yet check for empty files.
	    //
	    map<int, pair<int, int> >::iterator pit;
	    for (pit = pop_indexes.begin(); pit != pop_indexes.end(); pit++)
		if (i >= pit->second.first && i <= pit->second.second) {
		    pit->second.second--;
		    pit++;
		    while (pit != pop_indexes.end()) {
			pit->second.first--;
			pit->second.second--;
			pit++;
		    }
		    break;
		}

	    continue;
	}

	catalog_matches.push_back(m);
	if (samples.count(m[0]->sample_id) == 0) {
	    samples[m[0]->sample_id] = files[i].second;
	    sample_ids.push_back(m[0]->sample_id);
	} else {
	    cerr << "Fatal error: sample ID " << m[0]->sample_id << " occurs twice in this data set, likely the pipeline was run incorrectly.\n";
	    exit(0);
	}
    }

    //
    // Create the population map
    // 
    cerr << "Populating observed haplotypes for " << sample_ids.size() << " samples, " << catalog.size() << " loci.\n";
    PopMap<CSLocus> *pmap = new PopMap<CSLocus>(sample_ids.size(), catalog.size());
    pmap->populate(sample_ids, catalog, catalog_matches);


    apply_locus_constraints(catalog, pmap, pop_indexes);

   
    log_fh << "# Distribution of population loci after applying locus constraints.\n";
    //log_haplotype_cnts(catalog, log_fh);

    cerr << "Loading model outputs for " << sample_ids.size() << " samples, " << catalog.size() << " loci.\n";
    //map<int, CSLocus *>::iterator it;
    map<int, ModRes *>::iterator mit;
    Datum   *d;
    //CSLocus *loc;

    //
    // Load the output from the SNP calling model for each individual at each locus. This
    // model output string looks like this:
    //   OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOEOOOOOOEOOOOOOOOOOOOOOOOOOOOOOOOOOOOOUOOOOUOOOOOO
    // and records model calls for each nucleotide: O (hOmozygous), E (hEterozygous), U (Unknown)
    //
    for (uint i = 0; i < sample_ids.size(); i++) {
    	map<int, ModRes *> modres;
    	load_model_results(in_path + samples[sample_ids[i]], modres);

    	if (modres.size() == 0) {
    	    cerr << "Warning: unable to find any model results in file '" << samples[sample_ids[i]] << "', excluding this sample from population analysis.\n";
    	    continue;
    	}

    	for (it = catalog.begin(); it != catalog.end(); it++) {
    	    loc = it->second;
    	    d = pmap->datum(loc->id, sample_ids[i]);

    	    if (d != NULL) {
		if (modres.count(d->id) == 0) {
		    cerr << "Fatal error: Unable to find model data for catalog locus " << loc->id 
			 << ", sample ID " << sample_ids[i] << ", sample locus " << d->id 
			 << "; likely IDs were mismatched when running pipeline.\n";
		    exit(0);
		}
		d->len   = strlen(modres[d->id]->model);
    		d->model = new char[d->len + 1];
    		strcpy(d->model, modres[d->id]->model);
    	    }
    	}

    	for (mit = modres.begin(); mit != modres.end(); mit++)
    	    delete mit->second;
    	modres.clear();
    }

    uint pop_id, start_index, end_index;
    map<int, pair<int, int> >::iterator pit;

    PopSum<CSLocus> *psum = new PopSum<CSLocus>(pmap->loci_cnt(), pop_indexes.size());
    psum->initialize(pmap);
    
    for (pit = pop_indexes.begin(); pit != pop_indexes.end(); pit++) {
    	start_index = pit->second.first;
    	end_index   = pit->second.second;
    	pop_id      = pit->first;
    	cerr << "Generating nucleotide-level summary statistics for population '" << pop_key[pop_id] << "'\n";
    	psum->add_population(catalog, pmap, pop_id, start_index, end_index, verbose, log_fh);
    }

    cerr << "Tallying loci across populations...";
    psum->tally(catalog);
    cerr << "done.\n";

    //
    // We have removed loci that were below the -r and -p thresholds. Now we need to
    // identify individual SNPs that are below the -r threshold or the minor allele
    // frequency threshold (-a). In these cases we will remove the SNP, but keep the locus.
    //
    blacklist.clear();

   int pruned_snps = prune_polymorphic_sites(catalog, pmap, psum, pop_indexes, whitelist, blacklist, log_fh,wl_path);
   cerr << "Pruned " << pruned_snps << " variant sites due to filter constraints.\n";

   cerr << "Removing " << blacklist.size() << " additional loci for which all variant sites were filtered...";
   set<int> empty_list;
   reduce_catalog(catalog, empty_list, blacklist);
   reduce_catalog_snps(catalog, whitelist, pmap);
   int retained = pmap->prune(blacklist);
   cerr << " retained " << retained << " loci.\n";
   
   
   
   blacklist.clear();    
   if ( cluster_similarity > 0.0)
   {
   int cluster_filtering = cluster_filter (catalog,blacklist,log_fh,wl_path); 
   cerr << "Removing " << blacklist.size() << " additional loci which are clustered within the specified threshold...";
   }
      
        
}



int
apply_locus_constraints(map<int, CSLocus *> &catalog, 
			PopMap<CSLocus> *pmap, 
			map<int, pair<int, int> > &pop_indexes)
{
    uint pop_id, start_index, end_index;
    CSLocus *loc;
    Datum  **d;

    if (sample_limit == 0 && population_limit == 0 && min_stack_depth == 0) return 0;

    map<int, CSLocus *>::iterator it;
    map<int, pair<int, int> >::iterator pit;

    uint pop_cnt   = pop_indexes.size();
    int *pop_order = new int [pop_cnt];

    // Which population each sample belongs to.
    int *samples   = new int [pmap->sample_cnt()];

    // For the current locus, how many samples in each population.
    int *pop_cnts  = new int [pop_cnt];

    // The total number of samples in each population.
    int *pop_tot   = new int [pop_cnt];

    pop_id = 0;
    for (pit = pop_indexes.begin(); pit != pop_indexes.end(); pit++) {
	start_index = pit->second.first;
	end_index   = pit->second.second;
	pop_tot[pop_id]  = 0;

	for (uint i = start_index; i <= end_index; i++) {
	    samples[i] = pop_id;
	    pop_tot[pop_id]++;
	}
	pop_order[pop_id] = pit->first;
	pop_id++;
    }

    for (uint i = 0; i < pop_cnt; i++)
	pop_cnts[i] = 0;

    double pct       = 0.0;
    bool   pop_limit = false;
    int    pops      = 0;
    int    below_stack_dep  = 0;
    uint   below_lnl_thresh = 0;
    set<int> blacklist;

    for (it = catalog.begin(); it != catalog.end(); it++) {
	loc = it->second;
	d   = pmap->locus(loc->id);

	for (int i = 0; i < pmap->sample_cnt(); i++) {
	    //
	    // Check that each sample is over the minimum stack depth for this locus.
	    //
	    if (d[i] != NULL && 
		min_stack_depth > 0 && 
		d[i]->tot_depth < min_stack_depth) {
		below_stack_dep++;
		delete d[i];
		d[i] = NULL;
		loc->hcnt--;
	    }

	    //
	    // Check that each sample is over the log likelihood threshold.
	    //
	    if (d[i] != NULL && 
		filter_lnl   && 
		d[i]->lnl < lnl_limit) {
		below_lnl_thresh++;
		delete d[i];
		d[i] = NULL;
		loc->hcnt--;
	    }
	}

	//
	// Tally up the count of samples in this population.
	//
	for (int i = 0; i < pmap->sample_cnt(); i++) {
	    if (d[i] != NULL)
		pop_cnts[samples[i]]++;
	}

	//
	// Check that the counts for each population are over sample_limit. If not, zero out 
	// the members of that population.
	//
	for (uint i = 0; i < pop_cnt; i++) {
	    pct = (double) pop_cnts[i] / (double) pop_tot[i];

	    if (pop_cnts[i] > 0 && pct < sample_limit) {
		//cerr << "Removing population " << pop_order[i] << " at locus: " << loc->id << "; below sample limit: " << pct << "\n";
		start_index = pop_indexes[pop_order[i]].first;
		end_index   = pop_indexes[pop_order[i]].second;

		for (uint j  = start_index; j <= end_index; j++) {
		    if (d[j] != NULL) {
			delete d[j];
			d[j] = NULL;
			loc->hcnt--;
		    }
		}
		pop_cnts[i] = 0;
	    }
	}

	//
	// Check that this locus is present in enough populations.
	//
	for (uint i = 0; i < pop_cnt; i++)
	    if (pop_cnts[i] > 0) pops++;
	if (pops < population_limit) {
	    //cerr << "Removing locus: " << loc->id << "; below population limit: " << pops << "\n";
	    pop_limit = true;
	}

	if (pop_limit)
	    blacklist.insert(loc->id);

	for (uint i = 0; i < pop_cnt; i++)
	    pop_cnts[i] = 0;
	pop_limit = false;
	pops      = 0;
    }

    //
    // Remove loci
    //
    if (min_stack_depth > 0) 
        {
	cerr << "Removed " << below_stack_dep << " samples from loci that are below the minimum stack depth of " << min_stack_depth << "x\n";
        
        }
    cout <<below_stack_dep << "\n";
    if (filter_lnl)
        {
	cerr << "Removed " << below_lnl_thresh << " samples from loci that are below the log likelihood threshold of " << lnl_limit << "\n";
        
        }
    cout <<below_lnl_thresh << "\n";
    cerr << "Removing " << blacklist.size() << " loci that did not pass sample/population constraints...";
    cout << blacklist.size() << "\n";
    set<int> whitelist;
    reduce_catalog(catalog, whitelist, blacklist);
    int retained = pmap->prune(blacklist);
    cerr << " retained " << retained << " loci.\n";

    delete [] pop_cnts;
    delete [] pop_tot;
    delete [] pop_order;
    delete [] samples;

    if (retained == 0)
	exit(0);

    return 0;
}



bool 
order_unordered_loci(map<int, CSLocus *> &catalog) 
{
    map<int, CSLocus *>::iterator it;
    CSLocus *loc;
    set<string> chrs;

    for (it = catalog.begin(); it != catalog.end(); it++) {
	loc = it->second;
	if (strlen(loc->loc.chr) > 0) 
	    chrs.insert(loc->loc.chr);
    }

    //
    // This data is already reference aligned.
    //
    if (chrs.size() > 0)
	return true;

    cerr << "Catalog is not reference aligned, arbitrarily ordering catalog loci.\n";

    uint bp = 1;
    for (it = catalog.begin(); it != catalog.end(); it++) {
	loc = it->second;
	loc->loc.chr = new char[3];
	strcpy(loc->loc.chr, "un");
	loc->loc.bp  = bp;

	bp += strlen(loc->con);
    }

    return false;
}


int load_marker_list(string path, set<int> &list) {
    char     line[id_len];
    ifstream fh(path.c_str(), ifstream::in);

    if (fh.fail()) {
        cerr << "Error opening white/black list file '" << path << "'\n";
	exit(1);
    }

    int   marker;
    char *p, *e;

    while (fh.good()) {
	fh.getline(line, id_len);

	if (strlen(line) == 0) continue;

	//
	// Skip commented lines.
	//
	for (p = line; isspace(*p) && *p != '\0'; p++);
	if (*p == '#') continue;

	marker = (int) strtol(line, &e, 10);

	if (*e == '\0')
	    list.insert(marker);
    }

    fh.close();

    if (list.size() == 0) {
 	cerr << "Unable to load any markers from '" << path << "'\n";
	exit(1);
    }

    return 0;
}

int load_marker_column_list(string path, map<int, set<int> > &list) {
    char     line[id_len];
    ifstream fh(path.c_str(), ifstream::in);

    if (fh.fail()) {
        cerr << "Error opening white/black list file '" << path << "'\n";
	exit(1);
    }

    vector<string> parts;
    uint  marker, col;
    char *p, *e;

    uint line_num = 1;
    while (fh.good()) {
	fh.getline(line, id_len);

	if (strlen(line) == 0) continue;

	//
	// Skip commented lines.
	//
	for (p = line; isspace(*p) && *p != '\0'; p++);
	if (*p == '#') continue;

	//
	// Parse the whitelist, we expect:
	// <marker>[<tab><snp column>]
	//
	parse_tsv(line, parts);

	if (parts.size() > 2) {
	    cerr << "Too many columns in whitelist " << path << "' at line " << line_num << "\n";
	    exit(1);

	} else if (parts.size() == 2) {
	    marker = (int) strtol(parts[0].c_str(), &e, 10);
	    if (*e != '\0') {
		cerr << "Unable to parse whitelist, '" << path << "' at line " << line_num << "\n";
		exit(1);
	    }
	    col = (int) strtol(parts[1].c_str(), &e, 10);
	    if (*e != '\0') {
		cerr << "Unable to parse whitelist, '" << path << "' at line " << line_num << "\n";
		exit(1);
	    }
	    list[marker].insert(col);

	} else {
	    marker = (int) strtol(parts[0].c_str(), &e, 10);
	    if (*e != '\0') {
		cerr << "Unable to parse whitelist, '" << path << "' at line " << line << "\n";
		exit(1);
	    }
	    list.insert(make_pair(marker, std::set<int>()));
	}

	line_num++;
    }

    fh.close();

    if (list.size() == 0) {
 	cerr << "Unable to load any markers from '" << path << "'\n";
	help();
    }

    return 0;
}

int 
build_file_list(vector<pair<int, string> > &files, 
		map<int, pair<int, int> > &pop_indexes, 
		map<int, vector<int> > &grp_members) 
{
    char             line[max_len];
    vector<string>   parts;
    map<string, int> pop_key_rev, grp_key_rev;
    set<string>      pop_names, grp_names;
    string f;
    uint   len;

    if (pmap_path.length() > 0) {
	cerr << "Parsing population map.\n";

	ifstream fh(pmap_path.c_str(), ifstream::in);

	if (fh.fail()) {
	    cerr << "Error opening population map '" << pmap_path << "'\n";
	    return 0;
	}

	uint pop_id = 0;
	uint grp_id = 0;

	while (fh.good()) {
	    fh.getline(line, max_len);

	    len = strlen(line);
	    if (len == 0) continue;

	    //
	    // Check that there is no carraige return in the buffer.
	    //
	    if (line[len - 1] == '\r') line[len - 1] = '\0';

	    //
	    // Ignore comments
	    //
	    if (line[0] == '#') continue;

	    //
	    // Parse the population map, we expect:
	    // <file name><tab><population string>[<tab><group string>]
	    //
	    parse_tsv(line, parts);

	    if (parts.size() < 2 || parts.size() > 3) {
		cerr << "Population map is not formated correctly: expecting two or three, tab separated columns, found " << parts.size() << ".\n";
		return 0;
	    }

	    //
	    // Have we seen this population or group before?
	    //
	    if (pop_names.count(parts[1]) == 0) {
		pop_names.insert(parts[1]);
		pop_id++;
		pop_key[pop_id]       = parts[1];
		pop_key_rev[parts[1]] = pop_id;

		//
		// If this is the first time we have seen this population, but not the
		// first time we have seen this group, add the population to the group list.
		//
		if (parts.size() == 3 && grp_key_rev.count(parts[2]) > 0)
		    grp_members[grp_key_rev[parts[2]]].push_back(pop_id);
	    }
	    if (parts.size() == 3 && grp_names.count(parts[2]) == 0) {
		grp_names.insert(parts[2]);
		grp_id++;
		grp_key[grp_id] = parts[2];
		grp_key_rev[parts[2]] = grp_id;

		//
		// Associate the current population with the group.
		//
		grp_members[grp_id].push_back(pop_id);
	    }

	    //
	    // Test that file exists before adding to list.
	    //
	    ifstream test_fh;
	    gzFile   gz_test_fh;

	    f = in_path.c_str() + parts[0] + ".matches.tsv";
	    test_fh.open(f.c_str());

	   /*if (test_fh.fail()) {
		//
		// Test for a gzipped file.
		//
		f = in_path.c_str() + parts[0] + ".matches.tsv.gz";
		gz_test_fh = gzopen(f.c_str(), "rb");
		if (!gz_test_fh) {
		    cerr << " Unable to find " << f.c_str() << ", excluding it from the analysis.\n";
		} else {
		    gzclose(gz_test_fh);
		    files.push_back(make_pair(pop_key_rev[parts[1]], parts[0]));
		}
	    } else {*/
		test_fh.close();
		files.push_back(make_pair(pop_key_rev[parts[1]], parts[0]));
	    //}
	}

	fh.close();
    } else {
	cerr << "No population map specified, building file list.\n";

	//
	// If no population map is specified, read all the files from the Stacks directory.
	//
	uint   pos;
	string file;
	struct dirent *direntry;

	DIR *dir = opendir(in_path.c_str());

	if (dir == NULL) {
	    cerr << "Unable to open directory '" << in_path << "' for reading.\n";
	    exit(1);
	}

	while ((direntry = readdir(dir)) != NULL) {
	    file = direntry->d_name;

	    if (file == "." || file == "..")
		continue;

	    if (file.substr(0, 6) == "batch_")
		continue;

	    pos = file.rfind(".tags.tsv");
	    if (pos < file.length()) {
		files.push_back(make_pair(1, file.substr(0, pos)));
	    } else {
		pos = file.rfind(".tags.tsv.gz");
		if (pos < file.length())
		    files.push_back(make_pair(1, file.substr(0, pos)));
	    }
	}

	pop_key[1] = "1";

	closedir(dir);
    }

    if (files.size() == 0) {
	cerr << "Unable to locate any input files to process within '" << in_path << "'\n";
	return 0;
    }

    //
    // Sort the files according to population ID.
    //
    sort(files.begin(), files.end(), compare_pop_map);

    cerr << "Found " << files.size() << " input file(s).\n";

    //
    // Determine the start/end index for each population in the files array.
    //
    int start  = 0;
    int end    = 0;
    int pop_id = files[0].first;

    do {
	end++;
	if (pop_id != files[end].first) {
	    pop_indexes[pop_id] = make_pair(start, end - 1);
	    start  = end;
	    pop_id = files[end].first;
	}
    } while (end < (int) files.size());

    pop_indexes.size() == 1 ?
	cerr << "  " << pop_indexes.size() << " population found\n" :
	cerr << "  " << pop_indexes.size() << " populations found\n";

    if (population_limit > (int) pop_indexes.size()) {
	cerr << "Population limit (" 
	     << population_limit 
	     << ") larger than number of popualtions present, adjusting parameter to " 
	     << pop_indexes.size() << "\n";
	population_limit = pop_indexes.size();
    }

    map<int, pair<int, int> >::iterator it;
    for (it = pop_indexes.begin(); it != pop_indexes.end(); it++) {
	start = it->second.first;
	end   = it->second.second;
	cerr << "    " << pop_key[it->first] << ": ";
	for (int i = start; i <= end; i++) {
	    cerr << files[i].second;
	    if (i < end) cerr << ", ";
	}
	cerr << "\n";
    }

    //
    // If no group membership is specified in the population map, create a default 
    // group with each population ID as a member.
    //
    if (grp_members.size() == 0) {
	for (it = pop_indexes.begin(); it != pop_indexes.end(); it++)
	    grp_members[1].push_back(it->first);
	grp_key[1] = "1";
    }

    grp_members.size() == 1 ?
	cerr << "  " << grp_members.size() << " group of populations found\n" :
	cerr << "  " << grp_members.size() << " groups of populations found\n";
    map<int, vector<int> >::iterator git;
    for (git = grp_members.begin(); git != grp_members.end(); git++) {
	cerr << "    " << grp_key[git->first] << ": ";
	for (uint i = 0; i < git->second.size(); i++) {
	    cerr << pop_key[git->second[i]];
	    if (i < git->second.size() - 1) cerr << ", ";
	}
	cerr << "\n";
    }

    return 1;
}

bool compare_pop_map(pair<int, string> a, pair<int, string> b) {
    if (a.first == b.first)
	return (a.second < b.second);
    return (a.first < b.first);
}



int cluster_filter(map<int, CSLocus *> &catalog, 
			set<int> &blacklist,ofstream &log_fh,
			string wl_path)

{
   Tag *tag, *tag_1, *tag_2; 
   vector<int> keys;   
   int i =0, het_count = 0, loci_count = 0, non_clustered_count = 0, clustered_count = 0;
   map<int, Tag *> tags;
   map<int, std::vector<int> > merged;
   map<int, std::vector<int> >::iterator merged_it;
   vector<int>::iterator vec_it;
   vector<pair<int, int> >::iterator dist_it;
   map<int, Tag *>::iterator tags_it;
   map<int, int>::iterator psv_it;
   vector<int> het_counter;
   vector<int>::iterator hc_it;
   map<int, CSLocus *>::iterator it;
   CSLocus *loc; 
   loci_count =  catalog.size();
   int mismatches = 0, seq_len =0;

   loc = catalog.begin() ->second;
   seq_len = string(loc -> con).length();
   mismatches = seq_len - (cluster_similarity*seq_len);
   ofstream wl_fh(wl_path.c_str(), ofstream::out);
    if (wl_fh.fail()) {
        cerr << "Error opening WL file '" << wl_path << "'\n";
	exit(1);
    }
   cerr << "Clustering loci for paralog filtering" << "\n";
    for (it = catalog.begin(); it != catalog.end(); it++) {
        loc = it->second;
        if (loc->snps.size() != 0) {
         het_counter.push_back(loc -> id);
        }
        tag = new Tag;
        tag -> id = loc -> id;
        tag -> seq= loc -> con;
        tag -> len= tag -> seq.length();
        tags[i] = tag;
        keys.push_back(i);
        i++;

      }

    #pragma omp parallel private(tag_1, tag_2)
    { 
      #pragma omp for  schedule(dynamic) 
        
	for (int i = 0; i < keys.size(); i++) {
             if (i % 100 == 0) cerr << "Calculationg distances for loci# " << i << "       \r";
            tag_1 = tags[keys[i]];
            int d;
            for (int j = i; j < keys.size(); j++) {
            tag_2 = tags[keys[j]];
            if ( tag_1 == tag_2) d = 0;
            else d = dist(tag_1, tag_2, mismatches); 
            if ( d != -1) tag_1->add_dist(j, d);
            }               
           
           }  
      

  }


for (tags_it = tags.begin(); tags_it != tags.end(); tags_it++)
{
  
 for (dist_it = tags_it->second->dist.begin(); dist_it != tags_it->second->dist.end(); dist_it++)
{
  if ( dist_it->second <= mismatches)
 {
  merged[tags_it->first].push_back(dist_it->first);
  if (tags_it->first != dist_it->first)
  merged[dist_it->first].push_back(tags_it->first);
  
 }
}

}
int size =0, li =0, j=0;
for (int k=0; k < merged.size(); k++)
{   
size =  merged[k].size();
	for ( int i =0; i < size; i++)
	   {
		li = merged[k].at(i);
		if ( k == li) continue;
		if (merged[li].size() > 1)
		{
		merged[k].insert( merged[k].end(), merged[li].begin(), merged[li].end());
		merged[li].clear();
		std::sort( merged[k].begin(), merged[k].end() );
		merged[k].erase( std::unique( merged[k].begin(), merged[k].end() ), merged[k].end() );
		size =  merged[k].size();
		i = 0;
		}
	   }  
}

for (merged_it = merged.begin(); merged_it != merged.end(); merged_it++)
{   

j++;

 if (merged_it ->second.size() == 1) 
{
wl_fh << tags [merged_it->first]->id << "\n";
non_clustered_count++;
}
 else if (merged_it ->second.size() > 1)
  {
     
    hc_it = find(het_counter.begin(),het_counter.end(),tags [merged_it->first]->id );
    if ( hc_it != het_counter.end()) het_count++;
    blacklist.insert(tags [merged_it->first]->id);  
    for (vec_it = merged_it->second.begin(); vec_it != merged_it->second.end(); vec_it++)
     {   
     	if(merged_it ->first != *vec_it)
    	 { 
              hc_it = find(het_counter.begin(),het_counter.end(),tags [*vec_it]->id);
              if ( hc_it != het_counter.end()) het_count++;
               blacklist.insert(tags [*vec_it]->id); 
         	 
     	 }
    }
   
  }
 
 
}
clustered_count = loci_count - non_clustered_count;
log_fh << "\n#\n# Cluster filtering stats \n#\n";
log_fh << "Number of Non-clustered loci" <<"\t"<< non_clustered_count << "\n";  
log_fh << "Number of clustered loci" <<"\t"<< clustered_count<< "\n";  
log_fh << "Number of polymorphic loci in the clustered loci" <<"\t"<< het_count << "\n";
log_fh << "Number of fixed loci in the clustered loci" <<"\t"<< clustered_count - het_count << "\n";    
return 0;
}	


		
int
prune_polymorphic_sites(map<int, CSLocus *> &catalog, 
			PopMap<CSLocus> *pmap,
			PopSum<CSLocus> *psum,
			map<int, pair<int, int> > &pop_indexes, 
			map<int, set<int> > &whitelist,set<int> &blacklist,
			ofstream &log_fh, string wl_path)
{
    map<int, set<int> > new_wl;
    vector<int> pop_prune_list;
    CSLocus  *loc;
    LocTally *t;
    LocSum  **s;
    Datum   **d;
    bool      sample_prune, maf_prune, inc_prune, het_prune;
    int       size, pruned = 0;
    uint      pop_id, start_index, end_index;
    ofstream wl_fh(wl_path.c_str(), ofstream::out);
    if (wl_fh.fail()) {
        cerr << "Error opening WL file '" << wl_path << "'\n";
	exit(1);
    }
	log_fh << "\n#\n# List of pruned nucleotide sites\n#\n"
	       << "# Action\tLocus ID\tChr\tBP\tColumn\tReason\n";
      
	//
	// iterate over the catalog.
	//
	map<int, CSLocus *>::iterator it;
	for (it = catalog.begin(); it != catalog.end(); it++) {
	    loc = it->second;

	    //
	    // If this locus is fixed, don't try to filter it out.
	    //
	    if (loc->snps.size() == 0) {
		new_wl.insert(make_pair(loc->id, std::set<int>()));
		continue;
	    }

	    t = psum->locus_tally(loc->id);
	    s = psum->locus(loc->id);

	    for (uint i = 0; i < loc->snps.size(); i++) {

		//
		// If the site is fixed, ignore it.
		//
		if (t->nucs[loc->snps[i]->col].fixed == true)
		    {
                   new_wl.insert(make_pair(loc->id, std::set<int>()));
		   continue;
                    }

		sample_prune = false;
		maf_prune    = false;
		inc_prune    = false;
                het_prune    = false;
		pop_prune_list.clear();
		
		for (int j = 0; j < psum->pop_cnt(); j++) {
		    pop_id = psum->rev_pop_index(j);

		    if (s[j]->nucs[loc->snps[i]->col].incompatible_site)
			inc_prune = true;
		    else if (s[j]->nucs[loc->snps[i]->col].num_indv == 0 ||
			     (double) s[j]->nucs[loc->snps[i]->col].num_indv / (double) psum->pop_size(pop_id) < sample_limit)
			pop_prune_list.push_back(pop_id);
		}

		//
		// Check how many populations have to be pruned out due to sample limit. If less than
		// population limit, prune them; if more than population limit, mark locus for deletion.
		//
		if ((psum->pop_cnt() - pop_prune_list.size()) < (uint) population_limit) {
		    sample_prune = true;
		} else {
		    for (uint j = 0; j < pop_prune_list.size(); j++) {
			if (s[psum->pop_index(pop_prune_list[j])]->nucs[loc->snps[i]->col].num_indv == 0) continue;
			
		    	start_index = pop_indexes[pop_prune_list[j]].first;
		    	end_index   = pop_indexes[pop_prune_list[j]].second;
		    	d           = pmap->locus(loc->id);

			for (uint k = start_index; k <= end_index; k++) {
			    if (d[k] == NULL || loc->snps[i]->col >= (uint) d[k]->len) 
				continue;
			    if (d[k]->model != NULL) {
				d[k]->model[loc->snps[i]->col] = 'U';
			    }
			}
		    }
		}
		
		if (t->nucs[loc->snps[i]->col].allele_cnt > 1) {
		    //
		    // Test for minor allele frequency.
		    //
		    if ((1 - t->nucs[loc->snps[i]->col].p_freq) < minor_allele_freq)
                       {
			maf_prune = true;
                       
                        }
		    //
		    // Test for observed heterozygosity.
		    //
		    if (t->nucs[loc->snps[i]->col].obs_het > max_obs_het)
                        {
		    	het_prune = true;
                       
                        }
		}

		if (maf_prune == false && het_prune == false && sample_prune == false && inc_prune == false) {
		    new_wl[loc->id].insert(loc->snps[i]->col);
		} else {
		    pruned++;
		    if (verbose) {
			log_fh << "pruned_polymorphic_site\t"
			       << loc->id << "\t"
			       << loc->loc.chr << "\t"
			       << loc->sort_bp(loc->snps[i]->col) << "\t"
			       << loc->snps[i]->col << "\t"; 
			if (inc_prune)
			    log_fh << "incompatible_site\n";
			else if (sample_prune)
			    log_fh << "sample_limit\n";
			else if (maf_prune)
			    log_fh << "maf_limit\n";
			else if (het_prune)
			    log_fh << "obshet_limit\n";
			else
			    log_fh << "unknown_reason\n";
		    }
		}
	    }

	    //
	    // If no SNPs were retained for this locus, then mark it to be removed entirely.
	    //
	    if (new_wl.count(loc->id) == 0) {
		    log_fh << "removed_locus\t"
			   << loc->id << "\t"
			   << loc->loc.chr << "\t"
			   << loc->sort_bp() << "\t"
			   << 0 << "\tno_snps_remaining\n";
		blacklist.insert(loc->id);
                
	    }
           else
              {
                  wl_fh << loc->id << "\n";
              }
	}
     
    return pruned;
}




int parse_command_line(int argc, char* argv[]) {
    int c;
     
    while (1) {
	static struct option long_options[] = {
	    {"num_threads",    required_argument, NULL, 't'},
	    {"batch_id",       required_argument, NULL, 'b'},
	    {"progeny",        required_argument, NULL, 'r'},
	    {"min_populations",   required_argument, NULL, 'p'},
            {"in_path",        required_argument, NULL, 'P'},
             {"out_path",        required_argument, NULL, 'o'},
            {"pop_map",        required_argument, NULL, 'M'}, 
	    {"minor_allele_freq", required_argument, NULL, 'a'},
            {"max_obs_het",       required_argument, NULL, 'q'},
            {"lnl_lim",           required_argument, NULL, 'c'},
            {"min_depth",      required_argument, NULL, 'm'},
            {"ct", required_argument, NULL, 'C'},
	    {0, 0, 0, 0}
	};	
	// getopt_long stores the option index here.
	int option_index = 0;
     
	c = getopt_long(argc, argv,"a:b:c:C:m:p:q:r:t:M:P:", long_options, &option_index);

	// Detect the end of the options.
	if (c == -1)
	    break;
     
	switch (c) {
	case 't':
	    num_threads = atoi(optarg);
            cerr << num_threads << "\n";
	    break;
	case 'b':
	    batch_id = is_integer(optarg);
	    if (batch_id < 0) {
		cerr << "Batch ID (-b) must be an integer, e.g. 1, 2, 3\n";
		help();
	    }
	    break;
    case 'P':
	    in_path = optarg;
	    break;
	case 'M':
	    pmap_path = optarg;
	    break;
	case 'r':
	    sample_limit = atof(optarg);
	    break;
	case 'p':
	    population_limit = atoi(optarg);
	    break;
	case 'a':
	    minor_allele_freq = atof(optarg);
	    break;
       case 'C':
	    cluster_similarity = atof(optarg);
	    break;
      case 'c':
	    lnl_limit  = is_double(optarg);
	    break;
      case 'm':
	    min_stack_depth = atoi(optarg);
	    break;
      case 'q':
	    max_obs_het = is_double(optarg);
	    break;
	default:
	    help();
	    abort();
	}
    }
    if (in_path.length() == 0) {
	cerr << "You must specify a path to the directory containing Stacks output files.\n";
	help();
    }

    if (in_path.at(in_path.length() - 1) != '/') 
	in_path += "/";
    
	 
    if (pmap_path.length() == 0) {
	cerr << "A population map was not specified, all samples will be read from '" << in_path << "' as a single popultaion.\n";
    }
  
    

    if (batch_id < 0) {
	cerr << "You must specify a batch ID.\n";
	help();
    }

  
    if (minor_allele_freq > 0) {
	if (minor_allele_freq > 1)
	    minor_allele_freq = minor_allele_freq / 100;

	if (minor_allele_freq > 0.5) {
	    cerr << "Unable to parse the minor allele frequency\n";
	    help();
	}
    }

  if (max_obs_het != 1.0) {
	if (max_obs_het > 1)
	    max_obs_het = max_obs_het / 100;

	if (max_obs_het < 0 || max_obs_het > 1.0) {
	    cerr << "Unable to parse the maximum observed heterozygosity.\n";
	    help();
	}
    }


    if (sample_limit > 0) {
	if (sample_limit > 1)
	    sample_limit = sample_limit / 100;

	if (sample_limit > 1.0) {
	    cerr << "Unable to parse the sample limit frequency\n";
	    help();
	}
    }

    return 0;
}

void version() {
    std::cerr << "Stacks filter " << VERSION << "\n\n";

    exit(0);
}

void help() {
    std::cerr << "pmerge " << VERSION << "\n"
              << "pmerge -b batch_id -P path -M path [-r min] [-m min][-C cluster][-t threads]" << "\n"
	      << "  b: Batch ID to examine when exporting from the catalog.\n"
	      << "  P: path to the Stacks output files.\n"
	      << "  M: path to the population map, a tab separated file describing which individuals belong in which population.\n"
	      << "  t: number of threads to run in parallel sections of code.\n"

	    
	      << "  Data Filtering:\n"
	      << "    q: maximum observed heterozygosity.\n"
	      << "    r: minimum percentage of individuals in a population required to process a locus for that population.\n"
	      << "    p: minimum number of populations a locus must be present in to process a locus.\n"
	      << "    m: specify a minimum stack depth required for individuals at a locus.\n"
	      << "    a: specify a minimum minor allele frequency required to process a nucleotide site at a locus (0 < a < 0.5).\n"  
	      << "    c: filter loci with log likelihood values below this threshold.\n"
              << "    C: minimum percentage of similarity between loci to cluster. \n";
	     
    

    exit(0);
}
