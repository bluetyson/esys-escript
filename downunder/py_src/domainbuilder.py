
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

"""Domain construction from survey data for inversions"""

__copyright__="""Copyright (c) 2003-2012 by University of Queensland
http://www.uq.edu.au
Primary Business: Queensland, Australia"""
__license__="""Licensed under the Open Software License version 3.0
http://www.opensource.org/licenses/osl-3.0.php"""
__url__="https://launchpad.net/escript-finley"

__all__ = ['DomainBuilder']

import logging
import numpy as np
from esys.escript.util import *
from esys.escript import unitsSI as U
from esys.ripley import Brick, Rectangle
from .datasources import DataSource

class DomainBuilder(object):
    """
    This class is responsible for constructing an escript Domain object with
    suitable extents and resolution for survey data (`DataSource` objects)
    that is added to it.
    """

    def __init__(self, dim=3):
        """
        Constructor.

        :param dim: Dimensionality (2 or 3) of the target domain.
                    This has implications for the survey data than can be
                    added. By default a 3D domain is created.
        :type dim: ``int``
        """
        self.logger = logging.getLogger('inv.%s'%self.__class__.__name__)
        if dim not in (2,3):
            raise ValueError("Number of dimensions must be 2 or 3")
        self._domain=None
        self._dim=dim
        self._gravity_surveys=[]
        self._magnetic_surveys=[]
        self._sources=[]
        self.setPadding()
        self.setVerticalExtents()

    def addSource(self, source):
        """
        Adds a survey data provider to the domain builder.

        :param source: The data source to be added.
        :type source: `DataSource`
        """
        if not isinstance(source, DataSource):
            raise TypeError("source is not a DataSource")
        self._sources.append(source)

    def setPadding(self, pad_x=10, pad_y=10):
        """
        Sets the amount of padding around the dataset. If `pad_x`/`pad_y`
        is >=1 the value is treated as number of elements to be added to the
        domain (per side).
        If ``0 < pad_x,pad_y < 1``, the padding amount is relative to the
        dataset size. For example, calling ``setPadding(3, 0.1)`` to a data
        source with size 10x20 will result in the padded data set size
        16x24 (10+2*3, 20*(1+2*0.1))

        :param pad_x: Padding per side in x direction (default: 10 elements)
        :type pad_x: ``int`` or ``float``
        :param pad_y: Padding per side in y direction (default: 10 elements).
                      This value is only used for 3-dimensional datasets
        :type pad_y: ``int`` or ``float``
        """
        self._padding=[pad_x,pad_y]

    def getGravitySurveys(self):
        """
        Returns a list of `Data` objects for all gravity surveys available to
        this domain builder.

        :return: List of gravity surveys which are tuples (anomaly,error).
        :rtype: ``list``
        """
        if len(self._gravity_surveys)==0:
            for src in self._sources:
                if src.getDataType()==DataSource.GRAVITY:
                    survey=src.getSurveyData(self.getDomain(), self._dom_origin, self._dom_NE, self._spacing)
                    self._gravity_surveys.append(survey)
        return self._gravity_surveys

    def getMagneticSurveys(self):
        """
        Returns a list of `Data` objects for all magnetic surveys available to
        this domain builder.

        :return: List of magnetic surveys which are tuples (anomaly,error).
        :rtype: ``list``
        """
        if len(self._magnetic_surveys)==0:
            for src in self._sources:
                if src.getDataType()==DataSource.MAGNETIC:
                    survey=src.getSurveyData(self.getDomain(), self._dom_origin, self._dom_NE, self._spacing)
                    self._magnetic_surveys.append(survey)
        return self._magnetic_surveys

    def getBackgroundMagneticField(self):
        #FIXME:
        latitude=-28.5
        theta = (90-latitude)/180.*np.pi
        B_0 = U.Mu_0*U.Magnetic_Dipole_Moment_Earth / (4*np.pi*U.R_Earth**3)
        B_theta = B_0*sin(theta)
        B_r = 2*B_0*cos(theta)
        if self._dim < 3:
            return np.array([0., -B_r])
        else:
            #return np.array([3.0281e-5, 2.356e-6, -4.3346e-5])
            return np.array([-B_theta, 0., -B_r])

    def getSetDensityMask(self):
        return wherePositive(self.getDomain().getX()[self._dim-1])

    def getSetSusceptibilityMask(self):
        return wherePositive(self.getDomain().getX()[self._dim-1])

    def getDomain(self):
        """
        Returns a domain that spans the data area plus padding.
        The domain is created the first time this method is called, subsequent
        calls return the same domain so anything that affects the domain
        (such as padding) needs to be set beforehand.

        :return: The escript domain for this data source.
        :rtype: `esys.escript.Domain`
        """
        if self._domain is None:
            self._domain=self.__createDomain()
        return self._domain

    def setVerticalExtents(self, depth=40000., air_layer=10000., num_cells=25):
        """
        This method sets the target domain parameters for the vertical
        dimension.

        :param depth: Depth in meters of the domain.
        :type depth: ``float``
        :param air_layer: Depth of the layer above sea level in meters
        :type air_layer: ``float``
        :param num_cells: Number of domain elements for the entire vertical
                          dimension
        :type num_cells: ``int``
        """
        self._v_depth=depth
        self._v_air_layer=air_layer
        self._v_num_cells=num_cells

    def __getTotalExtentsWithPadding(self):
        """
        Helper method that computes origin and number of elements
        after adding padding to the bounding box of all available survey data.
        """
        X0, NX, DX = self.__getTotalExtents()
        DIM=len(X0)
        frac=[]
        # padding is applied to each side so multiply by 2 to get the total
        # amount of padding per dimension
        for i in range(DIM):
            if self._padding[i]>=0 and self._padding[i]<1:
                frac.append(2.*self._padding[i])
            else:
                frac.append(2.*self._padding[i]/float(NX[i]))

        # calculate new number of elements
        NX_padded=[int(round(NX[i]*(1+frac[i]))) for i in range(DIM)]
        NXdiff=[NX_padded[i]-NX[i] for i in range(DIM)]
        X0_padded=[X0[i]-NXdiff[i]/2.*DX[i] for i in range(DIM)]
        return X0_padded, NX_padded, DX

    def __getTotalExtents(self):
        """
        Helper method that computes the origin, number of elements and
        minimal element spacing taking into account all available survey data.
        """
        if len(self._sources)==0:
            raise ValueError("No data")
        X0, NE, DX = self._sources[0].getDataExtents()
        XN=[X0[i]+NE[i]*DX[i] for i in range(len(NE))]

        for src in self._sources[1:]:
            d_x0, d_ne, d_dx = src.getDataExtents()
            for i in range(len(d_x0)):
                X0[i]=min(X0[i], d_x0[i])
            for i in range(len(d_dx)):
                DX[i]=min(DX[i], d_dx[i])
            for i in range(len(d_ne)):
                XN[i]=max(XN[i], d_x0[i]+d_ne[i]*d_dx[i])
        NE=[int((XN[i]-X0[i])/DX[i]) for i in range(len(XN))]
        return X0, NE, DX

    def __createDomain(self):
        """
        Creates and returns an escript domain that spans the entire area of
        available data plus a padding zone. This method is called only once
        the first time `getDomain()` is invoked.

        :return: The escript domain
        :rtype: `esys.escript.Domain`
        """
        X0, NX, DX = self.__getTotalExtentsWithPadding()

        # number of domain elements
        NE = NX + [self._v_num_cells]

        # origin of domain
        origin = X0 + [-self._v_depth]
        self._dom_origin = [np.floor(oi) for oi in origin]

        # cell size / point spacing
        spacing = DX + [(self._v_depth+self._v_air_layer)/self._v_num_cells]
        self._spacing = [float(np.floor(si)) for si in spacing]

        lo=[(self._dom_origin[i], self._dom_origin[i]+NE[i]*self._spacing[i]) for i in range(self._dim)]
        if self._dim==3:
            dom=Brick(*NE, l0=lo[0], l1=lo[1], l2=lo[2])
        else:
            dom=Rectangle(*NE, l0=lo[0], l1=lo[1])
            
        # ripley may internally adjust NE and length, so recompute
        self._dom_len=[sup(dom.getX()[i])-inf(dom.getX()[i]) for i in range(self._dim)]
        self._dom_NE=[int(self._dom_len[i]/self._spacing[i]) for i in range(self._dim)]

        self.logger.debug("Domain size: "+str(self._dom_NE))
        self.logger.debug("     length: "+str(self._dom_len))
        self.logger.debug("     origin: "+str(self._dom_origin))
        return dom

