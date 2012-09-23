
##############################################################################
#
# Copyright (c) 2003-2012 by University of Queensland
# http://www.uq.edu.au
#
# Primary Business: Queensland, Australia
# Licensed under the Open Software License version 3.0
# http://www.opensource.org/licenses/osl-3.0.php
#
# Development until 2012 by Earth Systems Science Computational Center (ESSCC)
# Development since 2012 by School of Earth Sciences
#
##############################################################################

__copyright__="""Copyright (c) 2003-2012 by University of Queensland
http://www.uq.edu.au
Primary Business: Queensland, Australia"""
__license__="""Licensed under the Open Software License version 3.0
http://www.opensource.org/licenses/osl-3.0.php"""
__url__="https://launchpad.net/escript-finley"

import logging
import numpy as np
import os
import sys
import unittest
from esys.escript import inf,sup,saveDataCSV
from esys import ripley, finley, dudley
from esys.downunder.datasources import *

# this is mainly to avoid warning messages
logger=logging.getLogger('inv')
logger.setLevel(logging.INFO)
handler=logging.StreamHandler()
handler.setLevel(logging.INFO)
logger.addHandler(handler)

try:
    TEST_DATA_ROOT=os.environ['DOWNUNDER_TEST_DATA_ROOT']
except KeyError:
    TEST_DATA_ROOT='.'

try:
    WORKDIR=os.environ['DOWNUNDER_WORKDIR']
except KeyError:
    WORKDIR='.'


ERS_DATA = os.path.join(TEST_DATA_ROOT, 'ermapper_test.ers')
ERS_REF = os.path.join(TEST_DATA_ROOT, 'ermapper_test.csv')
ERS_NULL = -99999 * 1e-6
ERS_SIZE = [10,10]
ERS_ORIGIN = [309097.0, 6321502.0]
VMIN=-10000.
VMAX=10000
NE_V=15
ALT=0.
PAD_XY=7
PAD_Z=3

class TestERSDataSource(unittest.TestCase):
    def test_ers_with_padding(self):
        source = ERSDataSource(headerfile=ERS_DATA, vertical_extents=(VMIN,VMAX,NE_V), alt_of_data=ALT)
        source.setPadding(PAD_XY,PAD_Z)
        dom=source.getDomain()
        g,s=source.getGravityAndStdDev()

        outfn=os.path.join(WORKDIR, '_ersdata.csv')
        saveDataCSV(outfn, g=g[2], s=s)

        X0,NP,DX=source.getDataExtents()
        V0,NV,DV=source.getVerticalExtents()

        # check metadata
        self.assertEqual(NP, ERS_SIZE, msg="Wrong number of data points")
        # this only works if gdal is available
        #self.assertAlmostEqual(X0, ERS_ORIGIN, msg="Data origin wrong")

        # check data
        nx=NP[0]+2*PAD_XY
        ny=NP[1]+2*PAD_XY
        nz=NE_V+2*PAD_Z
        z_data=int(np.round((ALT-V0)/DV)-1+PAD_Z)

        ref=np.genfromtxt(ERS_REF, delimiter=',', dtype=float)
        g_ref=ref[:,0].reshape(NP)
        s_ref=ref[:,1].reshape(NP)

        out=np.genfromtxt(outfn, delimiter=',', skip_header=1, dtype=float)
        # recompute nz since ripley might have adjusted number of elements
        nz=len(out)/(nx*ny)
        g_out=out[:,0].reshape(nz,ny,nx)
        s_out=out[:,1].reshape(nz,ny,nx)

        self.assertAlmostEqual(np.abs(
            g_out[z_data,PAD_XY:PAD_XY+NP[1],PAD_XY:PAD_XY+NP[0]]-g_ref).max(),
            0., msg="Difference in data area")

        # overwrite data -> should only be padding value left
        g_out[z_data, PAD_XY:PAD_XY+NP[0], PAD_XY:PAD_XY+NP[0]]=ERS_NULL
        self.assertAlmostEqual(np.abs(g_out-ERS_NULL).max(), 0.,
                msg="Wrong values in padding area")

if __name__ == "__main__":
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(TestERSDataSource))
    s=unittest.TextTestRunner(verbosity=2).run(suite)
    if not s.wasSuccessful(): sys.exit(1)

