
/*****************************************************************************
*
* Copyright (c) 2003-2014 by University of Queensland
* http://www.uq.edu.au
*
* Primary Business: Queensland, Australia
* Licensed under the Open Software License version 3.0
* http://www.opensource.org/licenses/osl-3.0.php
*
* Development until 2012 by Earth Systems Science Computational Center (ESSCC)
* Development 2012-2013 by School of Earth Sciences
* Development from 2014 by Centre for Geoscience Computing (GeoComp)
*
*****************************************************************************/


/********************************************************************************************/

/* Paso: Pattern: Paso_Pattern_mis 

   Searches for a maximal independent set MIS in the matrix pattern.
   Vertices in the maximal independent set are marked in mis_marker.
   Nodes to be considered are marked by -1 on the input in mis_marker.

*/
/********************************************************************************************/

/* Copyrights by ACcESS Australia 2003,2004,2005                      */
/* Author: Lutz Gross, l.gross@uq.edu.au                              */

/************************************************************************************/


#include "Paso.h"
#include "PasoUtil.h"
#include "Pattern.h"
#include "esysUtils/mpi_C.h"

/* used to generate pseudo random numbers: */

static double Paso_Pattern_mis_seed=.4142135623730951;


/*************************************************************************************/
 
#define IS_AVAILABLE -1
#define IS_IN_MIS_NOW -2
#define IS_IN_MIS -3
#define IS_CONNECTED_TO_MIS -4

void Paso_Pattern_mis(Paso_Pattern* pattern_p, index_t* mis_marker) {

  const index_t index_offset=(pattern_p->type & MATRIX_FORMAT_OFFSET1 ? 1:0);
  dim_t i;
  double *value;
  index_t naib,iptr;
  index_t flag;
  dim_t n=pattern_p->numOutput;
  if (pattern_p->numOutput != pattern_p->numInput) {
     Esys_setError(VALUE_ERROR,"Paso_Pattern_mis: pattern must be square.");
     return;
  }
  value=new double[n];
  if (!Esys_checkPtr(value)) {

   
     /* is there any vertex available ?*/
     while (Paso_Util_isAny(n,mis_marker,IS_AVAILABLE)) {
        /* step 1: assign a random number in [0,1] to each vertex */
        /* step 2: if the vertex is available, check if its value is the smaller than all values of its neighbours 
                   if the answer is yes, the vertex is put into the independent set and all 
                   its neighbours are removed from the graph by setting it mis_marker to FALSE */
      
        /* step 2: is the vertex is available, check if its value is the smaller than all values of its neighbours */

           /* assign random number in [0,1] to each vertex */
           #pragma omp parallel for private(i) schedule(static)
           for (i=0;i<n;++i) {
                 if (mis_marker[i]==IS_AVAILABLE) {
                    value[i]=fmod(Paso_Pattern_mis_seed*(i+1),1.);
                 } else {
                    value[i]=2.;
                 }
           }
           /* update the seed */
           /* Paso_Pattern_mis_seed=fmod(sqrt(Paso_Pattern_mis_seed*(n+1)),1.); */
           /* detect independent vertices as those vertices that have a value less than all values of its neighbours */
           #pragma omp parallel for private(naib,i,iptr,flag) schedule(static) 
           for (i=0;i<n;++i) {
              if (mis_marker[i]==IS_AVAILABLE) {
                 flag=IS_IN_MIS_NOW;
                 for (iptr=pattern_p->ptr[i]-index_offset;iptr<pattern_p->ptr[i+1]-index_offset; ++iptr) {
                     naib=pattern_p->index[iptr]-index_offset;
                     if (naib!=i && value[naib]<=value[i]) {
                        flag=IS_AVAILABLE;
                        break;
                     }
                 }
                 mis_marker[i]=flag;
              }
           }
           /* detect independent vertices as those vertices that have a value less than all values of its neighbours */
           #pragma omp parallel for private(naib,i,iptr) schedule(static)
           for (i=0;i<n;i++) {
              if (mis_marker[i]==IS_IN_MIS_NOW) {
                 for (iptr=pattern_p->ptr[i]-index_offset;iptr<pattern_p->ptr[i+1]-index_offset; ++iptr) {
                     naib=pattern_p->index[iptr]-index_offset;
                     if (naib!=i) mis_marker[naib]=IS_CONNECTED_TO_MIS;
                 }
                 mis_marker[i]=IS_IN_MIS;
              }
           }
     }
     /* swap to TRUE/FALSE in mis_marker */
     #pragma omp parallel for private(i) schedule(static)
     for (i=0;i<n;i++) mis_marker[i]=(mis_marker[i]==IS_IN_MIS);
  }
  delete[] value;
}
#undef IS_AVAILABLE 
#undef IS_IN_MIS_NOW 
#undef IS_IN_MIS 
#undef IS_CONNECTED_TO_MIS 

void Paso_Pattern_color(Paso_Pattern* pattern, index_t* num_colors, index_t* colorOf) {
  index_t out=0, *mis_marker=NULL,i;
  dim_t n=pattern->numOutput;
  mis_marker=new index_t[n];
  if ( !Esys_checkPtr(mis_marker) ) {
    /* get coloring */
    #pragma omp parallel for private(i) schedule(static)
    for (i = 0; i < n; ++i) {
        colorOf[i]=-1;
        mis_marker[i]=-1;
    }

    while (Paso_Util_isAny(n,colorOf,-1) && Esys_noError()) {
       /*#pragma omp parallel for private(i) schedule(static)
       for (i = 0; i < n; ++i) mis_marker[i]=colorOf[i];*/
       Paso_Pattern_mis(pattern,mis_marker);

       #pragma omp parallel for private(i) schedule(static)
       for (i = 0; i < n; ++i) {
            if (mis_marker[i]) colorOf[i]=out;
            mis_marker[i]=colorOf[i];
       }
       ++out;
    }
    delete[] mis_marker;
    *num_colors=out;
  }
}

