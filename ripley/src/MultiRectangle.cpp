
/*****************************************************************************
*
* Copyright (c) 2003-2015 by University of Queensland
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

#define ESNEEDPYTHON
#include "esysUtils/first.h"

#include <ripley/MultiRectangle.h>
#include <ripley/blocktools.h>
#include <ripley/domainhelpers.h>
#include <paso/SystemMatrix.h>
#include <esysUtils/esysFileWriter.h>
#include <esysUtils/EsysRandom.h>
#include <escript/DataFactory.h>
#include <escript/FunctionSpaceFactory.h>
#include <boost/scoped_array.hpp>
#include <boost/math/special_functions/fpclassify.hpp>	// for isnan

#ifdef USE_NETCDF
#include <netcdfcpp.h>
#endif

#if USE_SILO
#include <silo.h>
#ifdef ESYS_MPI
#include <pmpio.h>
#endif
#endif

#define FIRST_QUAD 0.21132486540518711775
#define SECOND_QUAD 0.78867513459481288225

#include <iomanip>
#include <limits>


#include <algorithm>

namespace bp = boost::python;
using esysUtils::FileWriter;
using escript::AbstractSystemMatrix;

using boost::math::isnan;
using std::vector;
using std::string;
using std::min;
using std::max;
using std::copy;
using std::ios;
using std::fill;

namespace ripley {

MultiRectangle::MultiRectangle(dim_t n0, dim_t n1, double x0, double y0,
                     double x1, double y1, int d0, int d1,
                     const vector<double>& points,
                     const vector<int>& tags,
                     const TagMap& tagnamestonums,
                     escript::SubWorld_ptr w, unsigned int subdivisions)
     : Rectangle(n0,n1, x0,y0, x1,y1, d0,d1, points, tags, tagnamestonums, w),
       m_subdivisions(subdivisions)
{
    if (m_mpiInfo->size != 1)
        throw RipleyException("Multiresolution domains don't currently support multiple processes");

    if (subdivisions == 0 || (subdivisions & (subdivisions - 1)) != 0)
        throw RipleyException("Element subdivisions must be a power of two");

    dim_t oldNN[2] = {0};

    if (d0 == 0 || d1 == 0)
        throw RipleyException("Domain subdivisions must be positive");

    for (int i = 0; i < 2; i++) {
        m_NE[i] *= subdivisions;
        oldNN[i] = m_NN[i];
        m_NN[i] = m_NE[i] + 1;
        m_gNE[i] *= subdivisions;
        m_ownNE[i] *= subdivisions;
        m_dx[i] /= subdivisions;
        m_faceCount[i] *= subdivisions;
        m_faceCount[2+i] *= subdivisions;
    }

    // bottom-left node is at (offset0,offset1) in global mesh
    m_offset[0] = m_gNE[0]*subdivisions/d0*(m_mpiInfo->rank%d0);
    m_offset[1] = m_gNE[0]*subdivisions/d1*(m_mpiInfo->rank/d0);
    
    populateSampleIds();
    
    const dim_t nDirac = m_diracPoints.size();
#pragma omp parallel for
    for (int i = 0; i < nDirac; i++) {
        const dim_t node = m_diracPoints[i].node;
        const dim_t x = node % oldNN[0];
        const dim_t y = node / oldNN[0];
        m_diracPoints[i].node = INDEX2(x*subdivisions, y*subdivisions, m_NN[0]);
        m_diracPointNodeIDs[i] = m_diracPoints[i].node;
    }
}

MultiRectangle::~MultiRectangle()
{
}

void MultiRectangle::validateInterpolationAcross(int fsType_source,
        const escript::AbstractDomain& domain, int fsType_target) const
{
    const MultiRectangle *other = dynamic_cast<const MultiRectangle *>(&domain);
    if (other == NULL)
        throw RipleyException("Invalid interpolation: domains must both be instances of MultiRectangle");

    const double *len = other->getLength();
    const int *subdivs = other->getNumSubdivisionsPerDim();
    const int *elements = other->getNumElementsPerDim();
    const unsigned int level = other->getNumSubdivisionsPerElement();
    const unsigned int factor = m_subdivisions > level ? m_subdivisions/level : level/m_subdivisions;
    if ((factor & (factor - 1)) != 0) //factor == 2**x
        throw RipleyException("Invalid interpolation: elemental subdivisions of each domain must be powers of two");

    if (other->getMPIComm() != m_mpiInfo->comm)
        throw RipleyException("Invalid interpolation: Domains are on different communicators");

    for (int i = 0; i < m_numDim; i++) {
        if (m_length[i] != len[i])
            throw RipleyException("Invalid interpolation: domain length mismatch");
        if (m_NX[i] != subdivs[i])
            throw RipleyException("Invalid interpolation: domain process subdivision mismatch");
        if (m_subdivisions > level) {
            if (m_ownNE[i]/elements[i] != factor)
                throw RipleyException("Invalid interpolation: element factor mismatch");
        } else {
            if (elements[i]/m_ownNE[i] != factor)
                throw RipleyException("Invalid interpolation: element factor mismatch");
        }
    }
}

void MultiRectangle::interpolateNodesToNodesFiner(const escript::Data& source,
        escript::Data& target, const MultiRectangle& other) const
{
    const int scaling = other.getNumSubdivisionsPerElement()/m_subdivisions;
    const dim_t NN0 = m_NN[0], NN1 = m_NN[1], otherNN0 = other.getNumNodesPerDim()[0];
    const dim_t numComp = source.getDataPointSize();
#pragma omp parallel for
    for (dim_t ny = 0; ny < NN1 - 1; ny++) { //source nodes
        for (dim_t nx = 0; nx < NN0 - 1; nx++) {
            const double *x0y0 = source.getSampleDataRO(ny*NN0 + nx);
            const double *x0y1 = source.getSampleDataRO((ny+1)*NN0 + nx);
            const double *x1y0 = source.getSampleDataRO(ny*NN0 + nx + 1);
            const double *x1y1 = source.getSampleDataRO((ny+1)*NN0 + nx + 1);
            const double origin[2] = {getLocalCoordinate(nx, 0), getLocalCoordinate(ny, 1)};
            for (int sy = 0; sy < scaling + 1; sy++) { //target nodes
                const double y = (other.getLocalCoordinate(ny*scaling+sy, 1) - origin[1]) / m_dx[1];
                for (int sx = 0; sx < scaling + 1; sx++) {
                    const double x = (other.getLocalCoordinate(nx*scaling+sx, 0) - origin[0]) / m_dx[0];
                    double *out = target.getSampleDataRW(nx*scaling+sx + (ny*scaling+sy)*otherNN0);
                    for (int comp = 0; comp < numComp; comp++) {
                        out[comp] = x0y0[comp]*(1-x)*(1-y) + x1y0[comp]*x*(1-y) + x0y1[comp]*(1-x)*y + x1y1[comp]*x*y;
                    }
                }
            }
        }
    }
}

void MultiRectangle::interpolateReducedToElementsFiner(const escript::Data& source,
        escript::Data& target, const MultiRectangle& other) const
{
    const int scaling = other.getNumSubdivisionsPerElement()/m_subdivisions;
    const dim_t numComp = source.getDataPointSize();
    //for each of ours
#pragma omp parallel for
    for (dim_t ey = 0; ey < m_NE[1]; ey++) {
        for (dim_t ex = 0; ex < m_NE[0]; ex++) {
            const double *in = source.getSampleDataRO(ex + ey*m_NE[0]);
            //for each subelement
            for (dim_t sy = 0; sy < scaling; sy++) {
                const dim_t ty = ey*scaling + sy;
                for (dim_t sx = 0; sx < scaling; sx++) {
                    const dim_t tx = ex*scaling + sx;
                    double *out = target.getSampleDataRW(tx + ty*m_NE[0]*scaling);
                    for (dim_t comp = 0; comp < numComp; comp++) {
                        const double quadvalue = in[comp];
                        out[INDEX3(comp, 0, 0, numComp, 2)] = quadvalue;
                        out[INDEX3(comp, 0, 1, numComp, 2)] = quadvalue;
                        out[INDEX3(comp, 1, 0, numComp, 2)] = quadvalue;
                        out[INDEX3(comp, 1, 1, numComp, 2)] = quadvalue;
                    }
                }
            }
        }
    }
}

void MultiRectangle::interpolateReducedToReducedFiner(const escript::Data& source,
        escript::Data& target, const MultiRectangle& other) const
{
    const int scaling = other.getNumSubdivisionsPerElement()/m_subdivisions;
    const dim_t numComp = source.getDataPointSize();
    //for each of ours
#pragma omp parallel for
    for (dim_t ey = 0; ey < m_NE[1]; ey++) {
        for (dim_t ex = 0; ex < m_NE[0]; ex++) {
            const double *in = source.getSampleDataRO(ex + ey*m_NE[0]);
            //for each subelement
            for (dim_t sy = 0; sy < scaling; sy++) {
                const dim_t ty = ey*scaling + sy;
                for (dim_t sx = 0; sx < scaling; sx++) {
                    const dim_t tx = ex*scaling + sx;
                    double *out = target.getSampleDataRW(tx + ty*m_NE[0]*scaling);
                    for (dim_t comp = 0; comp < numComp; comp++) {
                        out[comp] = in[comp];
                    }
                }
            }
        }
    }
}

void MultiRectangle::interpolateNodesToElementsFiner(const escript::Data& source,
        escript::Data& target, const MultiRectangle& other) const
{
    const int scaling = other.getNumSubdivisionsPerElement()/m_subdivisions;
    const dim_t NE0 = m_NE[0], NE1 = m_NE[1];
    const dim_t numComp = source.getDataPointSize();
#pragma omp parallel for
    for (dim_t ey = 0; ey < NE1; ey++) { //source nodes
        for (dim_t ex = 0; ex < NE0; ex++) {
            const double *x0y0 = source.getSampleDataRO(ey*(NE0+1) + ex);
            const double *x0y1 = source.getSampleDataRO((ey+1)*(NE0+1) + ex);
            const double *x1y0 = source.getSampleDataRO(ey*(NE0+1) + ex + 1);
            const double *x1y1 = source.getSampleDataRO((ey+1)*(NE0+1) + ex + 1);
            const double origin[2] = {getLocalCoordinate(ex, 0), getLocalCoordinate(ey, 1)};
            for (int sy = 0; sy < scaling; sy++) { //target elements
                for (int sx = 0; sx < scaling; sx++) {
                    const double x1 = (other.getLocalCoordinate(ex*scaling+sx, 0) - origin[0]) / m_dx[0] + FIRST_QUAD/scaling;
                    const double x2 = x1 + (SECOND_QUAD - FIRST_QUAD)/scaling;
                    const double y1 = (other.getLocalCoordinate(ey*scaling+sy, 1) - origin[1]) / m_dx[1] + FIRST_QUAD/scaling;
                    const double y2 = y1 + (SECOND_QUAD - FIRST_QUAD)/scaling;
                    double *out = target.getSampleDataRW(ex*scaling+sx + (ey*scaling+sy)*NE0*scaling);
                    for (int comp = 0; comp < numComp; comp++) {
                        out[INDEX3(comp, 0, 0, numComp, 2)] = x0y0[comp]*(1-x1)*(1-y1) + x1y0[comp]*x1*(1-y1) + x0y1[comp]*(1-x1)*y1 + x1y1[comp]*x1*y1;
                        out[INDEX3(comp, 0, 1, numComp, 2)] = x0y0[comp]*(1-x1)*(1-y2) + x1y0[comp]*x1*(1-y2) + x0y1[comp]*(1-x1)*y2 + x1y1[comp]*x1*y2;
                        out[INDEX3(comp, 1, 0, numComp, 2)] = x0y0[comp]*(1-x2)*(1-y1) + x1y0[comp]*x2*(1-y1) + x0y1[comp]*(1-x2)*y1 + x1y1[comp]*x2*y1;
                        out[INDEX3(comp, 1, 1, numComp, 2)] = x0y0[comp]*(1-x2)*(1-y2) + x1y0[comp]*x2*(1-y2) + x0y1[comp]*(1-x2)*y2 + x1y1[comp]*x2*y2;
                    }
                }
            }
        }
    }
}

void MultiRectangle::interpolateElementsToElementsCoarser(const escript::Data& source,
        escript::Data& target, const MultiRectangle& other) const
{
    const int scaling = m_subdivisions/other.getNumSubdivisionsPerElement();
    const double scaling_volume = (1./scaling)*(1./scaling);
    const dim_t *theirNE = other.getNumElementsPerDim();
    const dim_t numComp = source.getDataPointSize();

    std::vector<double> points(scaling*2, 0);
    std::vector<double> first_lagrange(scaling*2, 1);
    std::vector<double> second_lagrange(scaling*2, 1);
    
    for (int i = 0; i < scaling*2; i+=2) {
        points[i] = (i/2 + FIRST_QUAD)/scaling;
        points[i+1] = (i/2 + SECOND_QUAD)/scaling;
    }
    
    for (int i = 0; i < scaling*2; i++) {
        first_lagrange[i] = (points[i] - SECOND_QUAD) / (FIRST_QUAD - SECOND_QUAD);
        second_lagrange[i] = (points[i] - FIRST_QUAD) / (SECOND_QUAD - FIRST_QUAD);
    }
    
    //for each of theirs
#pragma omp parallel for
    for (dim_t ty = 0; ty < theirNE[1]; ty++) {
        for (dim_t tx = 0; tx < theirNE[0]; tx++) {
            double *out = target.getSampleDataRW(tx + ty*theirNE[0]);
            //for each subelement
            for (dim_t sy = 0; sy < scaling; sy++) {
                const dim_t ey = ty*scaling + sy;
                for (dim_t sx = 0; sx < scaling; sx++) {
                    const dim_t ex = tx*scaling + sx;
                    const double *in = source.getSampleDataRO(ex + ey*m_NE[0]);
                    for (int quad = 0; quad < 4; quad++) {
                        int lx = sx*2 + quad%2;
                        int ly = sy*2 + quad/2;
                        for (dim_t comp = 0; comp < numComp; comp++) {
                            const double quadvalue = scaling_volume * in[comp + quad*numComp];
                            out[INDEX3(comp, 0, 0, numComp, 2)] += quadvalue * first_lagrange[lx] * first_lagrange[ly];
                            out[INDEX3(comp, 0, 1, numComp, 2)] += quadvalue * first_lagrange[lx] * second_lagrange[ly];
                            out[INDEX3(comp, 1, 0, numComp, 2)] += quadvalue * second_lagrange[lx] * first_lagrange[ly];
                            out[INDEX3(comp, 1, 1, numComp, 2)] += quadvalue * second_lagrange[lx] * second_lagrange[ly];
                        }
                    }
                }
            }
        }
    }
}


void MultiRectangle::interpolateElementsToElementsFiner(const escript::Data& source,
        escript::Data& target, const MultiRectangle& other) const
{
    const int scaling = other.getNumSubdivisionsPerElement()/m_subdivisions;
    const dim_t numComp = source.getDataPointSize();

    std::vector<double> points(scaling*2, 0);
    std::vector<double> lagranges(scaling*4, 1);

    for (int i = 0; i < scaling*2; i+=2) {
        points[i] = (i/2 + FIRST_QUAD)/scaling;
        points[i+1] = (i/2 + SECOND_QUAD)/scaling;
    }
    for (int i = 0; i < scaling*2; i++) {
        lagranges[i] = (points[i] - SECOND_QUAD) / (FIRST_QUAD - SECOND_QUAD);
        lagranges[i + 2*scaling] = (points[i] - FIRST_QUAD) / (SECOND_QUAD - FIRST_QUAD);
    }
    //for each of ours
#pragma omp parallel for
    for (dim_t ey = 0; ey < m_NE[1]; ey++) {
        for (dim_t ex = 0; ex < m_NE[0]; ex++) {
            const double *in = source.getSampleDataRO(ex + ey*m_NE[0]);
            //for each subelement
            for (dim_t sy = 0; sy < scaling; sy++) {
                const dim_t ty = ey*scaling + sy;
                for (dim_t sx = 0; sx < scaling; sx++) {
                    const dim_t tx = ex*scaling + sx;
                    double *out = target.getSampleDataRW(tx + ty*m_NE[0]*scaling);
                    for (int quad = 0; quad < 4; quad++) {
                        const int lx = scaling*2*(quad%2) + sx*2;
                        const int ly = scaling*2*(quad/2) + sy*2;
                        for (dim_t comp = 0; comp < numComp; comp++) {
                            const double quadvalue = in[comp + quad*numComp];
                            out[INDEX3(comp, 0, 0, numComp, 2)] += quadvalue * lagranges[lx] * lagranges[ly];
                            out[INDEX3(comp, 0, 1, numComp, 2)] += quadvalue * lagranges[lx] * lagranges[ly+1];
                            out[INDEX3(comp, 1, 0, numComp, 2)] += quadvalue * lagranges[lx+1] * lagranges[ly];
                            out[INDEX3(comp, 1, 1, numComp, 2)] += quadvalue * lagranges[lx+1] * lagranges[ly+1];
                        }
                    }
                }
            }
        }
    }
}

void MultiRectangle::interpolateAcross(escript::Data& target,
                                     const escript::Data& source) const
{
    const MultiRectangle *other =
                dynamic_cast<const MultiRectangle *>(target.getDomain().get());
    if (other == NULL)
        throw RipleyException("Invalid interpolation: Domains must both be instances of MultiRectangle");
    //shouldn't ever happen, but I want to know if it does
    if (other == this)
        throw RipleyException("interpolateAcross: this domain is the target");
        
    validateInterpolationAcross(source.getFunctionSpace().getTypeCode(),
            *(target.getDomain().get()), target.getFunctionSpace().getTypeCode());
    int fsSource = source.getFunctionSpace().getTypeCode();
    int fsTarget = target.getFunctionSpace().getTypeCode();

    std::stringstream msg;
    msg << "Invalid interpolation: interpolation not implemented for function space "
        << functionSpaceTypeAsString(fsSource)
        << " -> "
        << functionSpaceTypeAsString(fsTarget);
    if (other->getNumSubdivisionsPerElement() > getNumSubdivisionsPerElement()) {
        switch (fsSource) {
            case Nodes:
                switch (fsTarget) {
                    case Nodes:
                    case ReducedNodes:
                    case DegreesOfFreedom:
                    case ReducedDegreesOfFreedom:
                        interpolateNodesToNodesFiner(source, target, *other);
                        return;
                    case Elements:
                        interpolateNodesToElementsFiner(source, target, *other);
                        return;
                }
                break;
            case Elements:
                switch (fsTarget) {
                    case Elements:
                        interpolateElementsToElementsFiner(source, target, *other);
                        return;
                }
                break;
            case ReducedElements:
                switch (fsTarget) {
                    case Elements:
                        interpolateReducedToElementsFiner(source, target, *other);
                        return;
                }
                break;
        }
        msg << " when target is a finer mesh";
    } else {
        switch (fsSource) {
            case Nodes:
                switch (fsTarget) {
                    case Elements:
                        escript::Data elements=escript::Vector(0., escript::function(*this), true);
                        interpolateNodesOnElements(elements, source, false);
                        interpolateElementsToElementsCoarser(elements, target, *other);
                        return;
                }
                break;
            case Elements:
                switch (fsTarget) {
                    case Elements:
                        interpolateElementsToElementsCoarser(source, target, *other);
                        return;
                }
                break;
        }
        msg << " when target is a coarser mesh";
    }
    throw RipleyException(msg.str());
}

string MultiRectangle::getDescription() const
{
    return "ripley::MultiRectangle";
}

bool MultiRectangle::operator==(const AbstractDomain& other) const
{
    const MultiRectangle* o=dynamic_cast<const MultiRectangle*>(&other);
    if (o) {
        return (RipleyDomain::operator==(other) &&
                m_gNE[0]==o->m_gNE[0] && m_gNE[1]==o->m_gNE[1]
                && m_origin[0]==o->m_origin[0] && m_origin[1]==o->m_origin[1]
                && m_length[0]==o->m_length[0] && m_length[1]==o->m_length[1]
                && m_NX[0]==o->m_NX[0] && m_NX[1]==o->m_NX[1]
                && m_subdivisions==o->m_subdivisions);
    }

    return false;
}

void MultiRectangle::readNcGrid(escript::Data& out, string filename, string varname,
            const ReaderParameters& params) const
{
    if (m_subdivisions != 1)
        throw RipleyException("Non-parent MultiRectangles cannot read datafiles");
    Rectangle::readNcGrid(out, filename, varname, params);
}

void MultiRectangle::readBinaryGrid(escript::Data& out, string filename,
                               const ReaderParameters& params) const
{
    if (m_subdivisions != 1)
        throw RipleyException("Non-parent MultiRectangles cannot read datafiles");
    Rectangle::readBinaryGrid(out, filename, params);
}

#ifdef USE_BOOSTIO
void MultiRectangle::readBinaryGridFromZipped(escript::Data& out, string filename,
                               const ReaderParameters& params) const
{
    if (m_subdivisions != 1)
        throw RipleyException("Non-parent MultiRectangles cannot read datafiles");
    Rectangle::readBinaryGridFromZipped(out, filename, params);
}
#endif

void MultiRectangle::writeBinaryGrid(const escript::Data& in, string filename,
                                int byteOrder, int dataType) const
{
    if (m_subdivisions != 1)
        throw RipleyException("Non-parent MultiRectangles cannot read datafiles");
    Rectangle::writeBinaryGrid(in, filename, byteOrder, dataType);
}

void MultiRectangle::dump(const string& fileName) const
{
    if (m_subdivisions != 1)
        throw RipleyException("Non-parent MultiRectangles dump not implemented");
    Rectangle::dump(fileName);
}

const dim_t* MultiRectangle::borrowSampleReferenceIDs(int fsType) const
{
    switch (fsType) {
        case Nodes:
        case ReducedNodes: // FIXME: reduced
            return &m_nodeId[0];
        case DegreesOfFreedom:
        case ReducedDegreesOfFreedom: // FIXME: reduced
            return &m_dofId[0];
        case Elements:
        case ReducedElements:
            return &m_elementId[0];
        case FaceElements:
        case ReducedFaceElements:
            return &m_faceId[0];
        case Points:
            return &m_diracPointNodeIDs[0];
        default:
            break;
    }

    std::stringstream msg;
    msg << "borrowSampleReferenceIDs: invalid function space type " << fsType;
    throw RipleyException(msg.str());
}

bool MultiRectangle::ownSample(int fsType, index_t id) const
{
    if (getMPISize()==1)
        return true;

    switch (fsType) {
        case Nodes:
        case ReducedNodes: // FIXME: reduced
            return (m_dofMap[id] < getNumDOF());
        case DegreesOfFreedom:
        case ReducedDegreesOfFreedom:
            return true;
        case Elements:
        case ReducedElements:
            // check ownership of element's bottom left node
            return (m_dofMap[id%m_NE[0]+m_NN[0]*(id/m_NE[0])] < getNumDOF());
        case FaceElements:
        case ReducedFaceElements:
            {
                // determine which face the sample belongs to before
                // checking ownership of corresponding element's first node
                dim_t n=0;
                for (size_t i=0; i<4; i++) {
                    n+=m_faceCount[i];
                    if (id<n) {
                        index_t k;
                        if (i==1)
                            k=m_NN[0]-2;
                        else if (i==3)
                            k=m_NN[0]*(m_NN[1]-2);
                        else
                            k=0;
                        // determine whether to move right or up
                        const index_t delta=(i/2==0 ? m_NN[0] : 1);
                        return (m_dofMap[k+(id-n+m_faceCount[i])*delta] < getNumDOF());
                    }
                }
                return false;
            }
        default:
            break;
    }

    std::stringstream msg;
    msg << "ownSample: invalid function space type " << fsType;
    throw RipleyException(msg.str());
}

void MultiRectangle::setToNormal(escript::Data& out) const
{
    const dim_t NE0=m_NE[0];
    const dim_t NE1=m_NE[1];
    if (out.getFunctionSpace().getTypeCode() == FaceElements) {
        out.requireWrite();
#pragma omp parallel
        {
            if (m_faceOffset[0] > -1) {
#pragma omp for nowait
                for (index_t k1 = 0; k1 < NE1; ++k1) {
                    double* o = out.getSampleDataRW(m_faceOffset[0]+k1);
                    // set vector at two quadrature points
                    *o++ = -1.;
                    *o++ = 0.;
                    *o++ = -1.;
                    *o = 0.;
                }
            }

            if (m_faceOffset[1] > -1) {
#pragma omp for nowait
                for (index_t k1 = 0; k1 < NE1; ++k1) {
                    double* o = out.getSampleDataRW(m_faceOffset[1]+k1);
                    // set vector at two quadrature points
                    *o++ = 1.;
                    *o++ = 0.;
                    *o++ = 1.;
                    *o = 0.;
                }
            }

            if (m_faceOffset[2] > -1) {
#pragma omp for nowait
                for (index_t k0 = 0; k0 < NE0; ++k0) {
                    double* o = out.getSampleDataRW(m_faceOffset[2]+k0);
                    // set vector at two quadrature points
                    *o++ = 0.;
                    *o++ = -1.;
                    *o++ = 0.;
                    *o = -1.;
                }
            }

            if (m_faceOffset[3] > -1) {
#pragma omp for nowait
                for (index_t k0 = 0; k0 < NE0; ++k0) {
                    double* o = out.getSampleDataRW(m_faceOffset[3]+k0);
                    // set vector at two quadrature points
                    *o++ = 0.;
                    *o++ = 1.;
                    *o++ = 0.;
                    *o = 1.;
                }
            }
        } // end of parallel section
    } else if (out.getFunctionSpace().getTypeCode() == ReducedFaceElements) {
        out.requireWrite();
#pragma omp parallel
        {
            if (m_faceOffset[0] > -1) {
#pragma omp for nowait
                for (index_t k1 = 0; k1 < NE1; ++k1) {
                    double* o = out.getSampleDataRW(m_faceOffset[0]+k1);
                    *o++ = -1.;
                    *o = 0.;
                }
            }

            if (m_faceOffset[1] > -1) {
#pragma omp for nowait
                for (index_t k1 = 0; k1 < NE1; ++k1) {
                    double* o = out.getSampleDataRW(m_faceOffset[1]+k1);
                    *o++ = 1.;
                    *o = 0.;
                }
            }

            if (m_faceOffset[2] > -1) {
#pragma omp for nowait
                for (index_t k0 = 0; k0 < NE0; ++k0) {
                    double* o = out.getSampleDataRW(m_faceOffset[2]+k0);
                    *o++ = 0.;
                    *o = -1.;
                }
            }

            if (m_faceOffset[3] > -1) {
#pragma omp for nowait
                for (index_t k0 = 0; k0 < NE0; ++k0) {
                    double* o = out.getSampleDataRW(m_faceOffset[3]+k0);
                    *o++ = 0.;
                    *o = 1.;
                }
            }
        } // end of parallel section

    } else {
        std::stringstream msg;
        msg << "setToNormal: invalid function space type "
            << out.getFunctionSpace().getTypeCode();
        throw RipleyException(msg.str());
    }
}

void MultiRectangle::setToSize(escript::Data& out) const
{
    if (out.getFunctionSpace().getTypeCode() == Elements
            || out.getFunctionSpace().getTypeCode() == ReducedElements) {
        out.requireWrite();
        const dim_t numQuad = out.getNumDataPointsPerSample();
        const dim_t numElements = getNumElements();
        const double size=sqrt(m_dx[0]*m_dx[0]+m_dx[1]*m_dx[1]);
#pragma omp parallel for
        for (index_t k = 0; k < numElements; ++k) {
            double* o = out.getSampleDataRW(k);
            fill(o, o+numQuad, size);
        }
    } else if (out.getFunctionSpace().getTypeCode() == FaceElements
            || out.getFunctionSpace().getTypeCode() == ReducedFaceElements) {
        out.requireWrite();
        const dim_t numQuad=out.getNumDataPointsPerSample();
        const dim_t NE0 = m_NE[0];
        const dim_t NE1 = m_NE[1];
#pragma omp parallel
        {
            if (m_faceOffset[0] > -1) {
#pragma omp for nowait
                for (index_t k1 = 0; k1 < NE1; ++k1) {
                    double* o = out.getSampleDataRW(m_faceOffset[0]+k1);
                    fill(o, o+numQuad, m_dx[1]);
                }
            }

            if (m_faceOffset[1] > -1) {
#pragma omp for nowait
                for (index_t k1 = 0; k1 < NE1; ++k1) {
                    double* o = out.getSampleDataRW(m_faceOffset[1]+k1);
                    fill(o, o+numQuad, m_dx[1]);
                }
            }

            if (m_faceOffset[2] > -1) {
#pragma omp for nowait
                for (index_t k0 = 0; k0 < NE0; ++k0) {
                    double* o = out.getSampleDataRW(m_faceOffset[2]+k0);
                    fill(o, o+numQuad, m_dx[0]);
                }
            }

            if (m_faceOffset[3] > -1) {
#pragma omp for nowait
                for (index_t k0 = 0; k0 < NE0; ++k0) {
                    double* o = out.getSampleDataRW(m_faceOffset[3]+k0);
                    fill(o, o+numQuad, m_dx[0]);
                }
            }
        } // end of parallel section

    } else {
        std::stringstream msg;
        msg << "setToSize: invalid function space type "
            << out.getFunctionSpace().getTypeCode();
        throw RipleyException(msg.str());
    }
}

//private
void MultiRectangle::populateSampleIds()
{
    // degrees of freedom are numbered from left to right, bottom to top in
    // each rank, continuing on the next rank (ranks also go left-right,
    // bottom-top).
    // This means rank 0 has id 0...n0-1, rank 1 has id n0...n1-1 etc. which
    // helps when writing out data rank after rank.

    // build node distribution vector first.
    // rank i owns m_nodeDistribution[i+1]-nodeDistribution[i] nodes which is
    // constant for all ranks in this implementation
    m_nodeDistribution.assign(m_mpiInfo->size+1, 0);
    const dim_t numDOF=getNumDOF();
    for (dim_t k=1; k<m_mpiInfo->size; k++) {
        m_nodeDistribution[k]=k*numDOF;
    }
    m_nodeDistribution[m_mpiInfo->size]=getNumDataPointsGlobal();
    try {
        m_nodeId.resize(getNumNodes());
        m_dofId.resize(numDOF);
        m_elementId.resize(getNumElements());
    } catch (const std::length_error& le) {
        throw RipleyException("The system does not have sufficient memory for a domain of this size.");
    }

    // populate face element counts
    //left
    if (m_offset[0]==0)
        m_faceCount[0]=m_NE[1];
    else
        m_faceCount[0]=0;
    //right
    if (m_mpiInfo->rank%m_NX[0]==m_NX[0]-1)
        m_faceCount[1]=m_NE[1];
    else
        m_faceCount[1]=0;
    //bottom
    if (m_offset[1]==0)
        m_faceCount[2]=m_NE[0];
    else
        m_faceCount[2]=0;
    //top
    if (m_mpiInfo->rank/m_NX[0]==m_NX[1]-1)
        m_faceCount[3]=m_NE[0];
    else
        m_faceCount[3]=0;

    const dim_t NFE = getNumFaceElements();
    m_faceId.resize(NFE);

    const index_t left = (m_offset[0]==0 ? 0 : 1);
    const index_t bottom = (m_offset[1]==0 ? 0 : 1);
    const dim_t nDOF0 = (m_gNE[0]+1)/m_NX[0];
    const dim_t nDOF1 = (m_gNE[1]+1)/m_NX[1];
    const dim_t NE0 = m_NE[0];
    const dim_t NE1 = m_NE[1];

#define globalNodeId(x,y) \
    ((m_offset[0]+x)/nDOF0)*nDOF0*nDOF1+(m_offset[0]+x)%nDOF0 \
    + ((m_offset[1]+y)/nDOF1)*nDOF0*nDOF1*m_NX[0]+((m_offset[1]+y)%nDOF1)*nDOF0

    // set corner id's outside the parallel region
    m_nodeId[0] = globalNodeId(0, 0);
    m_nodeId[m_NN[0]-1] = globalNodeId(m_NN[0]-1, 0);
    m_nodeId[m_NN[0]*(m_NN[1]-1)] = globalNodeId(0, m_NN[1]-1);
    m_nodeId[m_NN[0]*m_NN[1]-1] = globalNodeId(m_NN[0]-1,m_NN[1]-1);
#undef globalNodeId

#pragma omp parallel
    {
        // populate degrees of freedom and own nodes (identical id)
#pragma omp for nowait
        for (dim_t i=0; i<nDOF1; i++) {
            for (dim_t j=0; j<nDOF0; j++) {
                const index_t nodeIdx=j+left+(i+bottom)*m_NN[0];
                const index_t dofIdx=j+i*nDOF0;
                m_dofId[dofIdx] = m_nodeId[nodeIdx]
                    = m_nodeDistribution[m_mpiInfo->rank]+dofIdx;
            }
        }

        // populate the rest of the nodes (shared with other ranks)
        if (m_faceCount[0]==0) { // left column
#pragma omp for nowait
            for (dim_t i=0; i<nDOF1; i++) {
                const index_t nodeIdx=(i+bottom)*m_NN[0];
                const index_t dofId=(i+1)*nDOF0-1;
                m_nodeId[nodeIdx]
                    = m_nodeDistribution[m_mpiInfo->rank-1]+dofId;
            }
        }
        if (m_faceCount[1]==0) { // right column
#pragma omp for nowait
            for (dim_t i=0; i<nDOF1; i++) {
                const index_t nodeIdx=(i+bottom+1)*m_NN[0]-1;
                const index_t dofId=i*nDOF0;
                m_nodeId[nodeIdx]
                    = m_nodeDistribution[m_mpiInfo->rank+1]+dofId;
            }
        }
        if (m_faceCount[2]==0) { // bottom row
#pragma omp for nowait
            for (dim_t i=0; i<nDOF0; i++) {
                const index_t nodeIdx=i+left;
                const index_t dofId=nDOF0*(nDOF1-1)+i;
                m_nodeId[nodeIdx]
                    = m_nodeDistribution[m_mpiInfo->rank-m_NX[0]]+dofId;
            }
        }
        if (m_faceCount[3]==0) { // top row
#pragma omp for nowait
            for (dim_t i=0; i<nDOF0; i++) {
                const index_t nodeIdx=m_NN[0]*(m_NN[1]-1)+i+left;
                const index_t dofId=i;
                m_nodeId[nodeIdx]
                    = m_nodeDistribution[m_mpiInfo->rank+m_NX[0]]+dofId;
            }
        }

        // populate element id's
#pragma omp for nowait
        for (dim_t i1=0; i1<NE1; i1++) {
            for (dim_t i0=0; i0<NE0; i0++) {
                m_elementId[i0+i1*NE0]=(m_offset[1]+i1)*m_gNE[0]+m_offset[0]+i0;
            }
        }

        // face elements
#pragma omp for
        for (dim_t k=0; k<NFE; k++)
            m_faceId[k]=k;
    } // end parallel section

    m_nodeTags.assign(getNumNodes(), 0);
    updateTagsInUse(Nodes);

    m_elementTags.assign(getNumElements(), 0);
    updateTagsInUse(Elements);

    // generate face offset vector and set face tags
    const index_t LEFT=1, RIGHT=2, BOTTOM=10, TOP=20;
    const index_t faceTag[] = { LEFT, RIGHT, BOTTOM, TOP };
    m_faceOffset.assign(4, -1);
    m_faceTags.clear();
    index_t offset=0;
    for (size_t i=0; i<4; i++) {
        if (m_faceCount[i]>0) {
            m_faceOffset[i]=offset;
            offset+=m_faceCount[i];
            m_faceTags.insert(m_faceTags.end(), m_faceCount[i], faceTag[i]);
        }
    }
    setTagMap("left", LEFT);
    setTagMap("right", RIGHT);
    setTagMap("bottom", BOTTOM);
    setTagMap("top", TOP);
    updateTagsInUse(FaceElements);

    populateDofMap();
}

//private
vector<IndexVector> MultiRectangle::getConnections() const
{
    // returns a vector v of size numDOF where v[i] is a vector with indices
    // of DOFs connected to i (up to 9 in 2D)
    const dim_t nDOF0 = (m_gNE[0]+1)/m_NX[0];
    const dim_t nDOF1 = (m_gNE[1]+1)/m_NX[1];
    const dim_t M = nDOF0*nDOF1;
    vector<IndexVector> indices(M);

#pragma omp parallel for
    for (index_t i=0; i < M; i++) {
        const index_t x = i % nDOF0;
        const index_t y = i / nDOF0;
        // loop through potential neighbours and add to index if positions are
        // within bounds
        for (dim_t i1=y-1; i1<y+2; i1++) {
            for (dim_t i0=x-1; i0<x+2; i0++) {
                if (i0>=0 && i1>=0 && i0<nDOF0 && i1<nDOF1) {
                    indices[i].push_back(i1*nDOF0 + i0);
                }
            }
        }
    }
    return indices;
}

//private
void MultiRectangle::populateDofMap()
{
    const dim_t nDOF0 = (m_gNE[0]+1)/m_NX[0];
    const dim_t nDOF1 = (m_gNE[1]+1)/m_NX[1];
    const index_t left = (m_offset[0]==0 ? 0 : 1);
    const index_t bottom = (m_offset[1]==0 ? 0 : 1);

    // populate node->DOF mapping with own degrees of freedom.
    // The rest is assigned in the loop further down
    m_dofMap.assign(getNumNodes(), 0);
#pragma omp parallel for
    for (index_t i=bottom; i<bottom+nDOF1; i++) {
        for (index_t j=left; j<left+nDOF0; j++) {
            m_dofMap[i*m_NN[0]+j]=(i-bottom)*nDOF0+j-left;
        }
    }

    // build list of shared components and neighbours by looping through
    // all potential neighbouring ranks and checking if positions are
    // within bounds
    const dim_t numDOF=nDOF0*nDOF1;
    RankVector neighbour;
    IndexVector offsetInShared(1,0);
    IndexVector sendShared, recvShared;
    const int x=m_mpiInfo->rank%m_NX[0];
    const int y=m_mpiInfo->rank/m_NX[0];
    // numShared will contain the number of shared DOFs after the following
    // blocks
    dim_t numShared=0;
    // sharing bottom edge
    if (y > 0) {
        neighbour.push_back((y-1)*m_NX[0] + x);
        const dim_t num = nDOF0;
        offsetInShared.push_back(offsetInShared.back()+num);
        for (dim_t i=0; i<num; i++, numShared++) {
            sendShared.push_back(i);
            recvShared.push_back(numDOF+numShared);
            m_dofMap[left+i]=numDOF+numShared;
        }
    }
    // sharing top edge
    if (y < m_NX[1] - 1) {
        neighbour.push_back((y+1)*m_NX[0] + x);
        const dim_t num = nDOF0;
        offsetInShared.push_back(offsetInShared.back()+num);
        for (dim_t i=0; i<num; i++, numShared++) {
            sendShared.push_back(numDOF-num+i);
            recvShared.push_back(numDOF+numShared);
            m_dofMap[m_NN[0]*(m_NN[1]-1)+left+i]=numDOF+numShared;
        }
    }
    // sharing left edge
    if (x > 0) {
        neighbour.push_back(y*m_NX[0] + x-1);
        const dim_t num = nDOF1;
        offsetInShared.push_back(offsetInShared.back()+num);
        for (dim_t i=0; i<num; i++, numShared++) {
            sendShared.push_back(i*nDOF0);
            recvShared.push_back(numDOF+numShared);
            m_dofMap[(bottom+i)*m_NN[0]]=numDOF+numShared;
        }
    }
    // sharing right edge
    if (x < m_NX[0] - 1) {
        neighbour.push_back(y*m_NX[0] + x+1);
        const dim_t num = nDOF1;
        offsetInShared.push_back(offsetInShared.back()+num);
        for (dim_t i=0; i<num; i++, numShared++) {
            sendShared.push_back((i+1)*nDOF0-1);
            recvShared.push_back(numDOF+numShared);
            m_dofMap[(bottom+1+i)*m_NN[0]-1]=numDOF+numShared;
        }
    }
    // sharing bottom-left node
    if (x > 0 && y > 0) {
        neighbour.push_back((y-1)*m_NX[0] + x-1);
        // sharing a node
        offsetInShared.push_back(offsetInShared.back()+1);
        sendShared.push_back(0);
        recvShared.push_back(numDOF+numShared);
        m_dofMap[0]=numDOF+numShared;
        ++numShared;
    }
    // sharing top-left node
    if (x > 0 && y < m_NX[1]-1) {
        neighbour.push_back((y+1)*m_NX[0] + x-1);
        offsetInShared.push_back(offsetInShared.back()+1);
        sendShared.push_back(numDOF-nDOF0);
        recvShared.push_back(numDOF+numShared);
        m_dofMap[m_NN[0]*(m_NN[1]-1)]=numDOF+numShared;
        ++numShared;
    }
    // sharing bottom-right node
    if (x < m_NX[0]-1 && y > 0) {
        neighbour.push_back((y-1)*m_NX[0] + x+1);
        offsetInShared.push_back(offsetInShared.back()+1);
        sendShared.push_back(nDOF0-1);
        recvShared.push_back(numDOF+numShared);
        m_dofMap[m_NN[0]-1]=numDOF+numShared;
        ++numShared;
    }
    // sharing top-right node
    if (x < m_NX[0]-1 && y < m_NX[1]-1) {
        neighbour.push_back((y+1)*m_NX[0] + x+1);
        offsetInShared.push_back(offsetInShared.back()+1);
        sendShared.push_back(numDOF-1);
        recvShared.push_back(numDOF+numShared);
        m_dofMap[m_NN[0]*m_NN[1]-1]=numDOF+numShared;
        ++numShared;
    }

    // TODO: paso::SharedComponents should take vectors to avoid this
    Esys_MPI_rank* neighPtr = NULL;
    index_t* sendPtr = NULL;
    index_t* recvPtr = NULL;
    if (neighbour.size() > 0) {
        neighPtr = &neighbour[0];
        sendPtr = &sendShared[0];
        recvPtr = &recvShared[0];
    }

    // create connector
    paso::SharedComponents_ptr snd_shcomp(new paso::SharedComponents(
            numDOF, neighbour.size(), neighPtr, sendPtr,
            &offsetInShared[0], 1, 0, m_mpiInfo));
    paso::SharedComponents_ptr rcv_shcomp(new paso::SharedComponents(
            numDOF, neighbour.size(), neighPtr, recvPtr,
            &offsetInShared[0], 1, 0, m_mpiInfo));
    m_connector.reset(new paso::Connector(snd_shcomp, rcv_shcomp));

    // useful debug output
    /*
    cout << "--- rcv_shcomp ---" << endl;
    cout << "numDOF=" << numDOF << ", numNeighbors=" << neighbour.size() << endl;
    for (size_t i=0; i<neighbour.size(); i++) {
        cout << "neighbor[" << i << "]=" << neighbour[i]
            << " offsetInShared[" << i+1 << "]=" << offsetInShared[i+1] << endl;
    }
    for (size_t i=0; i<recvShared.size(); i++) {
        cout << "shared[" << i << "]=" << recvShared[i] << endl;
    }
    cout << "--- snd_shcomp ---" << endl;
    for (size_t i=0; i<sendShared.size(); i++) {
        cout << "shared[" << i << "]=" << sendShared[i] << endl;
    }
    cout << "--- dofMap ---" << endl;
    for (size_t i=0; i<m_dofMap.size(); i++) {
        cout << "m_dofMap[" << i << "]=" << m_dofMap[i] << endl;
    }
    */
}

dim_t MultiRectangle::findNode(const double *coords) const
{
    const dim_t NOT_MINE = -1;
    //is the found element even owned by this rank
    // (inside owned or shared elements but will map to an owned element)
    for (int dim = 0; dim < m_numDim; dim++) {
        double min = m_origin[dim] + m_offset[dim]* m_dx[dim]
                - m_dx[dim]/2.; //allows for point outside mapping onto node
        double max = m_origin[dim] + (m_offset[dim] + m_NE[dim])*m_dx[dim]
                + m_dx[dim]/2.;
        if (min > coords[dim] || max < coords[dim]) {
            return NOT_MINE;
        }
    }
    // get distance from origin
    double x = coords[0] - m_origin[0];
    double y = coords[1] - m_origin[1];

    //check if the point is even inside the domain
    if (x < 0 || y < 0 || x > m_length[0] || y > m_length[1])
        return NOT_MINE;

    // distance in elements
    dim_t ex = (dim_t) floor((x + 0.01*m_dx[0]) / m_dx[0]);
    dim_t ey = (dim_t) floor((y + 0.01*m_dx[1]) / m_dx[1]);
    // set the min distance high enough to be outside the element plus a bit
    dim_t closest = NOT_MINE;
    double minDist = 1;
    for (int dim = 0; dim < m_numDim; dim++) {
        minDist += m_dx[dim]*m_dx[dim];
    }
    //find the closest node
    for (int dx = 0; dx < 1; dx++) {
        double xdist = (x - (ex + dx)*m_dx[0]);
        for (int dy = 0; dy < 1; dy++) {
            double ydist = (y - (ey + dy)*m_dx[1]);
            double total = xdist*xdist + ydist*ydist;
            if (total < minDist) {
                closest = INDEX2(ex+dx-m_offset[0], ey+dy-m_offset[1], m_NN[0]);
                minDist = total;
            }
        }
    }
    //if this happens, we've let a dirac point slip through, which is awful
    if (closest == NOT_MINE) {
        throw RipleyException("Unable to map appropriate dirac point to a node,"
                " implementation problem in MultiRectangle::findNode()");
    }
    return closest;
}

} // end of namespace ripley
