
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

/* Paso: TransportProblem (see TransportSolver::solve)                      */

/****************************************************************************/

/* Author: l.gross@uq.edu.au                                                */

/****************************************************************************/

#include "Transport.h"
#include "PasoException.h"
#include "PasoUtil.h"
#include "Preconditioner.h"
#include "Solver.h" // only for resetting

#include <escript/Data.h>

#include <limits>

namespace bp = boost::python;

namespace paso {

TransportProblem::TransportProblem(SystemMatrixPattern_ptr pattern,
                                   int block_size,
                                   const escript::FunctionSpace& functionspace) :
    AbstractTransportProblem(block_size, functionspace),
    valid_matrices(false),
    dt_max_R(LARGE_POSITIVE_FLOAT),
    dt_max_T(LARGE_POSITIVE_FLOAT),
    constraint_mask(NULL),
    main_diagonal_low_order_transport_matrix(NULL),
    lumped_mass_matrix(NULL),
    reactive_matrix(NULL),
    main_diagonal_mass_matrix(NULL)
{
    // at the moment only block size 1 is supported
    SystemMatrixType matrix_type = MATRIX_FORMAT_DEFAULT+MATRIX_FORMAT_BLK1;

    transport_matrix.reset(new SystemMatrix(matrix_type, pattern, block_size,
                                            block_size, false,
                                            functionspace, functionspace));
    mass_matrix.reset(new SystemMatrix(matrix_type, pattern, block_size,
                                       block_size, false, functionspace,
                                       functionspace));

    mpi_info = pattern->mpi_info;

    if (Esys_noError()) {
        const dim_t n = transport_matrix->getTotalNumRows();
        constraint_mask = new double[n];
        lumped_mass_matrix = new double[n];
        reactive_matrix = new double[n];
        main_diagonal_mass_matrix = new double[n];
        main_diagonal_low_order_transport_matrix = new double[n];

#pragma omp parallel for
        for (dim_t i = 0; i < n; ++i) {
            lumped_mass_matrix[i] = 0.;
            main_diagonal_low_order_transport_matrix[i] = 0.;
            constraint_mask[i] = 0.;
        }
    }
}

TransportProblem::~TransportProblem()
{
    delete[] constraint_mask;
    delete[] reactive_matrix;
    delete[] main_diagonal_mass_matrix;
    delete[] lumped_mass_matrix;
    delete[] main_diagonal_low_order_transport_matrix;
}

void TransportProblem::setToSolution(escript::Data& out, escript::Data& u0,
                                     escript::Data& source, double dt,
                                     bp::object& options)
{
    Options paso_options(options);
    options.attr("resetDiagnostics")();
    if (out.getDataPointSize() != getBlockSize()) {
        throw PasoException("solve: block size of solution does not match block size of transport problems.");
    } else if (source.getDataPointSize() != getBlockSize()) {
        throw PasoException("solve: block size of source term does not match block size of transport problems.");
    } else if (out.getFunctionSpace() != getFunctionSpace()) {
        throw PasoException("solve: function spaces of solution and of transport problem don't match.");
    } else if (source.getFunctionSpace() != getFunctionSpace()) {
        throw PasoException("solve: function spaces of source term and of transport problem don't match.");
    } else if (dt <= 0.) {
        throw PasoException("solve: time increment dt needs to be positive.");
    }
    out.expand();
    source.expand();
    u0.expand();
    out.requireWrite();
    source.requireWrite();
    u0.requireWrite();
    double* out_dp = out.getSampleDataRW(0);
    double* u0_dp = u0.getSampleDataRW(0);
    double* source_dp = source.getSampleDataRW(0);
    solve(out_dp, dt, u0_dp, source_dp, &paso_options);
    paso_options.updateEscriptDiagnostics(options);
}

void TransportProblem::copyConstraint(escript::Data& source, escript::Data& q,
                                      escript::Data& r)
{
    if (q.getDataPointSize() != getBlockSize()) {
        throw PasoException("copyConstraint: block size does not match the number of components of constraint mask.");
    } else if (q.getFunctionSpace() != getFunctionSpace()) {
        throw PasoException("copyConstraint: function spaces of transport problem and constraint mask don't match.");
    } else if (r.getDataPointSize() != getBlockSize()) {
        throw PasoException("copyConstraint: block size does not match the number of components of constraint values.");
    } else if (r.getFunctionSpace() != getFunctionSpace()) {
        throw PasoException("copyConstraint: function spaces of transport problem and constraint values don't match.");
    } else if (source.getDataPointSize() != getBlockSize()) {
        throw PasoException("copyConstraint: block size does not match the number of components of source.");
    } else if (source.getFunctionSpace() != getFunctionSpace()) {
        throw PasoException("copyConstraint: function spaces of transport problem and source don't match.");
    }

#if 0
    // r2=r where q>0, 0 elsewhere
    escript::Data r2(0., q.getDataPointShape(), q.getFunctionSpace());
    r2.copyWithMask(r, q);

    // source -= tp->mass_matrix*r2
    r2.expand();
    source.expand();
    q.expand();
    r2.requireWrite();
    source.requireWrite();
    q.requireWrite();
    double* r2_dp = r2.getSampleDataRW(0);
    double* source_dp = source.getSampleDataRW(0);
    double* q_dp = q.getSampleDataRW(0);

    mass_matrix->MatrixVector(-1., r2_dp, 1., source_dp);
    checkPasoError();

    // insert 0 rows into transport matrix
    transport_matrix->nullifyRows(q_dp, 0.);
    checkPasoError();

    // insert 0 rows and 1 in main diagonal into mass matrix
    mass_matrix->nullifyRowsAndCols(q_dp, q_dp, 1.);
    checkPasoError();
    source.copyWithMask(escript::Data(0.,q.getDataPointShape(),q.getFunctionSpace()),q);
#else
    r.expand();
    source.expand();
    q.expand();
    r.requireWrite();
    source.requireWrite();
    q.requireWrite();
    double* r_dp = r.getSampleDataRW(0);
    double* source_dp = source.getSampleDataRW(0);
    double* q_dp = q.getSampleDataRW(0);
    setUpConstraint(q_dp);
    insertConstraint(r_dp, source_dp);
#endif
}

void TransportProblem::resetTransport() const
{
    const dim_t n = transport_matrix->getTotalNumRows();
    transport_matrix->setValues(0.);
    mass_matrix->setValues(0.);
    solve_free(iteration_matrix.get());
    util::zeroes(n, constraint_mask);
    valid_matrices = false;
}

double TransportProblem::getUnlimitedTimeStepSize() const
{
    return std::numeric_limits<double>::max();
}


void TransportProblem::setUpConstraint(const double* q)
{
    if (valid_matrices) {
        Esys_setError(VALUE_ERROR, "TransportProblem::setUpConstraint: "
                            "Cannot insert a constraint into a valid system.");
        return;
    }

    const dim_t n = transport_matrix->getTotalNumRows();
#pragma omp parallel for
    for (dim_t i=0; i<n; ++i) {
        if (q[i] > 0) {
            constraint_mask[i]=1;
        } else {
            constraint_mask[i]=0;
        }
    }
}

void TransportProblem::insertConstraint(const double* r,  double* source) const
{
    const dim_t n = transport_matrix->getTotalNumRows();

#pragma omp parallel for
    for (dim_t i=0; i<n; ++i) {
        if (constraint_mask[i] > 0)
            source[i] = r[i];
    }
}

} // namespace paso

