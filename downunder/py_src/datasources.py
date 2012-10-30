
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

"""Data readers/providers for inversions"""

__copyright__="""Copyright (c) 2003-2012 by University of Queensland
http://www.uq.edu.au
Primary Business: Queensland, Australia"""
__license__="""Licensed under the Open Software License version 3.0
http://www.opensource.org/licenses/osl-3.0.php"""
__url__="https://launchpad.net/escript-finley"

__all__ = ['DataSource','UBCDataSource','ERSDataSource','SyntheticDataSource','SmoothAnomaly']

import logging
import numpy as np
from esys.escript import ReducedFunction, Scalar
from esys.escript.linearPDEs import LinearSinglePDE
from esys.escript.util import *
import esys.escript.unitsSI as U
from esys.ripley import Brick, Rectangle, ripleycpp

try:
    from scipy.io.netcdf import netcdf_file
    __all__ += ['NetCDFDataSource']
except:
    pass

def LatLonToUTM(lon, lat, wkt_string=None):
    """
    Converts one or more longitude,latitude pairs to the corresponding x,y
    coordinates in the Universal Transverse Mercator projection.

    :note: If the ``pyproj`` module is not installed a warning is printed and
           the input values are scaled by a constant and returned.
    :note: If `wkt_string` is not given or invalid or the ``gdal`` module is
           not available to convert the string, then the input values are
           assumed to be given in the Clarke 1866 projection.

    :param lon: longitude value(s)
    :type lon: `float`, `list`, `tuple`, or ``numpy.array``
    :param lat: latitude value(s)
    :type lat: `float`, `list`, `tuple`, or ``numpy.array``
    :rtype: ``numpy.array``
    """

    # not really optimal: if pyproj is not installed we return the input
    # values scaled by a constant.
    try:
        import pyproj
    except:
        print("Warning, pyproj not available. Domain extents will be wrong")
        return np.array(lon)*1000., np.array(lat)*1000.

    # determine UTM zone from the input data
    zone=int(np.median((np.floor((np.array(lon) + 180)/6) + 1) % 60))
    try:
        import osgeo.osr
        srs = osgeo.osr.SpatialReference()
        srs.ImportFromWkt(wkt_string)
        p_src = pyproj.Proj(srs.ExportToProj4())
    except:
        p_src = pyproj.Proj('+proj=longlat +ellps=clrk66 +no_defs')
    # we assume southern hemisphere here
    p_dest = pyproj.Proj('+proj=utm +zone=%d +south +units=m'%zone)
    x,y=pyproj.transform(p_src, p_dest, lon, lat)
    return x,y

class DataSource(object):
    """
    A class that provides survey data for the inversion process.
    This is an abstract base class that implements common functionality.
    Methods to be overwritten by subclasses are marked as such.
    This class assumes 2D data which is mapped to a slice of a 3D domain.
    For other setups override the `_createDomain` method.
    """

    def __init__(self):
        """
        Constructor. Sets some defaults and initializes logger.
        """
        self._constrainBottom=False
        self._constrainSides=True
        self._domain=None
        self.__set_density_mask=None
        self.__set_susceptibility_mask=None
        self.setPadding()
        self.logger = logging.getLogger('inv.%s'%self.__class__.__name__)

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
        self._pad_x=pad_x
        self._pad_y=pad_y

    def setConstraints(self, bottom=False, sides=True):
        """
        If `bottom` is True, then the density mask will be set to 1 in the
        padding area at the bottom of the domain. By default this area is
        unconstrained. Similarly, if `sides` is True (default) then the
        horizontal padding area is constrained, otherwise not.

        :param bottom: Whether to constrain the density at the bottom of the
                       domain
        :type bottom: ``bool``
        :param sides: Whether to constrain the density in the padding area
                      surrounding the data
        :type sides: ``bool``
        """
        self._constrainBottom=bottom
        self._constrainSides=sides

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
            self._domain=self._createDomain(self._pad_x, self._pad_y)
        return self._domain

    def getSetDensityMask(self):
        """
        Returns the density mask data object, where mask has value 1 in the
        padding area, 0 elsewhere.

        :return: The mask for the density.
        :rtype: `esys.escript.Data`
        """
        return self.__set_density_mask

    def setSetDensityMask(self, mask):
        """
        Sets the density mask to use.
        """
        self.__set_density_mask=mask

    def getSetSusceptibilityMask(self):
        """
        Returns the susceptibility mask data object, where mask has value 1
        in the padding area, 0 elsewhere.

        :return: The mask for the susceptibility.
        :rtype: `esys.escript.Data`
        """
        return self.__set_susceptibility_mask

    def setSetSusceptibilityMask(self, mask):
        """
        Sets the susceptibility mask to use.
        """
        self.__set_susceptibility_mask=mask

    def getGravityAndStdDev(self):
        """
        Returns the gravity anomaly and standard deviation data objects as a
        tuple. This method must be implemented in subclasses that supply
        gravity data.
        """
        raise NotImplementedError

    def getMagneticFieldAndStdDev(self):
        """
        Returns the magnetic field and standard deviation data objects as a
        tuple. This method must be implemented in subclasses that supply
        magnetic data.
        """
        raise NotImplementedError

    def getBackgroundMagneticField(self):
        """
        Returns the background magnetic field. This method must be
        implemented in subclasses that supply magnetic data.
        """
        return NotImplementedError

    def getDataExtents(self):
        """
        returns a tuple of tuples ``( (x0, y0), (nx, ny), (dx, dy) )``, where

        - ``x0``, ``y0`` = coordinates of data origin
        - ``nx``, ``ny`` = number of data points in x and y
        - ``dx``, ``dy`` = spacing of data points in x and y

        This method must be implemented in subclasses.
        """
        raise NotImplementedError

    def getVerticalExtents(self):
        """
        returns a tuple ``(z0, nz, dz)``, where

        - ``z0`` = minimum z coordinate (origin)
        - ``nz`` = number of elements in z direction
        - ``dz`` = spacing of elements (= cell size in z)

        This method must be implemented in subclasses.
        """
        raise NotImplementedError

    def _addPadding(self, pad_x, pad_y, NE, l, origin):
        """
        Helper method that computes new number of elements, length and origin
        after adding padding to the input values.

        :param pad_x: Number of elements or fraction of padding in x direction
        :type pad_x: ``int`` or ``float``
        :param pad_y: Number of elements or fraction of padding in y direction
        :type pad_y: ``int`` or ``float``
        :param NE: Initial number of elements
        :type NE: ``tuple`` or ``list``
        :param l: Initial side lengths
        :type l: ``tuple`` or ``list``
        :param origin: Initial origin
        :type origin: ``tuple`` or ``list``
        :return: tuple with three elements ``(NE_padded, l_padded, origin_padded)``,
                 which are lists of the updated input parameters
        """
        DIM=len(NE)
        frac=[0.]*(DIM-1)+[0]
        # padding is applied to each side so multiply by 2 to get the total
        # amount of padding per dimension
        if pad_x>0 and pad_y<1:
            frac[0]=2.*pad_x
        elif pad_x>=1:
            frac[0]=2.*pad_x/float(NE[0])
        if DIM>2:
            if pad_y>0 and pad_y<1:
                frac[1]=2.*pad_y
            elif pad_y>=1:
                frac[1]=2.*pad_y/(float(NE[1]))

        # calculate new number of elements
        NE_new=[int(NE[i]*(1+frac[i])) for i in range(DIM)]
        NEdiff=[NE_new[i]-NE[i] for i in range(DIM)]
        spacing=[l[i]/NE[i] for i in range(DIM)]
        l_new=[NE_new[i]*spacing[i] for i in range(DIM)]
        origin_new=[origin[i]-NEdiff[i]/2.*spacing[i] for i in range(DIM)]
        return NE_new, l_new, origin_new

    def _interpolateOnDomain(self, data):
        """
        Helper method that interpolates data arrays onto the domain.
        Currently this works like a nearest neighbour mapping, i.e. values
        are directly inserted into data objects at closest location.
        """
        dom=self.getDomain()
        dim=dom.getDim()
        # determine number of values required per element
        DPP=Scalar(0., ReducedFunction(dom)).getNumberOfDataPoints()
        for i in range(dim):
            DPP=DPP/self._dom_NE[i]
        DPP=int(DPP)

        # idx_mult.dot([x,y,z]) = flat index into data object
        idx_mult=np.array([DPP]+self._dom_NE[:dim-1]).cumprod()

        # separate data arrays and coordinates
        num_arrays=len(data[0])-dim
        arrays=[]
        for i in range(num_arrays):
            d=Scalar(0., ReducedFunction(dom))
            d.expand()
            arrays.append(d)

        for entry in data:
            index=[int((entry[i]-self._dom_origin[i])/self._spacing[i]) for i in range(dim)]
            index=int(idx_mult.dot(index))
            for i in range(num_arrays):
                for p in range(DPP):
                    arrays[i].setValueOfDataPoint(index+p, entry[dim+i])

        return arrays

    def _createDomain(self, padding_x, padding_y):
        """
        Creates and returns an escript domain that spans the entire area of
        available data plus a buffer zone. This method is called only once
        the first time `getDomain()` is invoked and may be overwritten if
        required.

        :return: The escript domain for this data source.
        :rtype: `esys.escript.Domain`
        """
        X0, NX, DX = self.getDataExtents()
        z0, nz, dz = self.getVerticalExtents()

        # number of elements (without padding)
        NE = [NX[0], NX[1], nz]

        # origin of domain (without padding)
        origin = [X0[0], X0[1], z0]
        origin = [np.round(oi) for oi in origin]

        # cell size / point spacing
        self._spacing = DX+[dz]
        self._spacing = [float(np.round(si)) for si in self._spacing]

        # length of domain (without padding)
        l = [NE[i]*self._spacing[i] for i in range(len(NE))]

        # now add padding to the values
        NE_new, l_new, origin_new = self._addPadding(padding_x, padding_y, \
                NE, l, origin)

        # number of padding elements per side
        NE_pad=[(NE_new[i]-NE[i])//2 for i in range(3)]

        self._dom_NE_pad = NE_pad
        self._dom_len = l_new
        self._dom_NE = NE_new
        self._dom_origin = origin_new
        lo=[(origin_new[i], origin_new[i]+l_new[i]) for i in range(3)]
        dom=Brick(*self._dom_NE, l0=lo[0], l1=lo[1], l2=lo[2])
        # ripley may internally adjust NE and length, so recompute
        self._dom_len=[sup(dom.getX()[i])-inf(dom.getX()[i]) for i in range(3)]
        self._dom_NE=[int(self._dom_len[i]/self._spacing[i]) for i in range(3)]
        x=dom.getX()-[self._dom_origin[i]+NE_pad[i]*self._spacing[i] for i in range(3)]
        mask=wherePositive(dom.getX()[2])

        # prepare density mask (=1 at padding area, 0 else)
        if self._constrainSides:
            for i in range(2):
                mask=mask + whereNegative(x[i]) + \
                        wherePositive(x[i]-l_new[i]+2*NE_pad[i]*self._spacing[i])

        if self._constrainBottom:
            mask = mask + whereNonPositive(x[2])
        self.setSetDensityMask(wherePositive(mask))

        self.logger.debug("Domain size: %d x %d x %d elements"%(self._dom_NE[0],self._dom_NE[1],self._dom_NE[2]))
        self.logger.debug("     length: %g x %g x %g"%(self._dom_len[0],self._dom_len[1],self._dom_len[2]))
        self.logger.debug("     origin: %g x %g x %g"%(origin_new[0],origin_new[1],origin_new[2]))

        return dom


##############################################################################
class UBCDataSource(DataSource):
    def __init__(self, meshfile, gravfile, topofile=None):
        super(UBCDataSource,self).__init__()
        self.__meshfile=meshfile
        self.__gravfile=gravfile
        self.__topofile=topofile
        self.__readMesh()

    def __readMesh(self):
        meshdata=open(self.__meshfile).readlines()
        numDataPoints=meshdata[0].split()
        origin=meshdata[1].split()
        self.__nPts=map(int, numDataPoints)
        self.__origin=map(float, origin)
        self.__delta=[float(X.split('*')[1]) for X in meshdata[2:]]
        # vertical data is upside down
        self.__origin[2]-=(self.__nPts[2]-1)*self.__delta[2]
        self.logger.debug("Data Source: %s (mesh file: %s)"%(self.__gravfile, self.__meshfile))

    def getDataExtents(self):
        """
        returns ( (x0, y0), (nx, ny), (dx, dy) )
        """
        return (self.__origin[:2], self.__nPts[:2], self.__delta[:2])

    def getVerticalExtents(self):
        """
        returns (z0, nz, dz)
        """
        return (self.__origin[2], self.__nPts[2], self.__delta[2])

    #def getSetDensityMask(self):
    #    topodata=self.__readTopography()
    #    mask=self._interpolateOnDomain(topodata)
    #    mask=wherePositive(self.getDomain().getX()[2]-mask[0])
    #    return mask

    def getGravityAndStdDev(self):
        gravlist=self.__readGravity() # x,y,z,g,s
        g_and_sigma=self._interpolateOnDomain(gravlist)
        return g_and_sigma[0]*[0,0,1], g_and_sigma[1]

    def __readTopography(self):
        f=open(self.__topofile)
        n=int(f.readline())
        topodata=np.zeros((n,3))
        for i in range(n):
            x=f.readline().split()
            x=map(float, x)
            topodata[i]=x
        f.close()
        return topodata

    def __readGravity(self):
        f=open(self.__gravfile)
        n=int(f.readline())
        gravdata=np.zeros((n,5))
        for i in range(n):
            x=f.readline().split()
            x=map(float, x) # x, y, z, anomaly in mGal, stddev
            # convert gravity anomaly units to m/s^2 and rescale error
            x[3]*=-1e-5
            x[4]*=1e-5
            gravdata[i]=x
        f.close()
        return gravdata

##############################################################################
class NetCDFDataSource(DataSource):
    """
    Data Source for gridded netCDF data that use CF/COARDS conventions.
    """
    def __init__(self, gravfile=None, magfile=None, topofile=None,
                       depth=40000.,
                       air_layer=10000.,
                       vertical_cells=25,
                       alt_of_data=0.):
        """
        :param gravfile: file with gravity data in netcdf format
        :type gravfile: ```str```
        :param magfile: file with magnetic datafile in netcdf format
        :type magfile: ```str```
        :param topofile: file with topography data in netcdf format
        :type topofile: ```str```
        :param depth: depth below surface filled with rock
        :type depth: ```float```
        :param air_layer: height above surface filled with air. It is assumed that that density correction is zero
                       in this region.
        :type air_layer: ```float```
        :param vertical_cells: number of vertical cells
        type vertical_cells: ```int```
        :param alt_of_data: altitude of measurements above ground in meter
        :type alt_of_data: ```float```
        """
        super(NetCDFDataSource,self).__init__()
        self.__topofile=topofile
        self.__gravfile=gravfile
        self.__magfile=magfile
        self.__determineExtents((-depth,air_layer,n))
        self.__altOfData=alt_of_data

    def __determineExtents(self, ve):
        self.logger.debug("Data Source: %s"%self.__gravfile)
        f=netcdf_file(self.__gravfile, 'r')
        NX=0
        for n in ['lon','longitude','x']:
            if n in f.dimensions:
                NX=f.dimensions[n]
                break
        if NX==0:
            raise RuntimeError("Could not determine extents of data")
        NY=0
        for n in ['lat','latitude','y']:
            if n in f.dimensions:
                NY=f.dimensions[n]
                break
        if NY==0:
            raise RuntimeError("Could not determine extents of data")

        # find longitude and latitude variables
        lon_name=None
        for n in ['lon','longitude']:
            if n in f.variables:
                lon_name=n
                longitude=f.variables.pop(n)
                break
        if lon_name is None:
            raise RuntimeError("Could not determine longitude variable")
        lat_name=None
        for n in ['lat','latitude']:
            if n in f.variables:
                lat_name=n
                latitude=f.variables.pop(n)
                break
        if lat_name is None:
            raise RuntimeError("Could not determine latitude variable")

        # try to figure out gravity variable name
        grav_name=None
        if len(f.variables)==1:
            grav_name=f.variables.keys()[0]
        else:
            for n in f.variables.keys():
                dims=f.variables[n].dimensions
                if (lat_name in dims) and (lon_name in dims):
                    grav_name=n
                    break
        if grav_name is None:
            raise RuntimeError("Could not determine gravity variable")

        # try to determine value for unused data
        if hasattr(f.variables[grav_name], 'missing_value'):
            maskval = float(f.variables[grav_name].missing_value)
        elif hasattr(f.variables[grav_name], '_FillValue'):
            maskval = float(f.variables[grav_name]._FillValue)
        else:
            self.logger.debug("missing_value attribute not found, using default.")
            maskval = 99999

        # see if there is a wkt string to convert coordinates
        try:
            wkt_string=f.variables[grav_name].esri_pe_string
        except:
            wkt_string=None

        # we don't trust actual_range & geospatial_lon_min/max since subset
        # data does not seem to have these fields updated.
        # Getting min/max from the arrays is obviously not very efficient but..
        #lon_range=longitude.actual_range
        #lat_range=latitude.actual_range
        #lon_range=[f.geospatial_lon_min,f.geospatial_lon_max]
        #lat_range=[f.geospatial_lat_min,f.geospatial_lat_max]
        lon_range=longitude.data.min(),longitude.data.max()
        lat_range=latitude.data.min(),latitude.data.max()
        lon_range,lat_range=LatLonToUTM(lon_range, lat_range, wkt_string)
        origin=[lon_range[0],lat_range[0],ve[0]]
        lengths=[lon_range[1]-lon_range[0], lat_range[1]-lat_range[0],ve[1]-ve[0]]

        f.close()

        self.__nPts=[NX, NY, ve[2]]
        self.__origin=origin
        # we are rounding to avoid interpolation issues
        self.__delta=[np.round(lengths[i]/self.__nPts[i]) for i in range(3)]
        self.__wkt_string=wkt_string
        self.__lon=lon_name
        self.__lat=lat_name
        self.__grv=grav_name
        self.__maskval=maskval

    def getDataExtents(self):
        """
        returns ( (x0, y0), (nx, ny), (dx, dy) )
        """
        return (self.__origin[:2], self.__nPts[:2], self.__delta[:2])

    def getVerticalExtents(self):
        """
        returns (z0, nz, dz)
        """
        return (self.__origin[2], self.__nPts[2], self.__delta[2])

    def getGravityAndStdDev(self):
        nValues=self.__nPts[:2]+[1]
        first=self._dom_NE_pad[:2]+[self._dom_NE_pad[2]+int((self.__altOfData-self.__origin[2])/self.__delta[2])]
        g=ripleycpp._readNcGrid(self.__gravfile, self.__grv,
                ReducedFunction(self.getDomain()),
                first, nValues, (), self.__maskval)
        sigma=whereNonZero(g-self.__maskval)
        g=g*1e-6
        sigma=sigma*2e-6
        return g*[0,0,1], sigma

    def _readTopography(self):
        f=netcdf_file(self.__topofile, 'r')
        lon=None
        for n in ['lon','longitude']:
            if n in f.variables:
                lon=f.variables[n][:]
                break
        if lon is None:
            raise RuntimeError("Could not determine longitude variable")
        lat=None
        for n in ['lat','latitude']:
            if n in f.variables:
                lat=f.variables[n][:]
                break
        if lat is None:
            raise RuntimeError("Could not determine latitude variable")
        alt=None
        for n in ['altitude','alt']:
            if n in f.variables:
                alt=f.variables[n][:]
                break
        if alt is None:
            raise RuntimeError("Could not determine altitude variable")

        topodata=np.column_stack((lon,lat,alt))
        f.close()
        return topodata

##############################################################################
class ERSDataSource(DataSource):
    """
    Data Source for ER Mapper raster data.
    Note that this class only accepts a very specific type of ER Mapper data
    input and will raise an exception if other data is found.
    """
    def __init__(self, headerfile, datafile=None, vertical_extents=(-40000,10000,25), alt_of_data=0.):
        """
        headerfile - usually ends in .ers
        datafile - usually has the same name as the headerfile without '.ers'
        """
        super(ERSDataSource,self).__init__()
        self.__headerfile=headerfile
        if datafile is None:
            self.__datafile=headerfile[:-4]
        else:
            self.__datafile=datafile
        self.__readHeader(vertical_extents)
        self.__altOfData=alt_of_data

    def __readHeader(self, ve):
        self.logger.debug("Data Source: %s (header: %s)"%(self.__datafile, self.__headerfile))
        metadata=open(self.__headerfile, 'r').readlines()
        # parse metadata
        start=-1
        for i in range(len(metadata)):
            if metadata[i].strip() == 'DatasetHeader Begin':
                start=i+1
        if start==-1:
            raise RuntimeError('Invalid ERS file (DatasetHeader not found)')

        md_dict={}
        section=[]
        for i in range(start, len(metadata)):
            line=metadata[i]
            if line[-6:].strip() == 'Begin':
                section.append(line[:-6].strip())
            elif line[-4:].strip() == 'End':
                if len(section)>0:
                    section.pop()
            else:
                vals=line.split('=')
                if len(vals)==2:
                    key = vals[0].strip()
                    value = vals[1].strip()
                    fullkey='.'.join(section+[key])
                    md_dict[fullkey]=value

        try:
            if md_dict['RasterInfo.CellType'] != 'IEEE4ByteReal':
                raise RuntimeError('Unsupported data type '+md_dict['RasterInfo.CellType'])
        except KeyError:
            self.logger.warn("Cell type not specified. Assuming IEEE4ByteReal.")

        try:
            NX = int(md_dict['RasterInfo.NrOfCellsPerLine'])
            NY = int(md_dict['RasterInfo.NrOfLines'])
        except:
            raise RuntimeError("Could not determine extents of data")

        try:
            maskval = float(md_dict['RasterInfo.NullCellValue'])
        except:
            maskval = 99999

        try:
            spacingX = float(md_dict['RasterInfo.CellInfo.Xdimension'])
            spacingY = float(md_dict['RasterInfo.CellInfo.Ydimension'])
        except:
            raise RuntimeError("Could not determine cell dimensions")

        try:
            if md_dict['CoordinateSpace.CoordinateType']=='EN':
                originX = float(md_dict['RasterInfo.RegistrationCoord.Eastings'])
                originY = float(md_dict['RasterInfo.RegistrationCoord.Northings'])
            elif md_dict['CoordinateSpace.CoordinateType']=='LL':
                originX = float(md_dict['RasterInfo.RegistrationCoord.Longitude'])
                originY = float(md_dict['RasterInfo.RegistrationCoord.Latitude'])
            else:
                raise RuntimeError("Unknown CoordinateType")
        except:
            self.logger.warn("Could not determine coordinate origin. Setting to (0.0, 0.0)")
            originX,originY = 0.0, 0.0

        if 'GEODETIC' in md_dict['CoordinateSpace.Projection']:
            # it appears we have lat/lon coordinates so need to convert
            # origin and spacing. Try using gdal to get the wkt if available:
            try:
                from osgeo import gdal
                ds=gdal.Open(self.__headerfile)
                wkt=ds.GetProjection()
            except:
                wkt='GEOGCS["GEOCENTRIC DATUM of AUSTRALIA",DATUM["GDA94",SPHEROID["GRS80",6378137,298.257222101]],PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433]]'
                self.logger.warn('GDAL not available or file read error, assuming GDA94 data')
            originX_UTM,originY_UTM=LatLonToUTM(originX, originY, wkt)
            op1X,op1Y=LatLonToUTM(originX+spacingX, originY+spacingY, wkt)
            # we are rounding to avoid interpolation issues
            spacingX=np.round(op1X-originX_UTM)
            spacingY=np.round(op1Y-originY_UTM)
            originX=np.round(originX_UTM)
            originY=np.round(originY_UTM)

        # data sets have origin in top-left corner so y runs top-down
        self.__dataorigin=[originX, originY]
        originY-=(NY-1)*spacingY
        self.__maskval=maskval
        spacingZ=np.round(float(ve[1]-ve[0])/ve[2])
        self.__delta = [spacingX, spacingY, spacingZ]
        self.__nPts = [NX, NY, ve[2]]
        self.__origin = [originX, originY, ve[0]]

    def getDataExtents(self):
        """
        returns ( (x0, y0), (nx, ny), (dx, dy) )
        """
        return (self.__origin[:2], self.__nPts[:2], self.__delta[:2])

    def getVerticalExtents(self):
        """
        returns (z0, nz, dz)
        """
        return (self.__origin[2], self.__nPts[2], self.__delta[2])

    def getGravityAndStdDev(self):
        nValues=self.__nPts[:2]+[1]
        first=self._dom_NE_pad[:2]+[self._dom_NE_pad[2]+int((self.__altOfData-self.__origin[2])/self.__delta[2])]
        g=ripleycpp._readBinaryGrid(self.__datafile,
                ReducedFunction(self.getDomain()),
                first, nValues, (), self.__maskval)
        sigma=whereNonZero(g-self.__maskval)
        g=g*1e-6
        sigma=sigma*2e-6
        return g*[0,0,1], sigma


##############################################################################
class SourceFeature(object):
    """
    A feature adds a density distribution to (parts of) a domain of a synthetic
    data source, for example a layer of a specific rock type or a simulated
    ore body.
    """
    def getDensity(self):
        """
        Returns the density for the area covered by mask. It can be constant
        or a data object with spatial dependency.
        """
        raise NotImplementedError
    def getMask(self, x):
        """
        Returns the mask of the area of interest for this feature. That is,
        mask is non-zero where the density returned by getDensity() should be
        applied, zero elsewhere.
        """
        raise NotImplementedError

class SmoothAnomaly(SourceFeature):
    def __init__(self, lx, ly, lz, x, y, depth, rho_inner=None, rho_outer=None, k_inner=None, k_outer=None):
        self.x=x
        self.y=y
        self.lx=lx
        self.ly=ly
        self.lz=lz
        self.depth=depth
        self.rho_inner=rho_inner
        self.rho_outer=rho_outer
        self.k_inner=k_inner
        self.k_outer=k_outer
        self.rho=None
        self.k=None
        self.mask=None

    def getDensity(self,x):
        if self.rho is None:
            if self.rho_outer is None or self.rho_inner is None:
                self.rho=0
            else:
                DIM=x.getDomain().getDim()
                alpha=-log(abs(self.rho_outer/self.rho_inner))*4
                rho=exp(-alpha*((x[0]-self.x)/self.lx)**2)
                rho=rho*exp(-alpha*((x[DIM-1]-(sup(x[DIM-1])-self.depth))/self.lz)**2)
                self.rho=maximum(abs(self.rho_outer), abs(self.rho_inner*rho))
                if self.rho_inner<0: self.rho=-self.rho

        return self.rho

    def getSusceptibility(self,x):
         if self.k is None:
            if self.k_outer is None or self.k_inner is None:
                self.k=0
            else:
                DIM=x.getDomain().getDim()
                alpha=-log(abs(self.k_outer/self.k_inner))*4
                k=exp(-alpha*((x[0]-self.x)/self.lx)**2)
                k=k*exp(-alpha*((x[DIM-1]-(sup(x[DIM-1])-self.depth))/self.lz)**2)
                self.k=maximum(abs(self.k_outer), abs(self.k_inner*k))
                if self.k_inner<0: self.k=-self.k

         return self.k

    def getMask(self, x):
        DIM=x.getDomain().getDim()
        m=whereNonNegative(x[DIM-1]-(sup(x[DIM-1])-self.depth-self.lz/2)) * whereNonPositive(x[DIM-1]-(sup(x[DIM-1])-self.depth+self.lz/2)) \
            *whereNonNegative(x[0]-(self.x-self.lx/2)) * whereNonPositive(x[0]-(self.x+self.lx/2))
        if DIM>2:
            m*=whereNonNegative(x[1]-(self.y-self.ly/2)) * whereNonPositive(x[1]-(self.y+self.ly/2))
        self.mask = m
        return m

##############################################################################
class SyntheticDataSource(DataSource):
    def __init__(self, DIM, NE, l, h, features, latitude=-32.):
        super(SyntheticDataSource,self).__init__()
        self._features = features
        self.DIM=DIM
        self.NE=NE
        self.l=l
        self.h=h
        self.latitude=latitude

    def _createDomain(self, padding_x, padding_y):
        NE_H=self.NE
        NE_L=int((self.l/self.h)*NE_H+0.5)
        l=[self.l]*(self.DIM-1)+[self.h]
        NE=[NE_L]*(self.DIM-1)+[NE_H]
        origin=[0.]*self.DIM
        NE_new, l_new, origin_new = self._addPadding(padding_x, padding_y, \
                NE, l, origin)

        self.NE=NE_new
        self.l=l_new[0]
        self.h=l_new[self.DIM-1]

        self.logger.debug("Data Source: synthetic with %d features"%len(self._features))
        if self.DIM==2:
            dom = Rectangle(n0=NE_new[0], n1=NE_new[1], l0=l_new[0], l1=l_new[1])
            self._x = dom.getX() + origin_new
            self.logger.debug("Domain size: %d x %d elements"%(NE_new[0], NE_new[1]))
            self.logger.debug("     length: %g x %g"%(l_new[0],l_new[1]))
            self.logger.debug("     origin: %g x %g"%(origin_new[0],origin_new[1]))
        else:
            dom = Brick(n0=NE_new[0], n1=NE_new[1], n2=NE_new[2], l0=l_new[0], l1=l_new[1], l2=l_new[2])
            self._x = dom.getX() + origin_new
            self.logger.debug("Domain size: %d x %d x %d elements"%(self.NE[0],self.NE[1],self.NE[2]))
            self.logger.debug("     length: %g x %g x %g"%(l_new[0],l_new[1],l_new[2]))
            self.logger.debug("     origin: %g x %g x %g"%(origin_new[0],origin_new[1],origin_new[2]))

        dz=l_new[self.DIM-1]/NE_new[self.DIM-1]
        self._g_mask=wherePositive(dom.getX()[0]-origin_new[0]) \
                * whereNegative(dom.getX()[0]-(l_new[0]-origin_new[0])) \
                * whereNonNegative(dom.getX()[self.DIM-1]-(l_new[self.DIM-1]+origin_new[self.DIM-1])) \
                * whereNonPositive(dom.getX()[self.DIM-1]-(l_new[self.DIM-1]+(origin_new[self.DIM-1]+dz)))

        self._B_mask=self._g_mask

        mask=whereNegative(self._x[self.DIM-1]) + \
                wherePositive(self._x[self.DIM-1]-l[self.DIM-1])
        for i in range(self.DIM-1):
            mask+= whereNegative(self._x[i]) +  wherePositive(self._x[i]-l[i])
        self.setSetDensityMask(wherePositive(mask))
        self.setSetSusceptibilityMask(wherePositive(mask))

        rho_ref=0.
        k_ref=0
        for f in self._features:
            m=f.getMask(self._x)
            rho_ref = rho_ref * (1-m) + f.getDensity(self._x) * m
            k_ref = k_ref * (1-m) + f.getSusceptibility(self._x) * m
        self._rho=rho_ref
        self._k=k_ref

        return dom


    def getReferenceDensity(self):
        return self._rho
    def getReferenceSusceptibility(self):
        return self._k

    def getGravityAndStdDev(self):
        pde=LinearSinglePDE(self.getDomain())
        G=U.Gravitational_Constant
        m_psi_ref=0.
        for i in range(self.DIM):
            m_psi_ref=m_psi_ref + whereZero(self._x[i]-inf(self._x[i])) \
                    + whereZero(self._x[i]-sup(self._x[i]))

        pde.setValue(A=kronecker(self.getDomain()), Y=-4*np.pi*G*self._rho, q=m_psi_ref)
        pde.setSymmetryOn()
        psi_ref=pde.getSolution()
        del pde
        g=-grad(psi_ref)
        sigma=self._g_mask
        return g,sigma

    def getMagneticFieldAndStdDev(self):
        pde=LinearSinglePDE(self.getDomain())
        B_b=self.getBackgroundMagneticField()
        DIM=self.getDomain().getDim()
        m_psi_ref=0.
        for i in range(self.DIM):
            m_psi_ref=m_psi_ref + whereZero(self._x[i]-inf(self._x[i])) \
                    + whereZero(self._x[i]-sup(self._x[i]))
        pde.setValue(A=kronecker(self.getDomain()), X=(1+self._k)*B_b, q=m_psi_ref)
        pde.setSymmetryOn()
        psi_ref=pde.getSolution()
        del pde
        B= (1+self._k) * B_b -grad(psi_ref)
        sigma=self._B_mask
        return B,sigma

    def getBackgroundMagneticField(self):
        theta = (90-self.latitude)/180.*np.pi
        B_0=U.Mu_0  * U.Magnetic_Dipole_Moment_Earth / (4 * np.pi *  U.R_Earth**3)
        B_theta= B_0 * sin(theta)
        B_r= 2 * B_0 * cos(theta)
        DIM=self.getDomain().getDim()
        if DIM<3:
            return np.array([0.,  -B_r])
        else:
            return np.array([-B_theta, 0.,  -B_r])
