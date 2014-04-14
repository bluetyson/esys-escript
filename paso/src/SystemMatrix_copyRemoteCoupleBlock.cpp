
/*****************************************************************************
*
* Copyright (c) 2003-2014 by University of Queensland
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
/* Paso: SystemMatrix                                                       */
/*                                                                          */
/*  Copy mainBlock and col_coupleBlock in other ranks                       */
/*  into remote_coupleBlock                                                 */
/*                                                                          */
/*  WARNING: function uses mpi_requests of the coupler attached to matrix.  */
/*                                                                          */
/****************************************************************************/

/* Copyrights by ACcESS Australia 2003 */
/* Author: Lin Gao, l.gao@uq.edu.au */

/****************************************************************************/

#include "SystemMatrix.h"

#include <cstring> // memcpy

namespace paso {

void SystemMatrix::copyRemoteCoupleBlock(bool recreatePattern)
{
    if (mpi_info->size == 1)
        return;

    if (recreatePattern)
        remote_coupleBlock.reset();

    if (remote_coupleBlock)
        return;

#ifdef ESYS_MPI
    // sending/receiving unknown's global ID
    const dim_t rank = mpi_info->rank;
    const dim_t mpi_size = mpi_info->size;
    index_t num_main_cols = mainBlock->numCols;
    double* cols = new double[num_main_cols];
    const index_t offset = col_distribution->first_component[rank];
#pragma omp parallel for
    for (index_t i=0; i<num_main_cols; ++i)
        cols[i] = offset + i;

    Coupler_ptr coupler;
    if (!global_id) {
        coupler.reset(new Coupler(col_coupler->connector, 1));
        coupler->startCollect(cols);
    }

    index_t* recv_buf = new index_t[mpi_size];
    index_t* recv_degree = new index_t[mpi_size];
    index_t* recv_offset = new index_t[mpi_size+1];
#pragma omp parallel for
    for (index_t i=0; i<mpi_size; i++) {
        recv_buf[i] = 0;
        recv_degree[i] = 1;
        recv_offset[i] = i;
    }

    index_t num_couple_cols = col_coupleBlock->numCols;
    const index_t overlapped_n = row_coupleBlock->numRows;
    SharedComponents_ptr send(row_coupler->connector->send);
    SharedComponents_ptr recv(row_coupler->connector->recv);
    index_t num_neighbors = send->numNeighbors;
    const size_t block_size_size = block_size * sizeof(double);

    // waiting for receiving unknown's global ID
    if (!global_id) {
        coupler->finishCollect();
        global_id = new index_t[num_couple_cols+1];
#pragma omp parallel for
        for (index_t i=0; i<num_couple_cols; ++i) 
            global_id[i] = coupler->recv_buffer[i];
        coupler.reset();
    }

    // distribute the number of cols in current col_coupleBlock to all ranks
    MPI_Allgatherv(&num_couple_cols, 1, MPI_INT, recv_buf, recv_degree,
                   recv_offset, MPI_INT, mpi_info->comm); 

    // distribute global_ids of cols to be considered to all ranks
    index_t len = 0;
    for (index_t i=0; i<mpi_size; i++){
        recv_degree[i] = recv_buf[i];
        recv_offset[i] = len;
        len += recv_buf[i];
    }
    recv_offset[mpi_size] = len;
    index_t* cols_array = new index_t[len];

    MPI_Allgatherv(global_id, num_couple_cols, MPI_INT, cols_array,
                   recv_degree, recv_offset, MPI_INT, mpi_info->comm);

    // first, prepare the ptr_ptr to be received
    index_t* ptr_ptr = new index_t[overlapped_n+1];
    for (index_t p=0; p<recv->numNeighbors; p++) {
        const index_t row = recv->offsetInShared[p];
        const index_t i = recv->offsetInShared[p+1];
        MPI_Irecv(&(ptr_ptr[row]), i-row, MPI_INT, recv->neighbor[p], 
                mpi_info->msg_tag_counter+recv->neighbor[p],
                mpi_info->comm,
                &(row_coupler->mpi_requests[p]));
    }

    // now prepare the rows to be sent (the degree, the offset and the data)
    index_t p = send->offsetInShared[num_neighbors];
    len = 0;
    for (index_t i=0; i<num_neighbors; i++) {
        // #cols per row X #rows
        len += recv_buf[send->neighbor[i]] * 
                (send->offsetInShared[i+1] - send->offsetInShared[i]);
    }
    double* send_buf = new double[len*block_size];
    index_t* send_idx = new index_t[len];
    index_t* send_offset = new index_t[p+1];
    index_t* send_degree = new index_t[num_neighbors];

    index_t k, l, m, n, q;
    len = 0;
    index_t base = 0;
    index_t i0 = 0;
    for (p=0; p<num_neighbors; p++) {
        index_t i = i0;
        const index_t neighbor = send->neighbor[p];
        const index_t l_ub = recv_offset[neighbor+1];
        const index_t l_lb = recv_offset[neighbor];
        const index_t j_ub = send->offsetInShared[p + 1];
        for (index_t j=send->offsetInShared[p]; j<j_ub; j++) {
            const index_t row = send->shared[j];

            // check col_coupleBlock for data to be passed @row
            l = l_lb;
            index_t k_ub = col_coupleBlock->pattern->ptr[row+1];
            k = col_coupleBlock->pattern->ptr[row];
            q = mainBlock->pattern->index[mainBlock->pattern->ptr[row]] + offset;
            while (k<k_ub && l<l_ub) {
                m = global_id[col_coupleBlock->pattern->index[k]];
                if (m > q) break;
                n = cols_array[l];
                if (m == n) {
                    send_idx[len] = l - l_lb;
                    memcpy(&send_buf[len*block_size],
                           &col_coupleBlock->val[block_size*k],
                           block_size_size);
                    len++;
                    l++;
                    k++;
                } else if (m < n) {
                    k++;
                } else {
                    l++;
                }
            }
            const index_t k_lb = k;

            // check mainBlock for data to be passed @row
            k_ub = mainBlock->pattern->ptr[row+1];
            k=mainBlock->pattern->ptr[row]; 
            while (k<k_ub && l<l_ub) {
                m = mainBlock->pattern->index[k] + offset;
                n = cols_array[l];
                if (m == n) {
                    send_idx[len] = l - l_lb;
                    memcpy(&send_buf[len*block_size],
                           &mainBlock->val[block_size*k], block_size_size);
                    len++;
                    l++; 
                    k++;
                } else if (m < n) {
                    k++;
                } else {
                    l++;
                }
            } 

            // check col_coupleBlock for data to be passed @row
            k_ub = col_coupleBlock->pattern->ptr[row+1];
            k=k_lb;
            while (k<k_ub && l<l_ub) {
                m = global_id[col_coupleBlock->pattern->index[k]];
                n = cols_array[l];
                if (m == n) {
                    send_idx[len] = l - l_lb;
                    memcpy(&send_buf[len*block_size],
                           &col_coupleBlock->val[block_size*k],
                           block_size_size);
                    len++;
                    l++;
                    k++;
                } else if (m < n) {
                    k++;
                } else {
                    l++;
                }
            }

            send_offset[i] = len - base;
            base = len;
            i++;
        }

        /* sending */
        MPI_Issend(&send_offset[i0], i-i0, MPI_INT, send->neighbor[p],
                mpi_info->msg_tag_counter+rank, mpi_info->comm,
                &row_coupler->mpi_requests[p+recv->numNeighbors]);
        send_degree[p] = len;
        i0 = i;
    }

    MPI_Waitall(row_coupler->connector->send->numNeighbors +
                    row_coupler->connector->recv->numNeighbors,
                row_coupler->mpi_requests, row_coupler->mpi_stati);
    ESYS_MPI_INC_COUNTER(*mpi_info, mpi_size)

    len = 0;
    for (index_t i=0; i<overlapped_n; i++) {
        p = ptr_ptr[i];
        ptr_ptr[i] = len;
        len += p;
    }
    ptr_ptr[overlapped_n] = len;
    index_t* ptr_idx = new index_t[len];

    // send/receive index array
    index_t j=0;
    for (p=0; p<recv->numNeighbors; p++) {
        const index_t i = ptr_ptr[recv->offsetInShared[p+1]] - ptr_ptr[recv->offsetInShared[p]];
        if (i > 0) 
            MPI_Irecv(&ptr_idx[j], i, MPI_INT, recv->neighbor[p],
                mpi_info->msg_tag_counter+recv->neighbor[p], mpi_info->comm,
                &row_coupler->mpi_requests[p]);
        j += i;
    }

    j=0;
    for (p=0; p<num_neighbors; p++) {
        const index_t i = send_degree[p] - j;
        if (i > 0) 
            MPI_Issend(&send_idx[j], i, MPI_INT, send->neighbor[p],
                mpi_info->msg_tag_counter+rank, mpi_info->comm,
                &row_coupler->mpi_requests[p+recv->numNeighbors]);
        j = send_degree[p];
    }

    MPI_Waitall(row_coupler->connector->send->numNeighbors +
                         row_coupler->connector->recv->numNeighbors,
                row_coupler->mpi_requests, row_coupler->mpi_stati);
    ESYS_MPI_INC_COUNTER(*mpi_info, mpi_size)

    // allocate pattern and sparse matrix for remote_coupleBlock
    Pattern_ptr pattern(new Pattern(row_coupleBlock->pattern->type,
                        overlapped_n, num_couple_cols, ptr_ptr, ptr_idx));
    remote_coupleBlock.reset(new SparseMatrix(row_coupleBlock->type,
                             pattern, row_block_size, col_block_size, false));

    // send/receive value array
    j=0;
    for (p=0; p<recv->numNeighbors; p++) {
        const index_t i = ptr_ptr[recv->offsetInShared[p+1]] - ptr_ptr[recv->offsetInShared[p]];
        if (i > 0) 
            MPI_Irecv(&remote_coupleBlock->val[j], i * block_size, 
                MPI_DOUBLE, recv->neighbor[p],
                mpi_info->msg_tag_counter+recv->neighbor[p], mpi_info->comm,
                &row_coupler->mpi_requests[p]);
        j += i*block_size;
    }

    j=0;
    for (p=0; p<num_neighbors; p++) {
        const index_t i = send_degree[p] - j;
        if (i > 0)
            MPI_Issend(&send_buf[j*block_size], i*block_size, MPI_DOUBLE,
                       send->neighbor[p], mpi_info->msg_tag_counter+rank,
                       mpi_info->comm,
                       &row_coupler->mpi_requests[p+recv->numNeighbors]);
        j = send_degree[p];
    }

    MPI_Waitall(row_coupler->connector->send->numNeighbors +
                     row_coupler->connector->recv->numNeighbors,
                row_coupler->mpi_requests, row_coupler->mpi_stati);
    ESYS_MPI_INC_COUNTER(*mpi_info, mpi_size)

    // release all temp memory allocation
    delete[] cols;
    delete[] cols_array;
    delete[] recv_offset;
    delete[] recv_degree;
    delete[] recv_buf;
    delete[] send_buf;
    delete[] send_offset;
    delete[] send_degree;
    delete[] send_idx;
#endif
}

} // namespace paso

