/*****************************************************************************
*
* Copyright (c) 2003-2019 by The University of Queensland
* http://www.uq.edu.au
*
* Primary Business: Queensland, Australia
* Licensed under the Apache License, version 2.0
* http://www.apache.org/licenses/LICENSE-2.0
*
* Development until 2012 by Earth Systems Science Computational Center (ESSCC)
* Development 2012-2013 by School of Earth Sciences
* Development from 2014 by Centre for Geoscience Computing (GeoComp)
*
*****************************************************************************/

#include <oxley/RefinementAlgorithms.h>

// This file contains various callback functions to decide on refinement.
 

/*
 * Here we use uniform refinement.  Note that this function is not suitable for
 * recursive refinement and must be used in an iterative fashion.
 */

namespace oxley {

int refine_uniform(p4est_t * p4est, p4est_topidx_t which_tree, p4est_quadrant_t * quadrant)
{
    return 1;
}

// int refine_uniform(p8est_t * p8est, p8est_topidx_t which_tree, p8est_quadrant_t * quadrant)
// {
//     return 1;
// };


} // namespace oxley