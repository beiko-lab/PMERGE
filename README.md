# PMERGE 

(Compatible with Stacks 1.42)

**PMERGE**, is a software,  which implements a new method that identifies candidate PSVs by building networks of loci that share high levels of nucleotide similarity. The PMERGE is embedded  in the analysis pipeline of the widely used Stacks software,  and it is straightforward to apply it as an additional filter in population-genomic studies using RAD-seq data.

![PMERGE workflow]
(https://github.com/beiko-lab/PMERGE/blob/master/pmerge_flow%20(1).png)


 The PMERGE software  is run after cstacks and before populations to generate a “whitelist” of loci from the catalog based on population-level filtering conditions and our new paralog-detection method. The populations program then uses only the whitelisted loci to generate population-genetic statistics. Apart from the paralog filter, PMERGE includes the following filters that are also used by the populations program: percent samples limit per population (r), which requires that a locus be present in at least the specified percentage of individuals in a population; locus population limit (p), the minimum number of populations in which a locus must be present; minor allele frequency cutoff (a), which sets a minimum threshold for the frequency of the minor allele (the second-most-frequent allele at a given locus); maximum observed heterozygozity (q); and minimum stack depth (m) at a given locus.

**

Implementation
--------------

**

The PMERGE software is implemented in C++ and parallelized using the OpenMP libraries. The PMERGE  will complied in GNU-based Linux systems or BSD-based OS X systems. It is released under GNU GPL license.

Installation
------------

Download the source files from https://github.com/beiko-lab/PMERGE

In the Terminal:

•	Unzip the downloaded zip file.

•	Traverse to the folder "Install".

    $ ./configure
    $ make
    root access or sudo 
    $ make install

PMERGE will be installed in the path “ /usr/local/bin “



Usage
-----

 

    pmerge -b batch_id -P path -M path [-r min] [-m min][-C cluster][-t threads]
    
       b: Batch ID to examine when exporting from the catalog.
       
       P: path to the Stacks output files.
       
       M: path to the population map, a tab separated file describing which individuals belong in which population.
       
       t: number of threads to run in parallel sections of code. 
       
    Data Filtering: 
    
       q: maximum observed heterozygosity. 
       
       r: minimum percentage of individuals in a population required to process a locus for that population. 
       
       p: minimum number of populations a locus must be present in to process a locus. 
       
       m: specify a minimum stack depth required for individuals at a locus. 
       
       a: specify a minimum minor allele frequency required to process a nucleotide site at a locus (0 < a < 0.5). 
       
       c: filter loci with log likelihood values below this threshold. 
       
       C: minimum percentage of similarity between loci to cluster.

