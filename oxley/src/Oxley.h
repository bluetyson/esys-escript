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

#include <escript/EsysMPI.h>

#include <boost/shared_ptr.hpp>
#include <list>
#include <map>
#include <string>
#include <vector>

namespace oxley {

using escript::DataTypes::dim_t;
using escript::DataTypes::index_t;
using escript::DataTypes::cplx_t;
using escript::DataTypes::real_t;

typedef std::pair<index_t,index_t> IndexPair;
typedef std::vector<index_t> IndexVector;
typedef std::vector<real_t> DoubleVector;
typedef std::vector<int> RankVector;
typedef std::map<std::string,int> TagMap;

// enum {
//     DegreesOfFreedom=1,
//     ReducedDegreesOfFreedom=2,
//     Nodes=3,
//     ReducedNodes=14,
//     Elements=4,
//     ReducedElements=10,
//     FaceElements=5,
//     ReducedFaceElements=11,
//     Points=6
// };

} // namespace oxley


