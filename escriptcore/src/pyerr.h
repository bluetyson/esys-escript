/*****************************************************************************
*
* Copyright (c)2015-2016 by The University of Queensland
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

#ifndef __ESCRIPT_PYERR_H__
#define __ESCRIPT_PYERR_H__

#include <boost/python/errors.hpp>

#include <string>

namespace escript {

void getStringFromPyException(boost::python::error_already_set e, std::string& errormsg);

} // namespace escript

#endif // __ESCRIPT_PYERR_H__
