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

#include <escript/Data.h>
#include <escript/DataFactory.h>
#include <escript/FunctionSpaceFactory.h>

#include <oxley/AbstractAssembler.h>
#include <oxley/DefaultAssembler2D.h>
#include <oxley/InitAlgorithms.h>
#include <oxley/OxleyData.h>
#include <oxley/Rectangle.h>
#include <oxley/RefinementAlgorithms.h>

#include <p4est.h>
#include <p4est_algorithms.h>
#include <p4est_extended.h>
#include <p4est_iterate.h>
#include <p4est_lnodes.h>
#include <p4est_vtk.h>

namespace bp = boost::python;

namespace oxley {

    /**
       \brief
       Constructor
    */
Rectangle::Rectangle(int order,
    dim_t n0, dim_t n1,
    double x0, double y0,
    double x1, double y1,
    int d0, int d1,
    int periodic0, int periodic1):
    OxleyDomain(2, order){

    // Possible error: User passes invalid values for the dimensions
    if(n0 <= 0 || n1 <= 0)
        throw OxleyException("Number of elements in each spatial dimension must be positive");

    // Ignore d0 and d1 if we are running in serial
    m_mpiInfo = escript::makeInfo(MPI_COMM_WORLD);
    if(m_mpiInfo->size == 1) {
        d0=1;
        d1=1;
    }

    // If the user did not set the number of divisions manually
    if(d0 == -1 && d1 == -1)
    {
        d0 = m_mpiInfo->size < 3 ? 1 : m_mpiInfo->size / 3;
        d1 = m_mpiInfo->size / d0;

        if(d0*d1 != m_mpiInfo->size)
            throw OxleyException("Could not find values for d0, d1 and d2. Please set them manually.");
    }

    // Create the connectivity
    const p4est_topidx_t num_vertices = P4EST_CHILDREN;
    const p4est_topidx_t num_trees = 1;
    const p4est_topidx_t num_corners = 1;
    const double vertices[P4EST_CHILDREN * 3] = {
                                                x0, y0, 0,
                                                x1, y0, 0,
                                                x0, y1, 0,
                                                x1, y1, 0,
                                                };
    const p4est_topidx_t tree_to_vertex[P4EST_CHILDREN] = {0, 1, 2, 3,};
    const p4est_topidx_t tree_to_tree[P4EST_FACES] = {0, 0, 0, 0,};
    // const int8_t tree_to_face[P4EST_FACES] = {1, 0, 3, 2,}; //aeae todo: add in periodic boundary conditions
    const int8_t tree_to_face[P4EST_FACES] = {0, 1, 2, 3,};
    const p4est_topidx_t tree_to_corner[P4EST_CHILDREN] = {0, 0, 0, 0,};
    const p4est_topidx_t ctt_offset[2] = {0, P4EST_CHILDREN,};
    const p4est_topidx_t corner_to_tree[P4EST_CHILDREN] = {0, 0, 0, 0,};
    const int8_t corner_to_corner[P4EST_CHILDREN] = {0, 1, 2, 3,};

    connectivity = p4est_connectivity_new_copy(num_vertices, num_trees,
                                      num_corners, vertices, tree_to_vertex,
                                      tree_to_tree, tree_to_face,
                                      tree_to_corner, ctt_offset,
                                      corner_to_tree, corner_to_corner);

    if(!p4est_connectivity_is_valid(connectivity))
        throw OxleyException("Could not create a valid connectivity.");

    // Allocate some memory
    forestData = new p4estData;
    forestData = (p4estData *) malloc(sizeof(p4estData));

    // Create a p4est
    p4est_locidx_t min_quadrants = n0*n1;
    int min_level = 0;
    int fill_uniform = 1;
    p4est = p4est_new_ext(m_mpiInfo->comm, connectivity, min_quadrants,
            min_level, fill_uniform, sizeof(p4estData), init_rectangle_data, (void *) &forestData);

    // Record the physical dimensions of the domain and the location of the origin
    forestData->m_origin[0] = x0;
    forestData->m_origin[1] = y0;
    forestData->m_length[0] = x1-x0;
    forestData->m_length[1] = y1-y0;

    // Whether or not we have periodic boundaries
    forestData->periodic[0] = periodic0;
    forestData->periodic[1] = periodic1;

    // Find the grid spacing for each level of refinement in the mesh
#pragma omp parallel for
    for(int i = 0; i<=P4EST_MAXLEVEL; i++){
        double numberOfSubDivisions = (p4est_qcoord_t) (1 << (P4EST_MAXLEVEL - i));
        forestData->m_dx[0][i] = forestData->m_length[0] / numberOfSubDivisions;
        forestData->m_dx[1][i] = forestData->m_length[1] / numberOfSubDivisions;
    }

    // max levels of refinement
    forestData->max_levels_refinement = MAXREFINEMENTLEVELS;

    // number of face elements per edge
    // forestData->m_faceCount[0] = n0; //AEAE check this
    // forestData->m_faceCount[1] = n0;
    // forestData->m_faceCount[2] = n1;
    // forestData->m_faceCount[3] = n1;

    // face offsets //AEAE todo
    // index_t offset = 0;
    // for(int i = 0; i < 4; i++){
    //     forestData->m_faceOffset[i]=offset;
    //     offset+=forestData->m_faceCount[i];
    // }
    
    // element order
    m_order = order;

    // initial tag
    tags[0] = 0;
    numberOfTags=1;

    // Number of dimensions
    m_numDim=2;

    // Create the node numbering scheme
    // This is the local numbering of the nodes that is used by p4est:
    //  *
    //  *         f_3
    //  *  c_2           c_3
    //  *      6---7---8
    //  *      |       |
    //  * f_0  3   4   5  f_1
    //  *      |       |
    //  *      0---1---2
    //  *  c_0           c_1
    //  *         f_2
    // Note that hanging nodes don't have a global number.

    nodes_ghost = p4est_ghost_new(p4est, P4EST_CONNECT_FULL);
    nodes = p4est_lnodes_new(p4est, nodes_ghost, 2);

    // Distribute the p4est across the processors
    int allow_coarsening = 0;
    p4est_partition(p4est, allow_coarsening, NULL);
    p4est_partition_lnodes(p4est, NULL, 2, allow_coarsening);

    // To prevent segmentation faults when using numpy ndarray
#ifdef ESYS_HAVE_BOOST_NUMPY
    Py_Initialize();
    boost::python::numpy::initialize();
#endif
    
}

/**
   \brief
   Destructor.
*/
Rectangle::~Rectangle(){

    // free(forestData);
    p4est_connectivity_destroy(connectivity);
    p4est_lnodes_destroy(nodes);
    p4est_ghost_destroy(nodes_ghost);
    p4est_destroy(p4est);
    // sc_finalize();
}

/**
   \brief
   returns a description for this domain
*/
std::string Rectangle::getDescription() const{
    return "oxley::rectangle";
}


/**
   \brief
   dumps the mesh to a file with the given name
   \param filename The name of the output file
*/
void Rectangle::dump(const std::string& filename) const
{
    throw OxleyException("dump: not supported");
}

/**
   \brief
   writes the current mesh to a file with the given name
   \param filename The name of the file to write to
*/
void Rectangle::write(const std::string& filename) const
{
    throw OxleyException("write: not supported");
}

bool Rectangle::probeInterpolationAcross(int fsType_source,
        const escript::AbstractDomain& domain, int fsType_target) const
{
    throw OxleyException("1currently: not supported"); //AE this is temporary
}

void Rectangle::interpolateAcross(escript::Data& target, const escript::Data& source) const
{
    throw OxleyException("2currently: not supported"); //AE this is temporary
}

void Rectangle::setToNormal(escript::Data& out) const
{
    if (out.getFunctionSpace().getTypeCode() == FaceElements) {
        out.requireWrite();
        for (p4est_topidx_t t = p4est->first_local_tree; t <= p4est->last_local_tree; t++) 
        {
            p4est_tree_t * currenttree = p4est_tree_array_index(p4est->trees, t);
            sc_array_t * tquadrants = &currenttree->quadrants;
            p4est_locidx_t Q = (p4est_locidx_t) tquadrants->elem_count;
#pragma omp parallel for
            for (p4est_locidx_t e = nodes->global_offset; e < Q+nodes->global_offset; e++)
            {
                // Work out what level this element is on 
                p4est_quadrant_t * quad = p4est_quadrant_array_index(tquadrants, e);
                quadrantData * quaddata = (quadrantData *) quad->p.user_data;

                if (quaddata->m_faceOffset == 0) {
                    double* o = out.getSampleDataRW(e);
                    // set vector at two quadrature points
                    *o++ = -1.;
                    *o++ = 0.;
                    *o++ = -1.;
                    *o = 0.;
                }

                if (quaddata->m_faceOffset == 1) {
                    double* o = out.getSampleDataRW(e);
                    // set vector at two quadrature points
                    *o++ = 1.;
                    *o++ = 0.;
                    *o++ = 1.;
                    *o = 0.;
                }

                if (quaddata->m_faceOffset == 2) {
                    double* o = out.getSampleDataRW(e);
                    // set vector at two quadrature points
                    *o++ = 0.;
                    *o++ = -1.;
                    *o++ = 0.;
                    *o = -1.;
                }

                if (quaddata->m_faceOffset == 3) {
                    double* o = out.getSampleDataRW(e);
                    // set vector at two quadrature points
                    *o++ = 0.;
                    *o++ = 1.;
                    *o++ = 0.;
                    *o = 1.;
                }
            }
        }
    } else if (out.getFunctionSpace().getTypeCode() == ReducedFaceElements) {
        out.requireWrite();
        for (p4est_topidx_t t = p4est->first_local_tree; t <= p4est->last_local_tree; t++) 
        {
            p4est_tree_t * currenttree = p4est_tree_array_index(p4est->trees, t);
            sc_array_t * tquadrants = &currenttree->quadrants;
            p4est_locidx_t Q = (p4est_locidx_t) tquadrants->elem_count;
#pragma omp parallel for
            for (p4est_locidx_t e = nodes->global_offset; e < Q+nodes->global_offset; e++)
            {
                // Work out what level this element is on 
                p4est_quadrant_t * quad = p4est_quadrant_array_index(tquadrants, e);
                quadrantData * quaddata = (quadrantData *) quad->p.user_data;

                if (quaddata->m_faceOffset == 0) {
                    double* o = out.getSampleDataRW(e);
                    *o++ = -1.;
                    *o = 0.;
                }

                if (quaddata->m_faceOffset == 1) {
                    double* o = out.getSampleDataRW(e);
                    *o++ = 1.;
                    *o = 0.;
                }

                if (quaddata->m_faceOffset == 2) {
                    double* o = out.getSampleDataRW(e);
                    *o++ = 0.;
                    *o = -1.;
                }

                if (quaddata->m_faceOffset == 3) {
                    double* o = out.getSampleDataRW(e);
                    *o++ = 0.;
                    *o = 1.;
                }
            }
        }
    } else {
        std::stringstream msg;
        msg << "setToNormal: invalid function space type "
            << out.getFunctionSpace().getTypeCode();
        throw ValueError(msg.str());
    }
}

void Rectangle::setToSize(escript::Data& out) const
{
    throw OxleyException("4currently: not supported"); //AE this is temporary
}

bool Rectangle::ownSample(int fsType, index_t id) const
{
    throw OxleyException("5currently: not supported"); //AE this is temporary
}


/* This is a wrapper for filtered (and non-filtered) randoms
 * For detailed doco see randomFillWorker
*/
escript::Data Rectangle::randomFill(const escript::DataTypes::ShapeType& shape,
                                    const escript::FunctionSpace& fs,
                                    long seed, const bp::tuple& filter) const
{
    throw OxleyException("6currently: not supported"); //AE this is temporary
}


const dim_t* Rectangle::borrowSampleReferenceIDs(int fsType) const
{
    throw OxleyException("7currently: not supported"); //AE this is temporary
}

void Rectangle::writeToVTK(std::string filename, bool writeMesh) const
{
    // Write to file
    const char * name = filename.c_str();
    if(writeMesh)
    {
        p4est_vtk_write_file(p4est, NULL, name);
    }
    else
    {
        // Create the context for the VTK file
        p4est_vtk_context_t * context = p4est_vtk_context_new(p4est, name);

        // Continuous point data
        p4est_vtk_context_set_continuous(context, true);

        // Set the scale
        p4est_vtk_context_set_scale(context, 1.0);

        // Write the header
        context = p4est_vtk_write_header(context);

        // Get the point and cell data together
        p4est_locidx_t numquads = p4est->local_num_quadrants;

        //  Info
        sc_array_t * quadTag = sc_array_new_count(sizeof(double), numquads);
        p4est_iterate(p4est, NULL, (void *) quadTag, getQuadTagVector, NULL, NULL);
        sc_array_t * xcoord = sc_array_new_count(sizeof(double), numquads);
        p4est_iterate(p4est, NULL, (void *) xcoord, getXCoordVector, NULL, NULL);
        sc_array_t * ycoord = sc_array_new_count(sizeof(double), numquads);
        p4est_iterate(p4est, NULL, (void *) ycoord, getYCoordVector, NULL, NULL);
        // sc_array_t * NodeNumber = sc_array_new_count(sizeof(double), numquads);
        // p4est_iterate(p4est, NULL, (void *) NodeNumber, getNodeNumber, NULL, NULL);

        // Write the cell Data
#ifdef P4EST_ENABLE_DEBUG
        context = p4est_vtk_write_cell_dataf(context,1,1,0,0,3,0,"tag",quadTag,"x",xcoord,"y",ycoord,context);
#else
        context = p4est_vtk_write_cell_dataf(context,0,0,0,0,3,0,"tag",quadTag,"x",xcoord,"y",ycoord,context);
#endif
        if(context == NULL)
            throw OxleyException("Error writing cell data");

        // Write the point Data
        context = p4est_vtk_write_point_dataf(context, 0, 0, context);
        if(context == NULL)
            throw OxleyException("Error writing point data");

        // Write the footer
        if(p4est_vtk_write_footer(context)) // The context is destroyed by this function
                throw OxleyException("Error writing footer.");

        // Cleanup
        sc_array_reset(quadTag);
        sc_array_destroy(quadTag);
        sc_array_reset(xcoord);
        sc_array_destroy(xcoord);
        sc_array_reset(ycoord);
        sc_array_destroy(ycoord);
    }
}

void Rectangle::refineMesh(int maxRecursion, std::string algorithmname)
{
    if(!algorithmname.compare("uniform")){
        if(maxRecursion == 0){
            p4est_refine_ext(p4est, true, -1, refine_uniform, NULL, refine_copy_parent_quadrant);
        } else {
            p4est_refine_ext(p4est, true, maxRecursion+2, refine_uniform, NULL, refine_copy_parent_quadrant);
        }
        p4est_balance_ext(p4est, P4EST_CONNECT_FULL, NULL, refine_copy_parent_quadrant);
        int partforcoarsen = 1;
        p4est_partition(p4est, partforcoarsen, NULL);
    } else {
        throw OxleyException("Unknown refinement algorithm name.");
    }
}

escript::Data Rectangle::getX() const
{
    escript::Data out=escript::Vector(0,escript::continuousFunction(*this),true);
    setToX(out);
    out.setProtection();
    return out;
}

void Rectangle::print_debug_report(std::string locat)
{
    std::cout << "report for " <<  locat << std::endl;
    std::cout << "p4est = " << &p4est << std::endl;
    if(!p4est_is_valid(p4est))
        std::cout << "WARNING: p4est is invalid" << std::endl;
    std::cout << "forestData = " << &forestData << std::endl;
    std::cout << "connectivity = " << &connectivity << std::endl;
    if(!p4est_connectivity_is_valid(connectivity))
        std::cout << "WARNING: connectivity is invalid" << std::endl;
    std::cout << "temp_data = " << &temp_data << std::endl;

}

Assembler_ptr Rectangle::createAssembler(std::string type, const DataMap& constants) const
{
    bool isComplex = false;
    DataMap::const_iterator it;
    for(it = constants.begin(); it != constants.end(); it++) {
        if(!it->second.isEmpty() && it->second.isComplex()) {
            isComplex = true;
            break;
        }
    }

    if(type.compare("DefaultAssembler") == 0) {
        if(isComplex) {
            return Assembler_ptr(new DefaultAssembler2D<cplx_t>(shared_from_this()));
        } else {
            return Assembler_ptr(new DefaultAssembler2D<real_t>(shared_from_this()));
        }
    } 
    throw escript::NotImplementedError("oxley::rectangle does not support the requested assembler");
}

//protected
void Rectangle::assembleCoordinates(escript::Data& arg) const
{
    if (!arg.isDataPointShapeEqual(1, &m_numDim))
        throw ValueError("assembleCoordinates: Invalid Data object shape");
    if (!arg.numSamplesEqual(1, getNumNodes()))
        throw ValueError("assembleCoordinates: Illegal number of samples in Data object");
    arg.requireWrite();

    double l0 = forestData->m_length[0]+forestData->m_origin[0];
    double l1 = forestData->m_length[1]+forestData->m_origin[1];
    for(p4est_topidx_t treeid = p4est->first_local_tree; treeid <= p4est->last_local_tree; treeid++)
    {
        p4est_tree_t * currenttree = p4est_tree_array_index(p4est->trees, treeid);
        sc_array_t * tquadrants = &currenttree->quadrants;
        p4est_locidx_t Q = (p4est_locidx_t) tquadrants->elem_count;
        int counter = 0;
// #pragma omp parallel for
        for (p4est_locidx_t e = 0; e < Q; e++)
        {
            double xy[2];

            p4est_quadrant_t * quad = p4est_quadrant_array_index(tquadrants, e);
            double * point = arg.getSampleDataRW(e);
            p4est_qcoord_to_vertex(connectivity, treeid, quad->x, quad->y, &point[0]);
            p4est_qcoord_t length = P4EST_QUADRANT_LEN(quad->level);
            p4est_qcoord_to_vertex(p4est->connectivity, treeid, quad->x+length,quad->y,xy);

            if(xy[0] == l0){
                double * pointx = arg.getSampleDataRW(Q+counter);
                pointx[0] = xy[0];
                pointx[1] = xy[1];
                counter++;
            }
            p4est_qcoord_to_vertex(p4est->connectivity, treeid, quad->x,quad->y+length,xy);
            if(xy[1] == l1){
                double * pointy = arg.getSampleDataRW(Q+counter);
                pointy[0] = xy[0];
                pointy[1] = xy[1];
                counter++;
            }
            p4est_qcoord_to_vertex(p4est->connectivity, treeid, quad->x+length,quad->y+length,xy);
            if(xy[0] == l0 && xy[1] == l1){
                double * pointxy = arg.getSampleDataRW(Q+counter);
                pointxy[0] = xy[0];
                pointxy[1] = xy[1];
                counter++;
            }
        }
    }
}

//private
void Rectangle::populateDofMap()
{
    OxleyException("Programming error");
//     const dim_t nDOF0 = getNumDOFInAxis(0);
//     const dim_t nDOF1 = getNumDOFInAxis(1);
//     const index_t left = getFirstInDim(0);
//     const index_t bottom = getFirstInDim(1);

//     // populate node->DOF mapping with own degrees of freedom.
//     // The rest is assigned in the loop further down
//     m_dofMap.assign(getNumNodes(), 0);
// #pragma omp parallel for
//     for (index_t i=bottom; i<bottom+nDOF1; i++) {
//         for (index_t j=left; j<left+nDOF0; j++) {
//             m_dofMap[i*m_NN[0]+j]=(i-bottom)*nDOF0+j-left;
//         }
//     }

//     // build list of shared components and neighbours by looping through
//     // all potential neighbouring ranks and checking if positions are
//     // within bounds
//     const dim_t numDOF=nDOF0*nDOF1;
//     RankVector neighbour;
//     IndexVector offsetInShared(1,0);
//     IndexVector sendShared, recvShared;
//     const int x=m_mpiInfo->rank%m_NX[0];
//     const int y=m_mpiInfo->rank/m_NX[0];
//     // numShared will contain the number of shared DOFs after the following
//     // blocks
//     dim_t numShared=0;
//     // sharing bottom edge
//     if (y > 0) {
//         neighbour.push_back((y-1)*m_NX[0] + x);
//         const dim_t num = nDOF0;
//         offsetInShared.push_back(offsetInShared.back()+num);
//         for (dim_t i=0; i<num; i++, numShared++) {
//             sendShared.push_back(i);
//             recvShared.push_back(numDOF+numShared);
//             m_dofMap[left+i]=numDOF+numShared;
//         }
//     }
//     // sharing top edge
//     if (y < m_NX[1] - 1) {
//         neighbour.push_back((y+1)*m_NX[0] + x);
//         const dim_t num = nDOF0;
//         offsetInShared.push_back(offsetInShared.back()+num);
//         for (dim_t i=0; i<num; i++, numShared++) {
//             sendShared.push_back(numDOF-num+i);
//             recvShared.push_back(numDOF+numShared);
//             m_dofMap[m_NN[0]*(m_NN[1]-1)+left+i]=numDOF+numShared;
//         }
//     }
//     // sharing left edge
//     if (x > 0) {
//         neighbour.push_back(y*m_NX[0] + x-1);
//         const dim_t num = nDOF1;
//         offsetInShared.push_back(offsetInShared.back()+num);
//         for (dim_t i=0; i<num; i++, numShared++) {
//             sendShared.push_back(i*nDOF0);
//             recvShared.push_back(numDOF+numShared);
//             m_dofMap[(bottom+i)*m_NN[0]]=numDOF+numShared;
//         }
//     }
//     // sharing right edge
//     if (x < m_NX[0] - 1) {
//         neighbour.push_back(y*m_NX[0] + x+1);
//         const dim_t num = nDOF1;
//         offsetInShared.push_back(offsetInShared.back()+num);
//         for (dim_t i=0; i<num; i++, numShared++) {
//             sendShared.push_back((i+1)*nDOF0-1);
//             recvShared.push_back(numDOF+numShared);
//             m_dofMap[(bottom+1+i)*m_NN[0]-1]=numDOF+numShared;
//         }
//     }
//     // sharing bottom-left node
//     if (x > 0 && y > 0) {
//         neighbour.push_back((y-1)*m_NX[0] + x-1);
//         // sharing a node
//         offsetInShared.push_back(offsetInShared.back()+1);
//         sendShared.push_back(0);
//         recvShared.push_back(numDOF+numShared);
//         m_dofMap[0]=numDOF+numShared;
//         ++numShared;
//     }
//     // sharing top-left node
//     if (x > 0 && y < m_NX[1]-1) {
//         neighbour.push_back((y+1)*m_NX[0] + x-1);
//         offsetInShared.push_back(offsetInShared.back()+1);
//         sendShared.push_back(numDOF-nDOF0);
//         recvShared.push_back(numDOF+numShared);
//         m_dofMap[m_NN[0]*(m_NN[1]-1)]=numDOF+numShared;
//         ++numShared;
//     }
//     // sharing bottom-right node
//     if (x < m_NX[0]-1 && y > 0) {
//         neighbour.push_back((y-1)*m_NX[0] + x+1);
//         offsetInShared.push_back(offsetInShared.back()+1);
//         sendShared.push_back(nDOF0-1);
//         recvShared.push_back(numDOF+numShared);
//         m_dofMap[m_NN[0]-1]=numDOF+numShared;
//         ++numShared;
//     }
//     // sharing top-right node
//     if (x < m_NX[0]-1 && y < m_NX[1]-1) {
//         neighbour.push_back((y+1)*m_NX[0] + x+1);
//         offsetInShared.push_back(offsetInShared.back()+1);
//         sendShared.push_back(numDOF-1);
//         recvShared.push_back(numDOF+numShared);
//         m_dofMap[m_NN[0]*m_NN[1]-1]=numDOF+numShared;
//         ++numShared;
//     }

// #ifdef ESYS_HAVE_PASO
//     createPasoConnector(neighbour, offsetInShared, offsetInShared, sendShared,
//                         recvShared);
// #endif

    // useful debug output
    
    // std::cout << "--- rcv_shcomp ---" << std::endl;
    // std::cout << "numDOF=" << numDOF << ", numNeighbors=" << neighbour.size() << std::endl;
    // for (size_t i=0; i<neighbour.size(); i++) {
    //     std::cout << "neighbor[" << i << "]=" << neighbour[i]
    //         << " offsetInShared[" << i+1 << "]=" << offsetInShared[i+1] << std::endl;
    // }
    // for (size_t i=0; i<recvShared.size(); i++) {
    //     std::cout << "shared[" << i << "]=" << recvShared[i] << std::endl;
    // }
    // std::cout << "--- snd_shcomp ---" << std::endl;
    // for (size_t i=0; i<sendShared.size(); i++) {
    //     std::cout << "shared[" << i << "]=" << sendShared[i] << std::endl;
    // }
    // std::cout << "--- dofMap ---" << std::endl;
    // for (size_t i=0; i<m_dofMap.size(); i++) {
    //     std::cout << "m_dofMap[" << i << "]=" << m_dofMap[i] << std::endl;
    // }
    
}


//private
template<typename Scalar>
void Rectangle::addToMatrixAndRHS(escript::AbstractSystemMatrix* S, escript::Data& F,
         const std::vector<Scalar>& EM_S, const std::vector<Scalar>& EM_F, 
         bool addS, bool addF, index_t firstNode, int nEq, int nComp) const
{
    //AEAEAEAE todo:
    
    IndexVector rowIndex(4);
    // rowIndex[0] = m_dofMap[firstNode];
    // rowIndex[1] = m_dofMap[firstNode+1];
    // rowIndex[2] = m_dofMap[firstNode+m_NN[0]];
    // rowIndex[3] = m_dofMap[firstNode+m_NN[0]+1];

    if(addF)
    {
        Scalar* F_p = F.getSampleDataRW(0, static_cast<Scalar>(0));
        for(p4est_topidx_t treeid = p4est->first_local_tree; treeid <= p4est->last_local_tree; treeid++)
        {
            p4est_tree_t * currenttree = p4est_tree_array_index(p4est->trees, treeid);
            sc_array_t * tquadrants = &currenttree->quadrants;
            p4est_locidx_t Q = (p4est_locidx_t) tquadrants->elem_count;
#pragma omp parallel for
            for (p4est_locidx_t e = nodes->global_offset; e < Q+nodes->global_offset; e++)
                F_p[e]+=EM_F[e];
        }
    }
    if(addS)
    {
        addToSystemMatrix<Scalar>(S, rowIndex, nEq, EM_S);
    }
}

template
void Rectangle::addToMatrixAndRHS<real_t>(escript::AbstractSystemMatrix* S, escript::Data& F,
         const std::vector<real_t>& EM_S, const std::vector<real_t>& EM_F, 
         bool addS, bool addF, index_t firstNode, int nEq, int nComp) const;

template
void Rectangle::addToMatrixAndRHS<cplx_t>(escript::AbstractSystemMatrix* S, escript::Data& F,
         const std::vector<cplx_t>& EM_S, const std::vector<cplx_t>& EM_F, 
         bool addS, bool addF, index_t firstNode, int nEq, int nComp) const;

//protected
void Rectangle::interpolateNodesOnElements(escript::Data& out,
                                           const escript::Data& in,
                                           bool reduced) const
{
    if (in.isComplex()!=out.isComplex())
    {
        throw OxleyException("Programmer Error: in and out parameters do not have the same complexity.");
    }
    if (in.isComplex())
    {
        interpolateNodesOnElementsWorker(out, in, reduced, escript::DataTypes::cplx_t(0));
    }
    else
    {
        interpolateNodesOnElementsWorker(out, in, reduced, escript::DataTypes::real_t(0));      
    }
}

//protected
void Rectangle::interpolateNodesOnFaces(escript::Data& out,
                                           const escript::Data& in,
                                           bool reduced) const
{
    if (in.isComplex()!=out.isComplex())
    {
        throw OxleyException("Programmer Error: in and out parameters do not have the same complexity.");
    }
    if (in.isComplex())
    {
        interpolateNodesOnFacesWorker(out, in, reduced, escript::DataTypes::cplx_t(0));
    }
    else
    {
        interpolateNodesOnFacesWorker(out, in, reduced, escript::DataTypes::real_t(0));      
    }
}


// private   
template <typename S> 
void Rectangle::interpolateNodesOnElementsWorker(escript::Data& out,
                                           const escript::Data& in,
                                           bool reduced, S sentinel) const
{
    const dim_t numComp = in.getDataPointSize();
    const long  numNodes = getNumNodes();
    if (reduced) {
        out.requireWrite();
        const S c0 = 0.25;
        double * fxx = new double[4*numComp*numNodes];

        // This structure is used to store info needed by p4est
        interpolateNodesOnElementsWorker_Data<S> interpolateData;
        interpolateData.fxx = fxx;
        interpolateData.sentinel = sentinel;
        interpolateData.offset = numComp*sizeof(S);

        p4est_iterate(p4est, nodes_ghost, &interpolateData, get_interpolateNodesOnElementWorker_data, NULL, NULL);
        for(p4est_topidx_t treeid = p4est->first_local_tree; treeid <= p4est->last_local_tree; treeid++)
        {
            p4est_tree_t * currenttree = p4est_tree_array_index(p4est->trees, treeid);
            sc_array_t * tquadrants = &currenttree->quadrants;
            p4est_locidx_t Q = (p4est_locidx_t) tquadrants->elem_count;
#pragma omp parallel for
            for (p4est_locidx_t e = nodes->global_offset; e < Q+nodes->global_offset; e++)
            {
                S* o = out.getSampleDataRW(e, sentinel);
                for (index_t i=0; i < numComp; ++i) 
                {
                    o[i] = c0*(fxx[INDEX3(0,i,e,numComp,numNodes)] + 
                               fxx[INDEX3(1,i,e,numComp,numNodes)] + 
                               fxx[INDEX3(2,i,e,numComp,numNodes)] + 
                               fxx[INDEX3(3,i,e,numComp,numNodes)]);
                }
            }
        }
        delete[] fxx;
    } else {
        out.requireWrite();
        const S c0 = 0.16666666666666666667;
        const S c1 = 0.044658198738520451079;
        const S c2 = 0.62200846792814621559;
        double * fxx = new double[4*numComp*numNodes];
        // This structure is used to store info needed by p4est
        interpolateNodesOnElementsWorker_Data<S> interpolateData;
        interpolateData.fxx = fxx;
        interpolateData.sentinel = sentinel;
        interpolateData.offset = numComp*sizeof(S);
        p4est_iterate(p4est, nodes_ghost, &interpolateData, get_interpolateNodesOnElementWorker_data, NULL, NULL);
        for(p4est_topidx_t treeid = p4est->first_local_tree; treeid <= p4est->last_local_tree; treeid++)
        {
            p4est_tree_t * currenttree = p4est_tree_array_index(p4est->trees, treeid);
            sc_array_t * tquadrants = &currenttree->quadrants;
            p4est_locidx_t Q = (p4est_locidx_t) tquadrants->elem_count;
#pragma omp parallel for
            for (p4est_locidx_t e = nodes->global_offset; e < Q+nodes->global_offset; e++)
            {
                S* o = out.getSampleDataRW(e, sentinel);
                for (index_t i=0; i < numComp; ++i) {
                    o[INDEX2(i,numComp,0)] = c0*(fxx[INDEX3(1,i,e,numComp,numNodes)] +    fxx[INDEX3(2,i,e,numComp,numNodes)]) + 
                                             c1* fxx[INDEX3(3,i,e,numComp,numNodes)] + c2*fxx[INDEX3(0,i,e,numComp,numNodes)];
                    o[INDEX2(i,numComp,1)] = c0*(fxx[INDEX3(2,i,e,numComp,numNodes)] +    fxx[INDEX3(3,i,e,numComp,numNodes)]) + 
                                             c1* fxx[INDEX3(1,i,e,numComp,numNodes)] + c2*fxx[INDEX3(2,i,e,numComp,numNodes)];
                    o[INDEX2(i,numComp,2)] = c0*(fxx[INDEX3(0,i,e,numComp,numNodes)] +    fxx[INDEX3(3,i,e,numComp,numNodes)]) + 
                                             c1* fxx[INDEX3(2,i,e,numComp,numNodes)] + c2*fxx[INDEX3(1,i,e,numComp,numNodes)];
                    o[INDEX2(i,numComp,3)] = c0*(fxx[INDEX3(1,i,e,numComp,numNodes)] +    fxx[INDEX3(2,i,e,numComp,numNodes)]) + 
                                             c1* fxx[INDEX3(0,i,e,numComp,numNodes)] + c2*fxx[INDEX3(3,i,e,numComp,numNodes)];
                }
            }
        }
        delete[] fxx;
    }
}

//private
template <typename S>
void Rectangle::interpolateNodesOnFacesWorker(escript::Data& out,
                                        const escript::Data& in,
                                        bool reduced, S sentinel) const
{
    const dim_t numComp = in.getDataPointSize();
    const dim_t numNodes = getNumNodes();
    if (reduced) {
        out.requireWrite();
        double * fxx = new double[4*numComp*numNodes];
        // This structure is used to store info needed by p4est
        interpolateNodesOnFacesWorker_Data<S> interpolateData;
        interpolateData.fxx = fxx;
        interpolateData.sentinel = sentinel;
        interpolateData.offset = numComp*sizeof(S);
        for(p4est_topidx_t treeid = p4est->first_local_tree; treeid <= p4est->last_local_tree; treeid++)
        {
            p4est_tree_t * currenttree = p4est_tree_array_index(p4est->trees, treeid);
            sc_array_t * tquadrants = &currenttree->quadrants;
            p4est_locidx_t Q = (p4est_locidx_t) tquadrants->elem_count;
#pragma omp parallel for
            for (p4est_locidx_t e = nodes->global_offset; e < Q+nodes->global_offset; e++)
            {
                p4est_quadrant_t * quad = p4est_quadrant_array_index(tquadrants, e);

                if(quad->x == 0)
                {
                    interpolateData.direction=0;
                    p4est_iterate(p4est, nodes_ghost, &interpolateData, get_interpolateNodesOnFacesWorker_data, NULL, NULL);

                    S* o = out.getSampleDataRW(e, sentinel);
                    for (index_t i=0; i < numComp; ++i) {
                        o[e] = (fxx[INDEX3(0,i,e,numComp,numNodes)] + fxx[INDEX3(1,i,e,numComp,numNodes)])/static_cast<S>(2);
                    }
                }

                if(quad->x == P4EST_ROOT_LEN)
                {
                    interpolateData.direction=1;
                    p4est_iterate(p4est, nodes_ghost, &interpolateData, get_interpolateNodesOnFacesWorker_data, NULL, NULL);

                    S* o = out.getSampleDataRW(e, sentinel);
                    for (index_t i=0; i < numComp; ++i) {
                        o[e] = (fxx[INDEX3(2,i,e,numComp,numNodes)] + fxx[INDEX3(3,i,e,numComp,numNodes)])/static_cast<S>(2);
                    }
                }

                if(quad->y == 0)
                {
                    interpolateData.direction=2;
                    p4est_iterate(p4est, nodes_ghost, &interpolateData, get_interpolateNodesOnFacesWorker_data, NULL, NULL);

                    S* o = out.getSampleDataRW(e, sentinel);
                    for (index_t i=0; i < numComp; ++i) {
                        o[e] = (fxx[INDEX3(0,i,e,numComp,numNodes)] + fxx[INDEX3(2,i,e,numComp,numNodes)])/static_cast<S>(2);
                    }
                }

                if(quad->y == P4EST_ROOT_LEN)
                {
                    interpolateData.direction=3;
                    p4est_iterate(p4est, nodes_ghost, &interpolateData, get_interpolateNodesOnFacesWorker_data, NULL, NULL);

                    S* o = out.getSampleDataRW(e, sentinel);
                    for (index_t i=0; i < numComp; ++i) {
                        o[e] = (fxx[INDEX3(1,i,e,numComp,numNodes)] + fxx[INDEX3(3,i,e,numComp,numNodes)])/static_cast<S>(2);
                    }
                }
            }
        }
    } else {
        out.requireWrite();
        const S c0 = 0.21132486540518711775;
        const S c1 = 0.78867513459481288225;

        double * fxx = new double[4*numComp*numNodes];
        // This structure is used to store info needed by p4est
        interpolateNodesOnFacesWorker_Data<S> interpolateData;
        interpolateData.fxx = fxx;
        interpolateData.sentinel = sentinel;
        interpolateData.offset = numComp*sizeof(S);
        for(p4est_topidx_t treeid = p4est->first_local_tree; treeid <= p4est->last_local_tree; treeid++)
        {
            p4est_tree_t * currenttree = p4est_tree_array_index(p4est->trees, treeid);
            sc_array_t * tquadrants = &currenttree->quadrants;
            p4est_locidx_t Q = (p4est_locidx_t) tquadrants->elem_count;
#pragma omp parallel for
            for (p4est_locidx_t e = nodes->global_offset; e < Q+nodes->global_offset; e++)
            {
                p4est_quadrant_t * quad = p4est_quadrant_array_index(tquadrants, e);

                if(quad->x == 0)
                {
                    interpolateData.direction=0;
                    p4est_iterate(p4est, nodes_ghost, &interpolateData, get_interpolateNodesOnFacesWorker_data, NULL, NULL);
                    S* o = out.getSampleDataRW(e, sentinel);
                    for (index_t i=0; i < numComp; ++i) {
                        o[INDEX2(i,numComp,0)] = c0*fxx[INDEX3(1,i,e,numComp,numNodes)] + c1*fxx[INDEX3(0,i,e,numComp,numNodes)];
                        o[INDEX2(i,numComp,1)] = c0*fxx[INDEX3(0,i,e,numComp,numNodes)] + c1*fxx[INDEX3(1,i,e,numComp,numNodes)];
                    }
                }

                if(quad->x == P4EST_ROOT_LEN)
                {
                    interpolateData.direction=1;
                    p4est_iterate(p4est, nodes_ghost, &interpolateData, get_interpolateNodesOnFacesWorker_data, NULL, NULL);
                    S* o = out.getSampleDataRW(e, sentinel);
                    for (index_t i=0; i < numComp; ++i) {
                        o[INDEX2(i,numComp,0)] = c1*fxx[INDEX3(2,i,e,numComp,numNodes)] + c0*fxx[INDEX3(3,i,e,numComp,numNodes)];
                        o[INDEX2(i,numComp,1)] = c1*fxx[INDEX3(3,i,e,numComp,numNodes)] + c0*fxx[INDEX3(2,i,e,numComp,numNodes)];
                    }
                }

                if(quad->y == 0)
                {
                    interpolateData.direction=2;
                    p4est_iterate(p4est, nodes_ghost, &interpolateData, get_interpolateNodesOnFacesWorker_data, NULL, NULL);
                    S* o = out.getSampleDataRW(e, sentinel);
                    for (index_t i=0; i < numComp; ++i) {
                        o[INDEX2(i,numComp,0)] = c0*fxx[INDEX3(2,i,e,numComp,numNodes)] + c1*fxx[INDEX3(0,i,e,numComp,numNodes)];
                        o[INDEX2(i,numComp,1)] = c0*fxx[INDEX3(0,i,e,numComp,numNodes)] + c1*fxx[INDEX3(2,i,e,numComp,numNodes)];
                    }
                }

                if(quad->y == P4EST_ROOT_LEN)
                {
                    interpolateData.direction=3;
                    p4est_iterate(p4est, nodes_ghost, &interpolateData, get_interpolateNodesOnFacesWorker_data, NULL, NULL);
                    S* o = out.getSampleDataRW(e, sentinel);
                    for (index_t i=0; i < numComp; ++i) {
                        o[INDEX2(i,numComp,0)] = c0*fxx[INDEX3(3,i,e,numComp,numNodes)] + c1*fxx[INDEX3(1,i,e,numComp,numNodes)];
                        o[INDEX2(i,numComp,1)] = c0*fxx[INDEX3(1,i,e,numComp,numNodes)] + c1*fxx[INDEX3(3,i,e,numComp,numNodes)];
                    }
                }
            }
        }
    }
}

////////////////////////////// inline methods ////////////////////////////////
inline dim_t Rectangle::getDofOfNode(dim_t node) const
{
    //AEAE todo 
    return -1;
    // return m_dofMap[node];
}

// //protected
// inline dim_t Rectangle::getNumDOFInAxis(unsigned axis) const
// {
//     ESYS_ASSERT(axis < m_numDim, "Invalid axis");
//     return (m_gNE[axis]+1)/m_NX[axis];
//     // return 0; //AEAE todo
// }

// protected
// inline index_t Rectangle::getFirstInDim(unsigned axis) const
// {
//     // return m_offset[axis] == 0 ? 0 : 1;
//     return nodes->global_offset == 0 ? 0 : 1;
// }

//protected
inline dim_t Rectangle::getNumNodes() const
{
    double xy[2];
    double l0 = forestData->m_length[0]+forestData->m_origin[0];
    double l1 = forestData->m_length[1]+forestData->m_origin[1];
    p4est_locidx_t numnodes = 0;
    for (p4est_topidx_t t = p4est->first_local_tree; t <= p4est->last_local_tree; t++) 
    {
        p4est_tree_t * currenttree = p4est_tree_array_index(p4est->trees, t);
        sc_array_t * tquadrants = &currenttree->quadrants;
        numnodes += (p4est_locidx_t) tquadrants->elem_count;
        for (p4est_locidx_t e = 0; e < tquadrants->elem_count; e++)
        {
            p4est_quadrant_t * q = p4est_quadrant_array_index(tquadrants, e);
            p4est_qcoord_t length = P4EST_QUADRANT_LEN(q->level);

            p4est_qcoord_to_vertex(p4est->connectivity, t, q->x+length,q->y,xy);
            if(xy[0] == l0){
                numnodes++;
            }
            p4est_qcoord_to_vertex(p4est->connectivity, t, q->x,q->y+length,xy);
            if(xy[1] == l1){
                numnodes++;
            }
            p4est_qcoord_to_vertex(p4est->connectivity, t, q->x+length,q->y+length,xy);
            if(xy[0] == l0 && xy[1] == l1){
                numnodes++;
            }
        }
    }
    return numnodes;
}

//protected
inline dim_t Rectangle::getNumElements() const
{
    return p4est->global_num_quadrants;
}

//protected
inline dim_t Rectangle::getNumFaceElements() const
{
    double xy[2];
    double x0 = forestData->m_origin[0];
    double y0 = forestData->m_origin[1];
    double x1 = forestData->m_length[0]+forestData->m_origin[0];
    double y1 = forestData->m_length[1]+forestData->m_origin[1];
    p4est_locidx_t numFaceElements = 0;
    for (p4est_topidx_t t = p4est->first_local_tree; t <= p4est->last_local_tree; t++) 
    {
        p4est_tree_t * currenttree = p4est_tree_array_index(p4est->trees, t);
        sc_array_t * tquadrants = &currenttree->quadrants;
        for (p4est_locidx_t e = 0; e < tquadrants->elem_count; e++)
        {
            p4est_quadrant_t * q = p4est_quadrant_array_index(tquadrants, e);
            p4est_qcoord_t length = P4EST_QUADRANT_LEN(q->level);
            p4est_qcoord_to_vertex(p4est->connectivity, t, q->x,q->y,xy);
            if(xy[0] == x0 || xy[1] == y0){
                numFaceElements++;
                break;
            }
            p4est_qcoord_to_vertex(p4est->connectivity, t, q->x+length,q->y+length,xy);
            if(xy[0] == x1 || xy[1] == y1){
                numFaceElements++;
                break;
            }
        }
    }
    return numFaceElements;
}

//protected
inline dim_t Rectangle::getNumDOF() const
{
    throw OxleyException("getNumDOF not implemented");
    return -1;
}

#ifdef ESYS_HAVE_TRILINOS
//protected
esys_trilinos::const_TrilinosGraph_ptr Rectangle::getTrilinosGraph() const
{
    //AEAE todo

    // if (m_graph.is_null()) {
    //     m_graph = createTrilinosGraph(m_dofId, m_nodeId);
    // }
    return m_graph;
}
#endif

#ifdef ESYS_HAVE_PASO
//protected
paso::SystemMatrixPattern_ptr Rectangle::getPasoMatrixPattern(
                                                    bool reducedRowOrder,
                                                    bool reducedColOrder) const
{
    if (m_pattern.get())
        return m_pattern;

    // first call - create pattern, then return
    paso::Connector_ptr conn(getPasoConnector());
    const dim_t numDOF = getNumDOF();
    const dim_t numShared = conn->send->numSharedComponents;
    const dim_t numNeighbours = conn->send->neighbour.size();
    const std::vector<index_t>& offsetInShared(conn->send->offsetInShared);
    const index_t* sendShared = conn->send->shared;

    // these are for the couple blocks
    std::vector<IndexVector> colIndices(numDOF);
    std::vector<IndexVector> rowIndices(numShared);

    for (dim_t i=0; i<numNeighbours; i++) {
        const dim_t start = offsetInShared[i];
        const dim_t end = offsetInShared[i+1];
        for (dim_t j = start; j < end; j++) {
            if (j > start)
                doublyLink(colIndices, rowIndices, sendShared[j-1], j);
            doublyLink(colIndices, rowIndices, sendShared[j], j);
            if (j < end-1)
                doublyLink(colIndices, rowIndices, sendShared[j+1], j);
        }
    }
#pragma omp parallel for
    for (dim_t i = 0; i < numShared; i++) {
        sort(rowIndices[i].begin(), rowIndices[i].end());
    }

    // create main and couple blocks
    paso::Pattern_ptr mainPattern = createPasoPattern(getConnections(), numDOF);
    paso::Pattern_ptr colPattern = createPasoPattern(colIndices, numShared);
    paso::Pattern_ptr rowPattern = createPasoPattern(rowIndices, numDOF);

    // allocate paso distribution
    IndexVector m_nodeDistribution = getNodeDistribution();
    escript::Distribution_ptr distribution(new escript::Distribution(m_mpiInfo, m_nodeDistribution));

    // finally create the system matrix pattern
    m_pattern.reset(new paso::SystemMatrixPattern(MATRIX_FORMAT_DEFAULT,
            distribution, distribution, mainPattern, colPattern, rowPattern,
            conn, conn));
    return m_pattern;
}
#endif // ESYS_HAVE_PASO

//private
void Rectangle::populateSampleIds()
{
    // AEAE todo

//     // degrees of freedom are numbered from left to right, bottom to top in
//     // each rank, continuing on the next rank (ranks also go left-right,
//     // bottom-top).
//     // This means rank 0 has id 0...n0-1, rank 1 has id n0...n1-1 etc. which
//     // helps when writing out data rank after rank.

//     // build node distribution vector first.
//     // rank i owns m_nodeDistribution[i+1]-nodeDistribution[i] nodes which is
//     // constant for all ranks in this implementation
//     IndexVector m_nodeDistribution = getNodeDistribution();

//     m_nodeDistribution.assign(m_mpiInfo->size+1, 0);
//     const dim_t numDOF=getNumDOF();
//     for (dim_t k=1; k<m_mpiInfo->size; k++) {
//         m_nodeDistribution[k]=k*numDOF;
//     }
//     m_nodeDistribution[m_mpiInfo->size]=getNumDataPointsGlobal();
//     try {
//         m_nodeId.resize(getNumNodes());
//         m_dofId.resize(numDOF);
//         m_elementId.resize(getNumElements());
//     } catch (const std::length_error& le) {
//         throw RipleyException("The system does not have sufficient memory for a domain of this size.");
//     }

//     // populate face element counts
//     //left
//     if (forestData->m_offset[0]==0)
//         forestData->m_faceCount[0]=forestData->m_NE[1];
//     else
//         forestData->m_faceCount[0]=0;
//     //right
//     if (m_mpiInfo->rank%forestData->m_NX[0]==forestData->m_NX[0]-1)
//         forestData->m_faceCount[1]=forestData->m_NE[1];
//     else
//         forestData->m_faceCount[1]=0;
//     //bottom
//     if (forestData->m_offset[1]==0)
//         forestData->m_faceCount[2]=forestData->m_NE[0];
//     else
//         forestData->m_faceCount[2]=0;
//     //top
//     if (m_mpiInfo->rank/forestData->m_NX[0]==forestData->m_NX[1]-1)
//         forestData->m_faceCount[3]=m_NE[0];
//     else
//         forestData->m_faceCount[3]=0;

//     const dim_t NFE = getNumFaceElements();
//     m_faceId.resize(NFE);

//     const index_t left = getFirstInDim(0);
//     const index_t bottom = getFirstInDim(1);
//     const dim_t nDOF0 = getNumDOFInAxis(0);
//     const dim_t nDOF1 = getNumDOFInAxis(1);
//     const dim_t NE0 = m_NE[0];
//     const dim_t NE1 = m_NE[1];

// #define globalNodeId(x,y) \
//     ((m_offset[0]+x)/nDOF0)*nDOF0*nDOF1+(m_offset[0]+x)%nDOF0 \
//     + ((m_offset[1]+y)/nDOF1)*nDOF0*nDOF1*m_NX[0]+((m_offset[1]+y)%nDOF1)*nDOF0

//     // set corner id's outside the parallel region
//     m_nodeId[0] = globalNodeId(0, 0);
//     m_nodeId[m_NN[0]-1] = globalNodeId(m_NN[0]-1, 0);
//     m_nodeId[m_NN[0]*(m_NN[1]-1)] = globalNodeId(0, m_NN[1]-1);
//     m_nodeId[m_NN[0]*m_NN[1]-1] = globalNodeId(m_NN[0]-1,m_NN[1]-1);
// #undef globalNodeId

// #pragma omp parallel
//     {
//         // populate degrees of freedom and own nodes (identical id)
// #pragma omp for nowait
//         for (dim_t i=0; i<nDOF1; i++) {
//             for (dim_t j=0; j<nDOF0; j++) {
//                 const index_t nodeIdx=j+left+(i+bottom)*m_NN[0];
//                 const index_t dofIdx=j+i*nDOF0;
//                 m_dofId[dofIdx] = m_nodeId[nodeIdx]
//                     = m_nodeDistribution[m_mpiInfo->rank]+dofIdx;
//             }
//         }

//         // populate the rest of the nodes (shared with other ranks)
//         if (m_faceCount[0]==0) { // left column
// #pragma omp for nowait
//             for (dim_t i=0; i<nDOF1; i++) {
//                 const index_t nodeIdx=(i+bottom)*m_NN[0];
//                 const index_t dofId=(i+1)*nDOF0-1;
//                 m_nodeId[nodeIdx]
//                     = m_nodeDistribution[m_mpiInfo->rank-1]+dofId;
//             }
//         }
//         if (m_faceCount[1]==0) { // right column
// #pragma omp for nowait
//             for (dim_t i=0; i<nDOF1; i++) {
//                 const index_t nodeIdx=(i+bottom+1)*m_NN[0]-1;
//                 const index_t dofId=i*nDOF0;
//                 m_nodeId[nodeIdx]
//                     = m_nodeDistribution[m_mpiInfo->rank+1]+dofId;
//             }
//         }
//         if (m_faceCount[2]==0) { // bottom row
// #pragma omp for nowait
//             for (dim_t i=0; i<nDOF0; i++) {
//                 const index_t nodeIdx=i+left;
//                 const index_t dofId=nDOF0*(nDOF1-1)+i;
//                 m_nodeId[nodeIdx]
//                     = m_nodeDistribution[m_mpiInfo->rank-m_NX[0]]+dofId;
//             }
//         }
//         if (m_faceCount[3]==0) { // top row
// #pragma omp for nowait
//             for (dim_t i=0; i<nDOF0; i++) {
//                 const index_t nodeIdx=m_NN[0]*(m_NN[1]-1)+i+left;
//                 const index_t dofId=i;
//                 m_nodeId[nodeIdx]
//                     = m_nodeDistribution[m_mpiInfo->rank+m_NX[0]]+dofId;
//             }
//         }

//         // populate element id's
// #pragma omp for nowait
//         for (dim_t i1=0; i1<NE1; i1++) {
//             for (dim_t i0=0; i0<NE0; i0++) {
//                 m_elementId[i0+i1*NE0]=(m_offset[1]+i1)*m_gNE[0]+m_offset[0]+i0;
//             }
//         }

//         // face elements
// #pragma omp for
//         for (dim_t k=0; k<NFE; k++)
//             m_faceId[k]=k;
//     } // end parallel section

//     m_nodeTags.assign(getNumNodes(), 0);
//     updateTagsInUse(Nodes);

//     m_elementTags.assign(getNumElements(), 0);
//     updateTagsInUse(Elements);

//     // generate face offset vector and set face tags
//     const index_t LEFT=1, RIGHT=2, BOTTOM=10, TOP=20;
//     const index_t faceTag[] = { LEFT, RIGHT, BOTTOM, TOP };
//     m_faceOffset.assign(4, -1);
//     m_faceTags.clear();
//     index_t offset=0;
//     for (size_t i=0; i<4; i++) {
//         if (m_faceCount[i]>0) {
//             m_faceOffset[i]=offset;
//             offset+=m_faceCount[i];
//             m_faceTags.insert(m_faceTags.end(), m_faceCount[i], faceTag[i]);
//         }
//     }

//     setTagMap("left", LEFT);
//     setTagMap("right", RIGHT);
//     setTagMap("bottom", BOTTOM);
//     setTagMap("top", TOP);
//     updateTagsInUse(FaceElements);

//     populateDofMap();
}

// This is a wrapper that converts the p4est node information into an IndexVector
IndexVector Rectangle::getNodeDistribution() const
{
    IndexVector m_nodeDistribution;

#pragma omp parallel for
    for(int n = 0; n < connectivity->num_vertices; n++)
        m_nodeDistribution[n] = connectivity->corner_to_corner[n];

    return m_nodeDistribution;
}

//private
std::vector<IndexVector> Rectangle::getConnections(bool includeShared) const
{
    // returns a vector v of size numMatrixRows where v[i] is a vector with indices
    // of DOFs connected to i 
    // In other words this method returns the occupied (local) matrix columns
    // for all (local) matrix rows.
    // If includeShared==true then connections to non-owned DOFs are also
    // returned (i.e. indices of the column couplings)

    const dim_t numMatrixRows = getNumNodes();
    std::vector<IndexVector> indices(numMatrixRows);

    if (includeShared) {
        throw OxleyException("Not implemented"); //AEAE todo
    } else {
#pragma omp parallel for
        for(int i = 0; i < connectivity->num_vertices; i++)
            indices[i].push_back(connectivity->corner_to_corner[i]);
    }
    return indices;
}

bool Rectangle::operator==(const AbstractDomain& other) const
{
    const Rectangle* o=dynamic_cast<const Rectangle*>(&other);
    if (o) {
        return ((p4est_checksum(p4est) == p4est_checksum(o->p4est))
            && (forestData == o->forestData));
    }
    return false;
}

//protected
void Rectangle::assembleGradient(escript::Data& out,
                                 const escript::Data& in) const
{
    if (out.isComplex() && in.isComplex())
        assembleGradientImpl<cplx_t>(out, in);
    else if (!out.isComplex() && !in.isComplex())
        assembleGradientImpl<real_t>(out, in);
    else
        throw ValueError("Gradient: input & output complexity must match.");
}

//protected
template<typename Scalar>
void Rectangle::assembleGradientImpl(escript::Data& out,
                                     const escript::Data& in) const
{
    //AEAE todo

    const dim_t numComp = in.getDataPointSize();
    
    // Find the maximum level of refinement in the mesh
    int max_level = 0;
    for(p4est_topidx_t tree = p4est->first_local_tree; tree < p4est->last_local_tree; tree++) {
        p4est_tree_t * tree_t = p4est_tree_array_index(p4est->trees, tree);
        max_level = SC_MAX(max_level, tree_t->maxlevel);
    }
    
    double cx[3][P4EST_MAXLEVEL] = {{0}};
    double cy[3][P4EST_MAXLEVEL] = {{0}};
#pragma omp parallel for
    for(int i = 0; i < max_level; i++)
    {
        cx[0][i] = 0.21132486540518711775/forestData->m_dx[0][i];
        cx[1][i] = 0.78867513459481288225/forestData->m_dx[0][i];
        cx[2][i] = 1./forestData->m_dx[0][i];
        cy[0][i] = 0.21132486540518711775/forestData->m_dx[1][i];
        cy[1][i] = 0.78867513459481288225/forestData->m_dx[1][i];
        cy[2][i] = 1./forestData->m_dx[1][i];
    }
    const Scalar zero = static_cast<Scalar>(0);

    if (out.getFunctionSpace().getTypeCode() == Elements) {
        out.requireWrite();

//         for (p4est_topidx_t t = domain->p4est->first_local_tree; t <= domain->p4est->last_local_tree; t++) // Loop over every tree
//         {
//             p4est_tree_t * currenttree = p4est_tree_array_index(domain->p4est->trees, t);
//             sc_array_t * tquadrants = &currenttree->quadrants;
//             p4est_locidx_t Q = (p4est_locidx_t) tquadrants->elem_count;
// #pragma omp parallel for
//             for (p4est_locidx_t e = domain->nodes->global_offset; e < Q+domain->nodes->global_offset; e++) // Loop over every quadrant within the tree
//             {
//                 p4est_quadrant_t * quad = p4est_quadrant_array_index(tquadrants, e);
//                 quadrantData * quaddata = (quadrantData *) quad->p.user_data;

//                 Scalar* o = out.getSampleDataRW(INDEX2(k0,k1,m_NE[0]), zero);
                // for (index_t i = 0; i < numComp; ++i) {
                //     o[INDEX3(i,0,0,numComp,2)] = (f_10[i]-f_00[i])*cx1 + (f_11[i]-f_01[i])*cx0;
                //     o[INDEX3(i,1,0,numComp,2)] = (f_01[i]-f_00[i])*cy1 + (f_11[i]-f_10[i])*cy0;
                //     o[INDEX3(i,0,1,numComp,2)] = (f_10[i]-f_00[i])*cx1 + (f_11[i]-f_01[i])*cx0;
                //     o[INDEX3(i,1,1,numComp,2)] = (f_01[i]-f_00[i])*cy0 + (f_11[i]-f_10[i])*cy1;
                //     o[INDEX3(i,0,2,numComp,2)] = (f_10[i]-f_00[i])*cx0 + (f_11[i]-f_01[i])*cx1;
                //     o[INDEX3(i,1,2,numComp,2)] = (f_01[i]-f_00[i])*cy1 + (f_11[i]-f_10[i])*cy0;
                //     o[INDEX3(i,0,3,numComp,2)] = (f_10[i]-f_00[i])*cx0 + (f_11[i]-f_01[i])*cx1;
                //     o[INDEX3(i,1,3,numComp,2)] = (f_01[i]-f_00[i])*cy0 + (f_11[i]-f_10[i])*cy1;
                // } 
    //         }
    //     }
    // } else if (out.getFunctionSpace().getTypeCode() == ReducedElements) {
    //     out.requireWrite();
// #pragma omp parallel
//         {
//             vector<Scalar> f_00(numComp, zero);
//             vector<Scalar> f_01(numComp, zero);
//             vector<Scalar> f_10(numComp, zero);
//             vector<Scalar> f_11(numComp, zero);
// #pragma omp for
//             for (index_t k1 = 0; k1 < NE1; ++k1) {
//                 for (index_t k0 = 0; k0 < NE0; ++k0) {
//                     memcpy(&f_00[0], in.getSampleDataRO(INDEX2(k0,k1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_01[0], in.getSampleDataRO(INDEX2(k0,k1+1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_10[0], in.getSampleDataRO(INDEX2(k0+1,k1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_11[0], in.getSampleDataRO(INDEX2(k0+1,k1+1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     Scalar* o = out.getSampleDataRW(INDEX2(k0,k1,m_NE[0]), zero);
//                     for (index_t i = 0; i < numComp; ++i) {
//                         o[INDEX3(i,0,0,numComp,2)] = (f_10[i] + f_11[i] - f_00[i] - f_01[i])*cx2 * 0.5;
//                         o[INDEX3(i,1,0,numComp,2)] = (f_01[i] + f_11[i] - f_00[i] - f_10[i])*cy2 * 0.5;
//                     } // end of component loop i
//                 } // end of k0 loop
//             } // end of k1 loop
//         } // end of parallel section
//     } else if (out.getFunctionSpace().getTypeCode() == FaceElements) {
//         out.requireWrite();
// #pragma omp parallel
//         {
//             vector<Scalar> f_00(numComp, zero);
//             vector<Scalar> f_01(numComp, zero);
//             vector<Scalar> f_10(numComp, zero);
//             vector<Scalar> f_11(numComp, zero);
//             if (m_faceOffset[0] > -1) {
// #pragma omp for nowait
//                 for (index_t k1 = 0; k1 < NE1; ++k1) {
//                     memcpy(&f_00[0], in.getSampleDataRO(INDEX2(0,k1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_01[0], in.getSampleDataRO(INDEX2(0,k1+1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_10[0], in.getSampleDataRO(INDEX2(1,k1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_11[0], in.getSampleDataRO(INDEX2(1,k1+1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     Scalar* o = out.getSampleDataRW(m_faceOffset[0]+k1, zero);
//                     for (index_t i = 0; i < numComp; ++i) {
//                         o[INDEX3(i,0,0,numComp,2)] = (f_10[i]-f_00[i])*cx1 + (f_11[i]-f_01[i])*cx0;
//                         o[INDEX3(i,1,0,numComp,2)] = (f_01[i]-f_00[i])*cy2;
//                         o[INDEX3(i,0,1,numComp,2)] = (f_10[i]-f_00[i])*cx0 + (f_11[i]-f_01[i])*cx1;
//                         o[INDEX3(i,1,1,numComp,2)] = (f_01[i]-f_00[i])*cy2;
//                     } // end of component loop i
//                 } // end of k1 loop
//             } // end of face 0
//             if (m_faceOffset[1] > -1) {
// #pragma omp for nowait
//                 for (index_t k1 = 0; k1 < NE1; ++k1) {
//                     memcpy(&f_00[0], in.getSampleDataRO(INDEX2(m_NN[0]-2,k1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_01[0], in.getSampleDataRO(INDEX2(m_NN[0]-2,k1+1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_10[0], in.getSampleDataRO(INDEX2(m_NN[0]-1,k1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_11[0], in.getSampleDataRO(INDEX2(m_NN[0]-1,k1+1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     Scalar* o = out.getSampleDataRW(m_faceOffset[1]+k1, zero);
//                     for (index_t i = 0; i < numComp; ++i) {
//                         o[INDEX3(i,0,0,numComp,2)] = (f_10[i]-f_00[i])*cx1 + (f_11[i]-f_01[i])*cx0;
//                         o[INDEX3(i,1,0,numComp,2)] = (f_11[i]-f_10[i])*cy2;
//                         o[INDEX3(i,0,1,numComp,2)] = (f_10[i]-f_00[i])*cx0 + (f_11[i]-f_01[i])*cx1;
//                         o[INDEX3(i,1,1,numComp,2)] = (f_11[i]-f_10[i])*cy2;
//                     } // end of component loop i
//                 } // end of k1 loop
//             } // end of face 1
//             if (m_faceOffset[2] > -1) {
// #pragma omp for nowait
//                 for (index_t k0 = 0; k0 < NE0; ++k0) {
//                     memcpy(&f_00[0], in.getSampleDataRO(INDEX2(k0,0, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_01[0], in.getSampleDataRO(INDEX2(k0,1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_10[0], in.getSampleDataRO(INDEX2(k0+1,0, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_11[0], in.getSampleDataRO(INDEX2(k0+1,1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     Scalar* o = out.getSampleDataRW(m_faceOffset[2]+k0, zero);
//                     for (index_t i = 0; i < numComp; ++i) {
//                         o[INDEX3(i,0,0,numComp,2)] = (f_10[i]-f_00[i])*cx2;
//                         o[INDEX3(i,1,0,numComp,2)] = (f_01[i]-f_00[i])*cy1 + (f_11[i]-f_10[i])*cy0;
//                         o[INDEX3(i,0,1,numComp,2)] = (f_10[i]-f_00[i])*cx2;
//                         o[INDEX3(i,1,1,numComp,2)] = (f_01[i]-f_00[i])*cy0 + (f_11[i]-f_10[i])*cy1;
//                     } // end of component loop i
//                 } // end of k0 loop
//             } // end of face 2
//             if (m_faceOffset[3] > -1) {
// #pragma omp for nowait
//                 for (index_t k0 = 0; k0 < NE0; ++k0) {
//                     memcpy(&f_00[0], in.getSampleDataRO(INDEX2(k0,m_NN[1]-2, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_01[0], in.getSampleDataRO(INDEX2(k0,m_NN[1]-1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_10[0], in.getSampleDataRO(INDEX2(k0+1,m_NN[1]-2, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_11[0], in.getSampleDataRO(INDEX2(k0+1,m_NN[1]-1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     Scalar* o = out.getSampleDataRW(m_faceOffset[3]+k0, zero);
//                     for (index_t i = 0; i < numComp; ++i) {
//                         o[INDEX3(i,0,0,numComp,2)] = (f_11[i]-f_01[i])*cx2;
//                         o[INDEX3(i,1,0,numComp,2)] = (f_01[i]-f_00[i])*cy1 + (f_11[i]-f_10[i])*cy0;
//                         o[INDEX3(i,0,1,numComp,2)] = (f_11[i]-f_01[i])*cx2;
//                         o[INDEX3(i,1,1,numComp,2)] = (f_01[i]-f_00[i])*cy0 + (f_11[i]-f_10[i])*cy1;
//                     } // end of component loop i
//                 } // end of k0 loop
//             } // end of face 3
//         } // end of parallel section

    } else if (out.getFunctionSpace().getTypeCode() == ReducedFaceElements) {
        out.requireWrite();
// #pragma omp parallel
//         {
//             vector<Scalar> f_00(numComp, zero);
//             vector<Scalar> f_01(numComp, zero);
//             vector<Scalar> f_10(numComp, zero);
//             vector<Scalar> f_11(numComp, zero);
//             if (m_faceOffset[0] > -1) {
// #pragma omp for nowait
//                 for (index_t k1 = 0; k1 < NE1; ++k1) {
//                     memcpy(&f_00[0], in.getSampleDataRO(INDEX2(0,k1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_01[0], in.getSampleDataRO(INDEX2(0,k1+1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_10[0], in.getSampleDataRO(INDEX2(1,k1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_11[0], in.getSampleDataRO(INDEX2(1,k1+1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     Scalar* o = out.getSampleDataRW(m_faceOffset[0]+k1, zero);
//                     for (index_t i = 0; i < numComp; ++i) {
//                         o[INDEX3(i,0,0,numComp,2)] = (f_10[i] + f_11[i] - f_00[i] - f_01[i])*cx2 * 0.5;
//                         o[INDEX3(i,1,0,numComp,2)] = (f_01[i]-f_00[i])*cy2;
//                     } // end of component loop i
//                 } // end of k1 loop
//             } // end of face 0
//             if (m_faceOffset[1] > -1) {
// #pragma omp for nowait
//                 for (index_t k1 = 0; k1 < NE1; ++k1) {
//                     memcpy(&f_00[0], in.getSampleDataRO(INDEX2(m_NN[0]-2,k1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_01[0], in.getSampleDataRO(INDEX2(m_NN[0]-2,k1+1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_10[0], in.getSampleDataRO(INDEX2(m_NN[0]-1,k1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_11[0], in.getSampleDataRO(INDEX2(m_NN[0]-1,k1+1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     Scalar* o = out.getSampleDataRW(m_faceOffset[1]+k1, zero);
//                     for (index_t i = 0; i < numComp; ++i) {
//                         o[INDEX3(i,0,0,numComp,2)] = (f_10[i] + f_11[i] - f_00[i] - f_01[i])*cx2 * 0.5;
//                         o[INDEX3(i,1,0,numComp,2)] = (f_11[i]-f_10[i])*cy2;
//                     } // end of component loop i
//                 } // end of k1 loop
//             } // end of face 1
//             if (m_faceOffset[2] > -1) {
// #pragma omp for nowait
//                 for (index_t k0 = 0; k0 < NE0; ++k0) {
//                     memcpy(&f_00[0], in.getSampleDataRO(INDEX2(k0,0, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_01[0], in.getSampleDataRO(INDEX2(k0,1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_10[0], in.getSampleDataRO(INDEX2(k0+1,0, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_11[0], in.getSampleDataRO(INDEX2(k0+1,1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     Scalar* o = out.getSampleDataRW(m_faceOffset[2]+k0, zero);
//                     for (index_t i = 0; i < numComp; ++i) {
//                         o[INDEX3(i,0,0,numComp,2)] = (f_10[i]-f_00[i])*cx2;
//                         o[INDEX3(i,1,0,numComp,2)] = (f_01[i] + f_11[i] - f_00[i] - f_10[i])*cy2 * 0.5;
//                     } // end of component loop i
//                 } // end of k0 loop
//             } // end of face 2
//             if (m_faceOffset[3] > -1) {
// #pragma omp for nowait
//                 for (index_t k0 = 0; k0 < NE0; ++k0) {
//                     memcpy(&f_00[0], in.getSampleDataRO(INDEX2(k0,m_NN[1]-2, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_01[0], in.getSampleDataRO(INDEX2(k0,m_NN[1]-1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_10[0], in.getSampleDataRO(INDEX2(k0+1,m_NN[1]-2, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     memcpy(&f_11[0], in.getSampleDataRO(INDEX2(k0+1,m_NN[1]-1, m_NN[0]), zero), numComp*sizeof(Scalar));
//                     Scalar* o = out.getSampleDataRW(m_faceOffset[3]+k0, zero);
//                     for (index_t i = 0; i < numComp; ++i) {
//                         o[INDEX3(i,0,0,numComp,2)] = (f_11[i]-f_01[i])*cx2;
//                         o[INDEX3(i,1,0,numComp,2)] = (f_01[i] + f_11[i] - f_00[i] - f_10[i])*cy2 * 0.5;
//                     } // end of component loop i
//                 } // end of k0 loop
//             } // end of face 3
//         } // end of parallel section
    }
}

dim_t Rectangle::findNode(const double *coords) const
{
    //AEAE todo
    return -1;

    // const dim_t NOT_MINE = -1;
    // //is the found element even owned by this rank
    // // (inside owned or shared elements but will map to an owned element)
    // for (int dim = 0; dim < m_numDim; dim++) {
    //     //allows for point outside mapping onto node
    //     double min = m_origin[dim] + m_offset[dim]* m_dx[dim]
    //             - m_dx[dim]/2. + escript::DataTypes::real_t_eps();
    //     double max = m_origin[dim] + (m_offset[dim] + m_NE[dim])*m_dx[dim]
    //             + m_dx[dim]/2. - escript::DataTypes::real_t_eps();
    //     if (min > coords[dim] || max < coords[dim]) {
    //         return NOT_MINE;
    //     }
    // }
    // // get distance from origin
    // double x = coords[0] - m_origin[0];
    // double y = coords[1] - m_origin[1];

    // //check if the point is even inside the domain
    // if (x < 0 || y < 0 || x > m_length[0] || y > m_length[1])
    //     return NOT_MINE;

    // // distance in elements
    // dim_t ex = (dim_t) floor((x + 0.01*m_dx[0]) / m_dx[0]);
    // dim_t ey = (dim_t) floor((y + 0.01*m_dx[1]) / m_dx[1]);
    // // set the min distance high enough to be outside the element plus a bit
    // dim_t closest = NOT_MINE;
    // double minDist = 1;
    // for (int dim = 0; dim < m_numDim; dim++) {
    //     minDist += m_dx[dim]*m_dx[dim];
    // }
    // //find the closest node
    // for (int dx = 0; dx < 1; dx++) {
    //     double xdist = (x - (ex + dx)*m_dx[0]);
    //     for (int dy = 0; dy < 1; dy++) {
    //         double ydist = (y - (ey + dy)*m_dx[1]);
    //         double total = xdist*xdist + ydist*ydist;
    //         if (total < minDist) {
    //             closest = INDEX2(ex+dx-m_offset[0], ey+dy-m_offset[1], m_NN[0]);
    //             minDist = total;
    //         }
    //     }
    // }
    // //if this happens, we've let a dirac point slip through, which is awful
    // if (closest == NOT_MINE) {
    //     throw RipleyException("Unable to map appropriate dirac point to a node,"
    //             " implementation problem in Rectangle::findNode()");
    // }
    // return closest;
}

//protected
void Rectangle::nodesToDOF(escript::Data& out, const escript::Data& in) const
{
    //AEAE todo

//     const dim_t numComp = in.getDataPointSize();
//     out.requireWrite();

//     const index_t left = getFirstInDim(0);
//     const index_t bottom = getFirstInDim(1);
//     const dim_t nDOF0 = getNumDOFInAxis(0);
//     const dim_t nDOF1 = getNumDOFInAxis(1);
// #pragma omp parallel for
//     for (index_t i=0; i<nDOF1; i++) {
//         for (index_t j=0; j<nDOF0; j++) {
//             const index_t n=j+left+(i+bottom)*m_NN[0];
//             const double* src=in.getSampleDataRO(n);
//             copy(src, src+numComp, out.getSampleDataRW(j+i*nDOF0));
//         }
//     }
}

// Updates m_faceOffset for each quadrant
void Rectangle::updateFaceOffset()
{
    p4est_iterate(p4est, nodes_ghost, NULL, update_node_faceoffset, NULL, NULL);
}

// instantiate our two supported versions
template
void Rectangle::assembleGradientImpl<real_t>(escript::Data& out,
                                             const escript::Data& in) const;

template
void Rectangle::assembleGradientImpl<cplx_t>(escript::Data& out,
                                             const escript::Data& in) const;


} // end of namespace oxley
