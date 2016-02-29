
/*****************************************************************************
*
* Copyright (c) 2003-2016 by The University of Queensland
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


/****************************************************************************/

/*   esysUtils: IndexList                                                   */

/****************************************************************************/

/*   Author: Lutz Gross, l.gross@uq.edu.au */

/****************************************************************************/

#ifndef __ESYSUTILS_INDEXLIST_H__
#define __ESYSUTILS_INDEXLIST_H__

#include <escript/DataTypes.h>

// pre-reserving saves time under OpenMP. The 85 is a value taken over
// from revision ~101 by jgs.
#define ESYS_INDEXLIST_LENGTH 85

namespace esysUtils {

struct IndexList {
    IndexList() : n(0), extension(NULL) {}
    ~IndexList() { delete extension; }

    escript::DataTypes::index_t m_list[ESYS_INDEXLIST_LENGTH];
    escript::DataTypes::dim_t n;
    IndexList* extension;

    /// inserts row index into the IndexList in if it does not exist
    inline void insertIndex(escript::DataTypes::index_t index)
    {
        for (escript::DataTypes::dim_t i=0; i<n; i++) {
            if (m_list[i] == index)
                return;
        }
        if (n < ESYS_INDEXLIST_LENGTH) {
            m_list[n++] = index;
        } else {
            if (extension == NULL)
                extension = new IndexList();
            extension->insertIndex(index);
        }
    }

    /// counts the number of row indices in the IndexList in
    inline escript::DataTypes::dim_t count(escript::DataTypes::index_t range_min, escript::DataTypes::index_t range_max) const
    {
        escript::DataTypes::dim_t out=0;
        for (escript::DataTypes::dim_t i=0; i < n; i++) {
            if (m_list[i] >= range_min && range_max > m_list[i])
                ++out;
        }
        if (extension)
            out += extension->count(range_min, range_max);
        return out;
    }

    /// index list to array
    inline void toArray(escript::DataTypes::index_t* array, escript::DataTypes::index_t range_min, escript::DataTypes::index_t range_max,
                        escript::DataTypes::index_t index_offset) const
    {
        escript::DataTypes::index_t idx = 0;
        for (escript::DataTypes::dim_t i=0; i < n; i++) {
            if (m_list[i] >= range_min && range_max > m_list[i]) {
                array[idx] = m_list[i]+index_offset;
                ++idx;
            }
        }
        if (extension)
            extension->toArray(&array[idx], range_min, range_max, index_offset);
    }
};

} // namespace esysUtils

#endif // __ESYSUTILS_INDEXLIST_H__

