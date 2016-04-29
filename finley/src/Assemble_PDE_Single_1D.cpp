
/*****************************************************************************
*
* Copyright (c) 2003-2016 by The University of Queensland
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


/****************************************************************************

  Assembles a single PDE into the stiffness matrix S and right hand side F

      -(A_{i,j} u_,j)_i-(B_{i} u)_i+C_{j} u_,j-D u_m  and -(X_,i)_i + Y

  in a 1D domain. The shape functions for test and solution must be identical
  and row_NS == row_NN.

  Shape of the coefficients:

      A = 1 x 1
      B = 1
      C = 1
      D = scalar
      X = 1
      Y = scalar

*****************************************************************************/

#include "Assemble.h"
#include "Util.h"

#include <escript/index.h>

namespace finley {

void Assemble_PDE_Single_1D(const AssembleParameters& p,
                            const escript::Data& A, const escript::Data& B,
                            const escript::Data& C, const escript::Data& D,
                            const escript::Data& X, const escript::Data& Y)
{
    const int DIM = 1;
    bool expandedA = A.actsExpanded();
    bool expandedB = B.actsExpanded();
    bool expandedC = C.actsExpanded();
    bool expandedD = D.actsExpanded();
    bool expandedX = X.actsExpanded();
    bool expandedY = Y.actsExpanded();
    double *F_p = NULL;
    if(!p.F.isEmpty()) {
        p.F.requireWrite();
        F_p = p.F.getSampleDataRW(0);
    }
    const std::vector<double>& S(p.row_jac->BasisFunctions->S);
    const int len_EM_S = p.row_numShapesTotal*p.col_numShapesTotal;
    const int len_EM_F = p.row_numShapesTotal;

#pragma omp parallel
    {
        for (index_t color = p.elements->minColor; color <= p.elements->maxColor; color++) {
            // loop over all elements:
#pragma omp for
            for (index_t e = 0; e < p.elements->numElements; e++) {
                if (p.elements->Color[e] == color) {
                    for (int isub = 0; isub < p.numSub; isub++) {
                        const double* Vol = &p.row_jac->volume[INDEX3(0,isub,e,p.numQuadSub,p.numSub)];
                        const double* DSDX = &p.row_jac->DSDX[INDEX5(0,0,0,isub,e, p.row_numShapesTotal,DIM,p.numQuadSub,p.numSub)];
                        std::vector<double> EM_S(len_EM_S);
                        std::vector<double> EM_F(len_EM_F);
                        bool add_EM_F = false;
                        bool add_EM_S = false;
                        ///////////////
                        // process A //
                        ///////////////
                        if (!A.isEmpty()) {
                            const double *A_p = A.getSampleDataRO(e);
                            add_EM_S = true;
                            if (expandedA) {
                                const double* A_q = &(A_p[INDEX4(0,0,0,isub,DIM,DIM,p.numQuadSub)]);
                                for (int s = 0; s < p.row_numShapes; s++) {
                                    for (int r = 0; r < p.col_numShapes; r++) {
                                        double f = 0.;
                                        for (int q = 0; q < p.numQuadSub; q++) {
                                            f += Vol[q]*DSDX[INDEX3(s,0,q,p.row_numShapesTotal,DIM)]*A_q[INDEX3(0,0,q,DIM,DIM)]*DSDX[INDEX3(r,0,q,p.row_numShapesTotal,DIM)];
                                        }
                                        EM_S[INDEX4(0,0,s,r,p.numEqu,p.numComp,p.row_numShapesTotal)]+=f;
                                    }
                                }
                            } else { // constant A
                                for (int s = 0; s < p.row_numShapes; s++) {
                                    for (int r = 0; r < p.col_numShapes; r++) {
                                        double f = 0.;
                                        for (int q = 0; q < p.numQuadSub; q++)
                                            f += Vol[q]*DSDX[INDEX3(s,0,q,p.row_numShapesTotal,DIM)]*DSDX[INDEX3(r,0,q,p.row_numShapesTotal,DIM)];
                                        EM_S[INDEX4(0,0,s,r,p.numEqu,p.numComp,p.row_numShapesTotal)]+=f*A_p[INDEX2(0,0,DIM)];
                                    }
                                }
                            }
                        }
                        ///////////////
                        // process B //
                        ///////////////
                        if (!B.isEmpty()) {
                            const double *B_p=B.getSampleDataRO(e);
                            add_EM_S=true;
                            if (expandedB) {
                                const double *B_q=&(B_p[INDEX3(0,0,isub,DIM,p.numQuadSub)]);
                                for (int s=0; s<p.row_numShapes; s++) {
                                    for (int r=0; r<p.col_numShapes; r++) {
                                        double f=0.;
                                        for (int q=0; q<p.numQuadSub; q++) {
                                            f+=Vol[q]*DSDX[INDEX3(s,0,q,p.row_numShapesTotal,DIM)]*B_q[INDEX2(0,q,DIM)]*S[INDEX2(r,q,p.row_numShapes)];
                                        }
                                        EM_S[INDEX4(0,0,s,r,p.numEqu,p.numComp,p.row_numShapesTotal)]+=f;
                                    }
                                }
                            } else { // constant B
                                for (int s=0; s<p.row_numShapes; s++) {
                                    for (int r=0; r<p.col_numShapes; r++) {
                                        double f=0.;
                                        for (int q=0; q<p.numQuadSub; q++)
                                            f+=Vol[q]*DSDX[INDEX3(s,0,q,p.row_numShapesTotal,DIM)]*S[INDEX2(r,q,p.row_numShapes)];
                                        EM_S[INDEX4(0,0,s,r,p.numEqu,p.numComp,p.row_numShapesTotal)]+=f*B_p[0];
                                    }
                                }
                            }
                        }
                        ///////////////
                        // process C //
                        ///////////////
                        if (!C.isEmpty()) {
                            const double *C_p=C.getSampleDataRO(e);
                            add_EM_S=true;
                            if (expandedC) {
                                const double *C_q=&(C_p[INDEX3(0,0,isub,DIM,p.numQuadSub)]);
                                for (int s=0; s<p.row_numShapes; s++) {
                                    for (int r=0; r<p.col_numShapes; r++) {
                                        double f=0.;
                                        for (int q=0; q<p.numQuadSub; q++) {
                                            f+=Vol[q]*S[INDEX2(s,q,p.row_numShapes)]*C_q[INDEX2(0,q,DIM)]*DSDX[INDEX3(r,0,q,p.row_numShapesTotal,DIM)];
                                        }
                                        EM_S[INDEX4(0,0,s,r,p.numEqu,p.numComp,p.row_numShapesTotal)]+=f;
                                    }
                                }
                            } else { // constant C
                                for (int s=0; s<p.row_numShapes; s++) {
                                    for (int r=0; r<p.col_numShapes; r++) {
                                        double f=0.;
                                        for (int q=0; q<p.numQuadSub; q++)
                                            f+=Vol[q]*S[INDEX2(s,q,p.row_numShapes)]*DSDX[INDEX3(r,0,q,p.row_numShapesTotal,DIM)];
                                        EM_S[INDEX4(0,0,s,r,p.numEqu,p.numComp,p.row_numShapesTotal)]+=f*C_p[0];
                                    }
                                }
                            }
                        }
                        ///////////////
                        // process D //
                        ///////////////
                        if (!D.isEmpty()) {
                            const double *D_p=D.getSampleDataRO(e);
                            add_EM_S=true;
                            if (expandedD) {
                                const double *D_q=&(D_p[INDEX2(0,isub,p.numQuadSub)]);
                                for (int s=0; s<p.row_numShapes; s++) {
                                    for (int r=0; r<p.col_numShapes; r++) {
                                        double f=0.;
                                        for (int q=0; q<p.numQuadSub; q++) {
                                            f+=Vol[q]*S[INDEX2(s,q,p.row_numShapes)]*D_q[q]*S[INDEX2(r,q,p.row_numShapes)];
                                        }
                                        EM_S[INDEX4(0,0,s,r,p.numEqu,p.numComp,p.row_numShapesTotal)]+=f;
                                    }
                                }
                            } else { // constant D
                                for (int s=0; s<p.row_numShapes; s++) {
                                    for (int r=0; r<p.col_numShapes; r++) {
                                        double f=0.;
                                        for (int q=0; q<p.numQuadSub; q++)
                                            f+=Vol[q]*S[INDEX2(s,q,p.row_numShapes)]*S[INDEX2(r,q,p.row_numShapes)];
                                        EM_S[INDEX4(0,0,s,r,p.numEqu,p.numComp,p.row_numShapesTotal)]+=f*D_p[0];
                                    }
                                }
                            }
                        }
                        ///////////////
                        // process X //
                        ///////////////
                        if (!X.isEmpty()) {
                            const double *X_p=X.getSampleDataRO(e);
                            add_EM_F=true;
                            if (expandedX) {
                                const double *X_q=&(X_p[INDEX3(0,0,isub,DIM,p.numQuadSub)]);
                                for (int s=0; s<p.row_numShapes; s++) {
                                    double f=0.;
                                    for (int q=0; q<p.numQuadSub; q++)
                                        f+=Vol[q]*DSDX[INDEX3(s,0,q,p.row_numShapesTotal,DIM)]*X_q[INDEX2(0,q,DIM)];
                                    EM_F[INDEX2(0,s,p.numEqu)]+=f;
                                }
                            } else { // constant X
                                for (int s=0; s<p.row_numShapes; s++) {
                                    double f=0.;
                                    for (int q=0; q<p.numQuadSub; q++)
                                        f+=Vol[q]*DSDX[INDEX3(s,0,q,p.row_numShapesTotal,DIM)];
                                    EM_F[INDEX2(0,s,p.numEqu)]+=f*X_p[0];
                                }
                            }
                        }
                        ///////////////
                        // process Y //
                        ///////////////
                        if (!Y.isEmpty()) {
                            const double *Y_p=Y.getSampleDataRO(e);
                            add_EM_F=true;
                            if (expandedY) {
                                const double *Y_q=&(Y_p[INDEX2(0,isub, p.numQuadSub)]);
                                for (int s = 0; s < p.row_numShapes; s++) {
                                    double f = 0.;
                                    for (int q = 0; q < p.numQuadSub; q++)
                                        f += Vol[q]*S[INDEX2(s,q,p.row_numShapes)]*Y_q[q];
                                    EM_F[INDEX2(0,s,p.numEqu)]+=f;
                                }
                            } else { // constant Y
                                for (int s = 0; s < p.row_numShapes; s++) {
                                    double f = 0.;
                                    for (int q = 0; q < p.numQuadSub; q++)
                                        f+=Vol[q]*S[INDEX2(s,q,p.row_numShapes)];
                                    EM_F[INDEX2(0,s,p.numEqu)]+=f*Y_p[0];
                                }
                            }
                        }
                        // add the element matrices onto the matrix and
                        // right hand side
                        IndexVector row_index(p.row_numShapesTotal);
                        for (int q = 0; q < p.row_numShapesTotal; q++)
                            row_index[q] = p.row_DOF[p.elements->Nodes[INDEX2(p.row_node[INDEX2(q, isub, p.row_numShapesTotal)], e, p.NN)]];

                        if (add_EM_F)
                            util::addScatter(p.row_numShapesTotal,
                                    &row_index[0], p.numEqu, &EM_F[0], F_p,
                                    p.row_DOF_UpperBound);
                        if (add_EM_S)
                            Assemble_addToSystemMatrix(p.S,
                                    p.row_numShapesTotal, &row_index[0],
                                    p.numEqu, p.col_numShapesTotal,
                                    &row_index[0], p.numComp, &EM_S[0]);
                    } // end of isub
                } // end color check
            } // end element loop
        } // end color loop
    } // end parallel region
}

} // namespace finley

