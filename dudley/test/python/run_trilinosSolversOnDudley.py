
##############################################################################
#
# Copyright (c) 2003-2016 by The University of Queensland
# http://www.uq.edu.au
#
# Primary Business: Queensland, Australia
# Licensed under the Open Software License version 3.0
# http://www.opensource.org/licenses/osl-3.0.php
#
# Development until 2012 by Earth Systems Science Computational Center (ESSCC)
# Development 2012-2013 by School of Earth Sciences
# Development from 2014 by Centre for Geoscience Computing (GeoComp)
#
##############################################################################

from __future__ import print_function, division

__copyright__="""Copyright (c) 2003-2016 by The University of Queensland
http://www.uq.edu.au
Primary Business: Queensland, Australia"""
__license__="""Licensed under the Open Software License version 3.0
http://www.opensource.org/licenses/osl-3.0.php"""
__url__="https://launchpad.net/escript-finley"

"""
Test suite for PDE solvers on dudley
"""

from test_simplesolve import SimpleSolveTestCase
import esys.escriptcore.utestselect as unittest
from esys.escriptcore.testing import *

from esys.dudley import Rectangle, Brick
from esys.escript.linearPDEs import SolverOptions

# number of elements in the spatial directions
NE0=12
NE1=12
NE2=8
OPTIMIZE=True

class SimpleSolveSingleOnly(SimpleSolveTestCase):
    @unittest.skip("PDE systems not supported with Trilinos yet")
    def test_system(self):
        pass

### BiCGStab + Jacobi

class Test_SimpleSolveDudleyRect_Trilinos_BICGSTAB_Jacobi(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Rectangle(NE0, NE1, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.BICGSTAB
        self.preconditioner = SolverOptions.JACOBI

    def tearDown(self):
        del self.domain

class Test_SimpleSolveDudleyBrick_Trilinos_BICGSTAB_Jacobi(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Brick(NE0, NE1, NE2, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.BICGSTAB
        self.preconditioner = SolverOptions.JACOBI

    def tearDown(self):
        del self.domain

### PCG + Jacobi

class Test_SimpleSolveDudleyRect_Trilinos_PCG_Jacobi(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Rectangle(NE0, NE1, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.PCG
        self.preconditioner = SolverOptions.JACOBI

    def tearDown(self):
        del self.domain

class Test_SimpleSolveDudleyBrick_Trilinos_PCG_Jacobi(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Brick(NE0, NE1, NE2, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.PCG
        self.preconditioner = SolverOptions.JACOBI

    def tearDown(self):
        del self.domain

### TFQMR + Jacobi

class Test_SimpleSolveDudleyRect_Trilinos_TFQMR_Jacobi(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Rectangle(NE0, NE1, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.TFQMR
        self.preconditioner = SolverOptions.JACOBI

    def tearDown(self):
        del self.domain

class Test_SimpleSolveDudleyBrick_Trilinos_TFQMR_Jacobi(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Brick(NE0, NE1, NE2, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.TFQMR
        self.preconditioner = SolverOptions.JACOBI

    def tearDown(self):
        del self.domain

### MINRES + Jacobi

class Test_SimpleSolveDudleyRect_Trilinos_MINRES_Jacobi(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Rectangle(NE0, NE1, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.MINRES
        self.preconditioner = SolverOptions.JACOBI

    def tearDown(self):
        del self.domain

class Test_SimpleSolveDudleyBrick_Trilinos_MINRES_Jacobi(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Brick(NE0, NE1, NE2, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.MINRES
        self.preconditioner = SolverOptions.JACOBI

    def tearDown(self):
        del self.domain

### BiCGStab + Gauss-Seidel

class Test_SimpleSolveDudleyRect_Trilinos_BICGSTAB_GaussSeidel(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Rectangle(NE0, NE1, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.BICGSTAB
        self.preconditioner = SolverOptions.GAUSS_SEIDEL

    def tearDown(self):
        del self.domain

class Test_SimpleSolveDudleyBrick_Trilinos_BICGSTAB_GaussSeidel(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Brick(NE0, NE1, NE2, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.BICGSTAB
        self.preconditioner = SolverOptions.GAUSS_SEIDEL

    def tearDown(self):
        del self.domain

### PCG + AMG

class Test_SimpleSolveDudleyRect_Trilinos_PCG_AMG(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Rectangle(NE0, NE1, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.PCG
        self.preconditioner = SolverOptions.AMG

    def tearDown(self):
        del self.domain

class Test_SimpleSolveDudleyBrick_Trilinos_PCG_AMG(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Brick(NE0, NE1, NE2, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.PCG
        self.preconditioner = SolverOptions.AMG

    def tearDown(self):
        del self.domain

### TFQMR + Gauss-Seidel

class Test_SimpleSolveDudleyRect_Trilinos_TFQMR_GaussSeidel(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Rectangle(NE0, NE1, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.TFQMR
        self.preconditioner = SolverOptions.GAUSS_SEIDEL

    def tearDown(self):
        del self.domain

class Test_SimpleSolveDudleyBrick_Trilinos_TFQMR_GaussSeidel(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Brick(NE0, NE1, NE2, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.TFQMR
        self.preconditioner = SolverOptions.GAUSS_SEIDEL

    def tearDown(self):
        del self.domain

### MINRES + AMG

class Test_SimpleSolveDudleyRect_Trilinos_MINRES_AMG(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Rectangle(NE0, NE1, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.MINRES
        self.preconditioner = SolverOptions.AMG

    def tearDown(self):
        del self.domain

class Test_SimpleSolveDudleyBrick_Trilinos_MINRES_AMG(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Brick(NE0, NE1, NE2, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.MINRES
        self.preconditioner = SolverOptions.AMG

    def tearDown(self):
        del self.domain

### BiCGStab + RILU

class Test_SimpleSolveDudleyRect_Trilinos_BICGSTAB_RILU(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Rectangle(NE0, NE1, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.BICGSTAB
        self.preconditioner = SolverOptions.RILU

    def tearDown(self):
        del self.domain

class Test_SimpleSolveDudleyBrick_Trilinos_BICGSTAB_RILU(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Brick(NE0, NE1, NE2, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.BICGSTAB
        self.preconditioner = SolverOptions.RILU

    def tearDown(self):
        del self.domain

### PCG + RILU

class Test_SimpleSolveDudleyRect_Trilinos_PCG_RILU(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Rectangle(NE0, NE1, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.PCG
        self.preconditioner = SolverOptions.RILU

    def tearDown(self):
        del self.domain

class Test_SimpleSolveDudleyBrick_Trilinos_PCG_RILU(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Brick(NE0, NE1, NE2, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.PCG
        self.preconditioner = SolverOptions.RILU

    def tearDown(self):
        del self.domain

### TFQMR + RILU

class Test_SimpleSolveDudleyRect_Trilinos_TFQMR_RILU(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Rectangle(NE0, NE1, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.TFQMR
        self.preconditioner = SolverOptions.RILU

    def tearDown(self):
        del self.domain

class Test_SimpleSolveDudleyBrick_Trilinos_TFQMR_RILU(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Brick(NE0, NE1, NE2, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.TFQMR
        self.preconditioner = SolverOptions.RILU

    def tearDown(self):
        del self.domain

### MINRES + RILU

class Test_SimpleSolveDudleyRect_Trilinos_MINRES_RILU(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Rectangle(NE0, NE1, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.MINRES
        self.preconditioner = SolverOptions.RILU

    def tearDown(self):
        del self.domain

class Test_SimpleSolveDudleyBrick_Trilinos_MINRES_RILU(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Brick(NE0, NE1, NE2, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.MINRES
        self.preconditioner = SolverOptions.RILU

    def tearDown(self):
        del self.domain

### PCG + ILUT

class Test_SimpleSolveDudleyRect_Trilinos_PCG_ILUT(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Rectangle(NE0, NE1, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.PCG
        self.preconditioner = SolverOptions.ILUT

    def tearDown(self):
        del self.domain

class Test_SimpleSolveDudleyBrick_Trilinos_PCG_ILUT(SimpleSolveSingleOnly):
    def setUp(self):
        self.domain = Brick(NE0, NE1, NE2, 1, optimize=OPTIMIZE)
        self.package = SolverOptions.TRILINOS
        self.method = SolverOptions.PCG
        self.preconditioner = SolverOptions.ILUT

    def tearDown(self):
        del self.domain


if __name__ == '__main__':
    run_tests(__name__, exit_on_failure=True)
