//
/*******************************************************
*
* Copyright (c) 2003-2011 by University of Queensland
* Earth Systems Science Computational Center (ESSCC)
* http://www.uq.edu.au/esscc
*
* Primary Business: Queensland, Australia
* Licensed under the Open Software License version 3.0
* http://www.opensource.org/licenses/osl-3.0.php
*
*******************************************************/


/**************************************************************/

/* Paso: defines AMG Interpolation                            */

/**************************************************************/

/* Author: l.gao@uq.edu.au                                    */

/**************************************************************/

#include "Paso.h"
#include "SparseMatrix.h"
#include "PasoUtil.h"
#include "Preconditioner.h"

/**************************************************************

    Methods nessecary for AMG preconditioner

    construct n_C x n_C interpolation operator A_C from matrix A
    and prolongation matrix P.

    The coarsening operator A_C is defined by RAP where R=P^T.

***************************************************************/

#define MY_DEBUG 0
#define MY_DEBUG1 0

/* Extend system matrix B with extra two sparse matrixes: 
	B_ext_main and B_ext_couple
   The combination of this two sparse matrixes represents  
   the portion of B that is stored on neighbor procs and 
   needed locally for triple matrix product (B^T) A B.

   FOR NOW, we assume that the system matrix B has a NULL 
   row_coupleBlock and a NULL remote_coupleBlock. Based on
   this assumption, we use link row_coupleBlock for sparse 
   matrix B_ext_main, and link remote_coupleBlock for sparse
   matrix B_ext_couple. 

   To be specific, B_ext (on processor k) are group of rows 
   in B, where the list of rows from processor q is given by 
        A->col_coupler->connector->send->shared[rPtr...] 
        rPtr=A->col_coupler->connector->send->OffsetInShared[k]
   on q. */
void Paso_Preconditioner_AMG_extendB(Paso_SystemMatrix* A, Paso_SystemMatrix* B)
{
  Paso_Pattern *pattern_main=NULL, *pattern_couple=NULL;
  Paso_Coupler *coupler=NULL;
  Paso_SharedComponents *send=NULL, *recv=NULL;
  double *cols=NULL, *send_buf=NULL, *ptr_val=NULL, *send_m=NULL, *send_c=NULL;
  index_t *global_id=NULL, *cols_array=NULL, *ptr_ptr=NULL, *ptr_idx=NULL;
  index_t *ptr_main=NULL, *ptr_couple=NULL, *idx_main=NULL, *idx_couple=NULL;
  index_t *send_idx=NULL, *send_offset=NULL, *recv_buf=NULL, *recv_offset=NULL;
  index_t *idx_m=NULL, *idx_c=NULL;
  index_t i, j, k, m, n, p, q, j_ub, k_lb, k_ub, m_lb, m_ub, l_m, l_c;
  index_t offset, len, block_size, block_size_size, max_num_cols;
  index_t num_main_cols, num_couple_cols, num_neighbors, row, neighbor;
  dim_t *recv_degree=NULL, *send_degree=NULL;
  dim_t rank=A->mpi_info->rank, size=A->mpi_info->size;

  if (size == 1) return;

  if (B->row_coupleBlock) {
    Paso_SparseMatrix_free(B->row_coupleBlock);
    B->row_coupleBlock = NULL;
  }

  if (B->row_coupleBlock || B->remote_coupleBlock) {
    Esys_setError(VALUE_ERROR, "Paso_Preconditioner_AMG_extendB: the link to row_coupleBlock or to remote_coupleBlock has already been set.");
    return;
  }

if (MY_DEBUG1) fprintf(stderr, "CP1_1\n");

  /* sending/receiving unknown's global ID */
  num_main_cols = B->mainBlock->numCols;
  cols = TMPMEMALLOC(num_main_cols, double);
  offset = B->col_distribution->first_component[rank];
  #pragma omp parallel for private(i) schedule(static)
  for (i=0; i<num_main_cols; ++i) cols[i] = offset + i;
  if (B->global_id == NULL) {
    coupler=Paso_Coupler_alloc(B->col_coupler->connector, 1);
    Paso_Coupler_startCollect(coupler, cols);
  }

  recv_buf = TMPMEMALLOC(size, index_t);
  recv_degree = TMPMEMALLOC(size, dim_t);
  recv_offset = TMPMEMALLOC(size+1, index_t);
  #pragma omp parallel for private(i) schedule(static)
  for (i=0; i<size; i++){
    recv_buf[i] = 0;
    recv_degree[i] = 1;
    recv_offset[i] = i;
  }

  block_size = B->col_coupler->block_size;
  block_size_size = block_size * sizeof(double);
  num_couple_cols = B->col_coupleBlock->numCols;
  send = A->col_coupler->connector->send;
  recv = A->col_coupler->connector->recv;
  num_neighbors = send->numNeighbors;
  p = send->offsetInShared[num_neighbors];
  len = p * B->col_distribution->first_component[size];
  send_buf = TMPMEMALLOC(len * block_size, double);
  send_idx = TMPMEMALLOC(len, index_t);
  send_offset = TMPMEMALLOC((p+1)*2, index_t);
  send_degree = TMPMEMALLOC(num_neighbors, dim_t);
  i = num_main_cols + num_couple_cols;
  send_m = TMPMEMALLOC(i * block_size, double);
  send_c = TMPMEMALLOC(i * block_size, double);
  idx_m = TMPMEMALLOC(i, index_t);
  idx_c = TMPMEMALLOC(i, index_t);

  /* waiting for receiving unknown's global ID */
  if (B->global_id == NULL) {
    Paso_Coupler_finishCollect(coupler);
    global_id = MEMALLOC(num_couple_cols, index_t);
    #pragma omp parallel for private(i) schedule(static)
    for (i=0; i<num_couple_cols; ++i)
        global_id[i] = coupler->recv_buffer[i];
    B->global_id = global_id;
    Paso_Coupler_free(coupler);
  } else 
    global_id = B->global_id;

  /* distribute the number of cols in current col_coupleBlock to all ranks */
  MPI_Allgatherv(&num_couple_cols, 1, MPI_INT, recv_buf, recv_degree, recv_offset, MPI_INT, A->mpi_info->comm);

if (MY_DEBUG1) fprintf(stderr, "CP1_2\n");

  /* distribute global_ids of cols to be considered to all ranks*/
  len = 0;
  max_num_cols = 0;
  for (i=0; i<size; i++){
    recv_degree[i] = recv_buf[i];
    recv_offset[i] = len;
    len += recv_buf[i];
    if (max_num_cols < recv_buf[i]) 
	max_num_cols = recv_buf[i];
  }
  recv_offset[size] = len;
  cols_array = TMPMEMALLOC(len, index_t);
  MPI_Allgatherv(global_id, num_couple_cols, MPI_INT, cols_array, recv_degree, recv_offset, MPI_INT, A->mpi_info->comm);

if (MY_DEBUG1) fprintf(stderr, "rank %d CP1_3\n", rank);
if (MY_DEBUG) {
    int i=0;
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    fprintf(stderr, "rank %d PID %d on %s ready for attach\n", A->mpi_info->rank, getpid(), hostname);
    if (rank == 1) {
      fflush(stdout);
      while (0 == i)
        sleep(5);
    }
}

  /* first, prepare the ptr_ptr to be received */
  q = recv->numNeighbors;
  len = recv->offsetInShared[q];
  ptr_ptr = TMPMEMALLOC((len+1) * 2, index_t);
  for (p=0; p<q; p++) {
    row = recv->offsetInShared[p];
    m = recv->offsetInShared[p + 1];
    MPI_Irecv(&(ptr_ptr[2*row]), 2 * (m-row), MPI_INT, recv->neighbor[p],
                A->mpi_info->msg_tag_counter+recv->neighbor[p],
                A->mpi_info->comm,
                &(A->col_coupler->mpi_requests[p]));
if(MY_DEBUG1) fprintf(stderr, "rank %d receive %d from %d tag %d (numNeighbors %d)\n", rank, 2*(m-row), recv->neighbor[p], A->mpi_info->msg_tag_counter+recv->neighbor[p], q);
  }

  /* now prepare the rows to be sent (the degree, the offset and the data) */
  len = 0;
  for (p=0; p<num_neighbors; p++) {
    i = 0;
    neighbor = send->neighbor[p];
    m_lb = B->col_distribution->first_component[neighbor];
    m_ub = B->col_distribution->first_component[neighbor + 1];
    j_ub = send->offsetInShared[p + 1];
if (MY_DEBUG1) fprintf(stderr, "rank%d neighbor %d m_lb %d m_ub %d offset %d\n", rank, neighbor, m_lb, m_ub, offset);
    for (j=send->offsetInShared[p]; j<j_ub; j++) {
	row = send->shared[j];
	l_m = 0;
	l_c = 0;
	k_ub = B->col_coupleBlock->pattern->ptr[row + 1];
	k_lb = B->col_coupleBlock->pattern->ptr[row];

	/* check part of col_coupleBlock for data to be passed @row */ 
//	q = B->mainBlock->pattern->index[B->mainBlock->pattern->ptr[row]] + offset;
	for (k=k_lb; k<k_ub; k++) {
	  m = global_id[B->col_coupleBlock->pattern->index[k]];
//	  if (m > q) break;
	  if (m > offset) break;
if (MY_DEBUG) fprintf(stderr, "rank%d (1) row %d m %d k %d\n", rank, row, m, B->col_coupleBlock->pattern->index[k]);
	  if (m>= m_lb && m < m_ub) {
	    /* data to be passed to sparse matrix B_ext_main */
	    idx_m[l_m] = m - m_lb;
	    memcpy(&(send_m[l_m*block_size]), &(B->col_coupleBlock->val[block_size*k]), block_size_size);
	    l_m++;
	  } else { 
	    /* data to be passed to sparse matrix B_ext_couple */
	    idx_c[l_c] = m;
if (MY_DEBUG) fprintf(stderr, "rank%d (1) m %d m_lb %d m_ub %d\n", rank, m, m_lb, m_ub);
	    memcpy(&(send_c[l_c*block_size]), &(B->col_coupleBlock->val[block_size*k]), block_size_size);
	    l_c++;
	  } 
	}
	k_lb = k;

	/* check mainBlock for data to be passed @row to sparse 
	   matrix B_ext_couple */
	k_ub = B->mainBlock->pattern->ptr[row + 1];
	k = B->mainBlock->pattern->ptr[row];
	memcpy(&(send_c[l_c*block_size]), &(B->mainBlock->val[block_size*k]), block_size_size * (k_ub-k));
	for (; k<k_ub; k++) {
	  m = B->mainBlock->pattern->index[k] + offset;
if (MY_DEBUG) fprintf(stderr, "rank%d (2) row %d m %d k %d\n", rank, row, m, B->mainBlock->pattern->index[k]);
if (MY_DEBUG) fprintf(stderr, "rank%d (2) m %d m_lb %d m_ub %d\n", rank, m, m_lb, m_ub);
	  idx_c[l_c] = m;
	  l_c++;
	}

	/* check the rest part of col_coupleBlock for data to 
	   be passed @row to sparse matrix B_ext_couple */
	k = k_lb;
	k_ub = B->col_coupleBlock->pattern->ptr[row + 1];
	for (k=k_lb; k<k_ub; k++) {
          m = global_id[B->col_coupleBlock->pattern->index[k]];
if (MY_DEBUG) fprintf(stderr, "rank%d (3) row %d m %d k %d\n", rank, row, m, B->col_coupleBlock->pattern->index[k]);
	  if (m>= m_lb && m < m_ub) {
	    /* data to be passed to sparse matrix B_ext_main */
	    idx_m[l_m] = m - m_lb;
	    memcpy(&(send_m[l_m*block_size]), &(B->col_coupleBlock->val[block_size*k]), block_size_size);
	    l_m++;
	  } else {
	    /* data to be passed to sparse matrix B_ext_couple */
	    idx_c[l_c] = m;
if (MY_DEBUG) fprintf(stderr, "rank%d (3) m %d m_lb %d m_ub %d\n", rank, m, m_lb, m_ub);
	    memcpy(&(send_c[l_c*block_size]), &(B->col_coupleBlock->val[block_size*k]), block_size_size);
	    l_c++;
	  }
	}

if (MY_DEBUG && rank == 2) fprintf(stderr, "rank %d p %d i %d j %d l_c %d l_m %d ub %d\n", rank, p, i, j, l_c, l_m, num_couple_cols + num_main_cols);
	memcpy(&(send_buf[len]), send_m, block_size_size*l_m);
	memcpy(&(send_idx[len]), idx_m, l_m * sizeof(index_t));
        send_offset[2*i] = l_m;
	len += l_m;
	memcpy(&(send_buf[len]), send_c, block_size_size*l_c);
	memcpy(&(send_idx[len]), idx_c, l_c * sizeof(index_t));
	send_offset[2*i+1] = l_c;
	len += l_c;
if (MY_DEBUG && rank == 0) {
int sum = l_m+l_c, my_i;
char *str1, *str2;
str1 = TMPMEMALLOC(sum*100+100, char);
str2 = TMPMEMALLOC(100, char);
sprintf(str1, "rank %d send_idx[%d] offset %d m%d c%d= (", rank, sum, len-sum, l_m, l_c);
for (my_i=len-sum; my_i<len; my_i++){
  sprintf(str2, "%d ", send_idx[my_i]);
  strcat(str1, str2);
}
fprintf(stderr, "%s)\n", str1);
TMPMEMFREE(str1);
TMPMEMFREE(str2);
}
	i++;
    }

if (MY_DEBUG && rank == 0) {
int my_i,sum = len;
char *str1, *str2;
str1 = TMPMEMALLOC(sum*100+100, char);
str2 = TMPMEMALLOC(100, char);
sprintf(str1, "rank %d send_idx[%d] = (", rank, sum);
for (my_i=0; my_i<sum; my_i++){
  sprintf(str2, "%d ", send_idx[my_i]);
  strcat(str1, str2);
}
fprintf(stderr, "%s)\n", str1);
TMPMEMFREE(str1);
TMPMEMFREE(str2);
}

    /* sending */
    MPI_Issend(send_offset, 2*i, MPI_INT, send->neighbor[p],
                A->mpi_info->msg_tag_counter+rank,
                A->mpi_info->comm,
                &(A->col_coupler->mpi_requests[p+recv->numNeighbors]));
if(MY_DEBUG) fprintf(stderr, "rank %d send %d to %d tag %d (numNeighbors %d)\n", rank, 2*i, send->neighbor[p], A->mpi_info->msg_tag_counter+rank, send->numNeighbors);
    send_degree[p] = len;
  }
  TMPMEMFREE(send_m);
  TMPMEMFREE(send_c);
  TMPMEMFREE(idx_m);
  TMPMEMFREE(idx_c);


  q = recv->numNeighbors;
  len = recv->offsetInShared[q];
  ptr_main = MEMALLOC((len+1), index_t);
  ptr_couple = MEMALLOC((len+1), index_t);


  MPI_Waitall(A->col_coupler->connector->send->numNeighbors+A->col_coupler->connector->recv->numNeighbors,
                A->col_coupler->mpi_requests,
                A->col_coupler->mpi_stati);
  A->mpi_info->msg_tag_counter += size;

if (MY_DEBUG) {
int sum = recv->offsetInShared[recv->numNeighbors] * 2;
char *str1, *str2;
str1 = TMPMEMALLOC(sum*100+100, char);
str2 = TMPMEMALLOC(100, char);
sprintf(str1, "rank %d ptr_ptr[%d] = (", rank, sum);
for (i=0; i<sum; i++){
  sprintf(str2, "%d ", ptr_ptr[i]);
  strcat(str1, str2);
}
fprintf(stderr, "%s)\n", str1);
TMPMEMFREE(str1);
TMPMEMFREE(str2);
}

  j = 0;
  k = 0;
  ptr_main[0] = 0;
  ptr_couple[0] = 0;
//  #pragma omp parallel for private(i,j,k) schedule(static)
  for (i=0; i<len; i++) {
    j += ptr_ptr[2*i];
    k += ptr_ptr[2*i+1];
    ptr_main[i+1] = j;
    ptr_couple[i+1] = k;
  }

  TMPMEMFREE(ptr_ptr);  
  idx_main = MEMALLOC(j, index_t);
  idx_couple = MEMALLOC(k, index_t);
  ptr_idx = TMPMEMALLOC(j+k, index_t);
  ptr_val = TMPMEMALLOC(j+k, double);
if (MY_DEBUG1) fprintf(stderr, "rank %d CP1_4_2 %d %d %d\n", rank, j, k, len);

  /* send/receive index array */
  j=0;
  k_ub = 0;
  for (p=0; p<recv->numNeighbors; p++) {
    k = recv->offsetInShared[p];
    m = recv->offsetInShared[p+1];
    i = ptr_main[m] - ptr_main[k] + ptr_couple[m] - ptr_couple[k];
    if (i > 0) {
	k_ub ++;
        MPI_Irecv(&(ptr_idx[j]), i, MPI_INT, recv->neighbor[p],
                A->mpi_info->msg_tag_counter+recv->neighbor[p],
                A->mpi_info->comm,
                &(A->col_coupler->mpi_requests[p]));
if(MY_DEBUG) fprintf(stderr, "rank %d (INDEX) recv %d from %d tag %d\n", rank, i, recv->neighbor[p], A->mpi_info->msg_tag_counter+recv->neighbor[p]);
    } else {
if (MY_DEBUG1) fprintf(stderr, "rank%d k %d m %d main(%d %d) couple(%d %d)\n", rank, k, m, ptr_main[m], ptr_main[k], ptr_couple[m], ptr_couple[k]);
    }
    j += i;
  }

if (MY_DEBUG) fprintf(stderr, "rank %d CP1_4_3  %d %d\n", rank, recv->numNeighbors, k_ub);
  j=0;
  k_ub = 0;
  for (p=0; p<num_neighbors; p++) {
    i = send_degree[p] - j;
    if (i > 0){
	k_ub ++;
if (MY_DEBUG && rank == 0 && send->neighbor[p] == 1) {
int sum = i, my_i;
char *str1, *str2;
str1 = TMPMEMALLOC(sum*100+100, char);
str2 = TMPMEMALLOC(100, char);
sprintf(str1, "rank %d send_idx[%d] offset %d = (", rank, sum, j);
for (my_i=0; my_i<sum; my_i++){
  sprintf(str2, "%d ", send_idx[j+my_i]);
  strcat(str1, str2);
}
fprintf(stderr, "%s)\n", str1);
TMPMEMFREE(str1);
TMPMEMFREE(str2);
}
        MPI_Issend(&(send_idx[j]), i, MPI_INT, send->neighbor[p],
                A->mpi_info->msg_tag_counter+rank,
                A->mpi_info->comm,
                &(A->col_coupler->mpi_requests[p+recv->numNeighbors]));
if(MY_DEBUG1) fprintf(stderr, "rank %d (INDEX) send %d to %d tag %d\n", rank, i, send->neighbor[p], A->mpi_info->msg_tag_counter+rank);
    } else {
if (MY_DEBUG1) fprintf(stderr, "rank%d send_degree %d, p %d, j %d\n", rank, send_degree[p], p, j);
    }
    j = send_degree[p];
  }
if (MY_DEBUG1) fprintf(stderr, "rank %d CP1_4_4 %d %d\n", rank,num_neighbors, k_ub);

  MPI_Waitall(A->col_coupler->connector->send->numNeighbors+A->col_coupler->connector->recv->numNeighbors,
                A->col_coupler->mpi_requests,
                A->col_coupler->mpi_stati);
  A->mpi_info->msg_tag_counter += size;
if (MY_DEBUG1) fprintf(stderr, "rank %d CP1_5 %d %d %d\n", rank, len, ptr_main[len], ptr_couple[len]);
if (MY_DEBUG && rank == 1) {
int my_i, sum1 = recv->offsetInShared[recv->numNeighbors];
int sum = ptr_main[sum1] + ptr_couple[sum1];
char *str1, *str2;
str1 = TMPMEMALLOC(sum*100+100, char);
str2 = TMPMEMALLOC(100, char);
sprintf(str1, "rank %d ptr_idx[%d] = (", rank, sum);
for (my_i=0; my_i<sum; my_i++){
  sprintf(str2, "%d ", ptr_idx[my_i]);
  strcat(str1, str2);
}
fprintf(stderr, "%s)\n", str1);
TMPMEMFREE(str1);
TMPMEMFREE(str2);
}


//  #pragma omp parallel for private(i,j,k,m,p) schedule(static)
  for (i=0; i<len; i++) {
    j = ptr_main[i];
    k = ptr_main[i+1];
    m = ptr_couple[i];
    for (p=j; p<k; p++) {
	idx_main[p] = ptr_idx[m+p];
    }
    j = ptr_couple[i+1];
    for (p=m; p<j; p++) {
	idx_couple[p] = ptr_idx[k+p];
    }
  }
  TMPMEMFREE(ptr_idx);
if (MY_DEBUG1) fprintf(stderr, "rank %d CP1_5_1 %d %d\n", rank, num_main_cols, len);
if (MY_DEBUG) {
int sum = num_main_cols, sum1 = ptr_main[sum];
char *str1, *str2;
str1 = TMPMEMALLOC(sum1*30+100, char);
str2 = TMPMEMALLOC(100, char);
sprintf(str1, "rank %d ptr_main[%d] = (", rank, sum);
for (i=0; i<sum+1; i++){
  sprintf(str2, "%d ", ptr_main[i]);
  strcat(str1, str2);
}
fprintf(stderr, "%s)\n", str1);
sprintf(str1, "rank %d idx_main[%d] = (", rank, sum1);
for (i=0; i<sum1; i++){
  sprintf(str2, "%d ", idx_main[i]);
  strcat(str1, str2);
}
fprintf(stderr, "%s)\n", str1);
TMPMEMFREE(str1);
TMPMEMFREE(str2);
}

  /* allocate pattern and sparsematrix for B_ext_main */
  pattern_main = Paso_Pattern_alloc(B->col_coupleBlock->pattern->type,
                len, num_main_cols, ptr_main, idx_main);
  B->row_coupleBlock = Paso_SparseMatrix_alloc(B->col_coupleBlock->type,
		pattern_main, B->row_block_size, B->col_block_size,
		FALSE);
  Paso_Pattern_free(pattern_main);
if (MY_DEBUG) fprintf(stderr, "rank %d CP1_5_2\n", rank);

  /* allocate pattern and sparsematrix for B_ext_couple */
  pattern_couple = Paso_Pattern_alloc(B->col_coupleBlock->pattern->type,
                len, B->col_distribution->first_component[size], 
		ptr_couple, idx_couple);
  B->remote_coupleBlock = Paso_SparseMatrix_alloc(B->col_coupleBlock->type,
                pattern_couple, B->row_block_size, B->col_block_size,
                FALSE);
  Paso_Pattern_free(pattern_couple);

if (MY_DEBUG1) fprintf(stderr, "CP1_6\n");

  /* send/receive value array */
  j=0;
  for (p=0; p<recv->numNeighbors; p++) {
    k = recv->offsetInShared[p];
    m = recv->offsetInShared[p+1];
    i = ptr_main[m] - ptr_main[k] + ptr_couple[m] - ptr_couple[k];
    if (i > 0)
        MPI_Irecv(&(ptr_val[j]), i * block_size,
                MPI_DOUBLE, recv->neighbor[p],
                A->mpi_info->msg_tag_counter+recv->neighbor[p],
                A->mpi_info->comm,
                &(A->col_coupler->mpi_requests[p]));
    j += i;
  }

  j=0;
  for (p=0; p<num_neighbors; p++) {
    i = send_degree[p] - j;
    if (i > 0)
        MPI_Issend(&(send_buf[j]), i*block_size, MPI_DOUBLE, send->neighbor[p],
                A->mpi_info->msg_tag_counter+rank,
                A->mpi_info->comm,
                &(A->col_coupler->mpi_requests[p+recv->numNeighbors]));
    j = send_degree[p];
  }

  MPI_Waitall(A->col_coupler->connector->send->numNeighbors+A->col_coupler->connector->recv->numNeighbors,
                A->col_coupler->mpi_requests,
                A->col_coupler->mpi_stati);
  A->mpi_info->msg_tag_counter += size;
if (MY_DEBUG1) fprintf(stderr, "CP1_7\n");

  #pragma omp parallel for private(i,j,k,m,p) schedule(static)
  for (i=0; i<len; i++) {
    j = ptr_main[i];
    k = ptr_main[i+1];
    m = ptr_couple[i];
    for (p=j; p<k; p++) {
	B->row_coupleBlock->val[p] = ptr_val[m+p];
    }
    j = ptr_couple[i+1];
    for (p=m; p<j; p++) {
	B->remote_coupleBlock->val[p] = ptr_val[k+p];
    }
  }
  TMPMEMFREE(ptr_val);

  /* release all temp memory allocation */
  TMPMEMFREE(cols);
  TMPMEMFREE(cols_array);
  TMPMEMFREE(recv_offset);
  TMPMEMFREE(recv_degree);
  TMPMEMFREE(recv_buf);
  TMPMEMFREE(send_buf);
  TMPMEMFREE(send_offset);
  TMPMEMFREE(send_degree);
  TMPMEMFREE(send_idx);
}

/* As defined, sparse matrix (let's called it T) defined by T(ptr, idx, val) 
   has the same number of rows as P->col_coupleBlock->numCols. Now, we need 
   to copy block of data in T to neighbor processors, defined by 
	P->col_coupler->connector->recv->neighbor[k] where k is in 
	[0, P->col_coupler->connector->recv->numNeighbors).
   Rows to be copied to neighbor processor k is in the list defined by
	P->col_coupler->connector->recv->offsetInShared[k] ...
	P->col_coupler->connector->recv->offsetInShared[k+1]  */
void Paso_Preconditioner_AMG_CopyRemoteData(Paso_SystemMatrix* P, 
	index_t **p_ptr, index_t **p_idx, double **p_val, index_t *global_id) 
{
  Paso_SharedComponents *send=NULL, *recv=NULL;
  index_t send_neighbors, recv_neighbors, send_rows, recv_rows;
  index_t i, j, p, m, n, rank, size, offset;
  index_t *send_degree=NULL, *recv_ptr=NULL, *recv_idx=NULL;
  index_t *ptr=*p_ptr, *idx=*p_idx;
  double  *val=*p_val, *recv_val=NULL;

  rank = P->mpi_info->rank;
  size = P->mpi_info->size;
  send = P->col_coupler->connector->recv;
  recv = P->col_coupler->connector->send;
  send_neighbors = send->numNeighbors;
  recv_neighbors = recv->numNeighbors;
  send_rows = P->col_coupleBlock->numCols;
  recv_rows = recv->offsetInShared[recv_neighbors];
  offset = P->col_distribution->first_component[rank];

  send_degree = TMPMEMALLOC(send_rows, index_t);
  recv_ptr = MEMALLOC(recv_rows + 1, index_t);
  #pragma omp for schedule(static) private(i)
  for (i=0; i<send_rows; i++) 
    send_degree[i] = ptr[i+1] - ptr[i];
if (MY_DEBUG) fprintf(stderr, "rank %d CP5_1 srows %d rrows %d\n", rank, send_rows, recv_rows);

  /* First, send/receive the degree */
  for (p=0; p<recv_neighbors; p++) { /* Receiving */
    m = recv->offsetInShared[p];
    n = recv->offsetInShared[p+1];
    MPI_Irecv(&(recv_ptr[m]), n-m, MPI_INT, recv->neighbor[p],
		P->mpi_info->msg_tag_counter + recv->neighbor[p],
		P->mpi_info->comm,
		&(P->col_coupler->mpi_requests[p]));
  }
  for (p=0; p<send_neighbors; p++) { /* Sending */
    m = send->offsetInShared[p];
    n = send->offsetInShared[p+1];
    MPI_Issend(&(send_degree[m]), n-m, MPI_INT, send->neighbor[p],
		P->mpi_info->msg_tag_counter + rank,
		P->mpi_info->comm,
		&(P->col_coupler->mpi_requests[p+recv_neighbors]));
  }
  MPI_Waitall(send_neighbors+recv_neighbors, 
		P->col_coupler->mpi_requests,
		P->col_coupler->mpi_stati);
  P->mpi_info->msg_tag_counter += size;
if (MY_DEBUG) fprintf(stderr, "rank %d CP5_3 %d %d\n", rank, recv_neighbors, send_neighbors);

  TMPMEMFREE(send_degree);
  m = Paso_Util_cumsum(recv_rows, recv_ptr);
  recv_ptr[recv_rows] = m;
  recv_idx = MEMALLOC(m, index_t); 
  recv_val = MEMALLOC(m, double);
if (MY_DEBUG) fprintf(stderr, "rank %d receive size %d\n", rank, m);

  /* Next, send/receive the index array */
  j = 0;
  for (p=0; p<recv_neighbors; p++) { /* Receiving */
    m = recv->offsetInShared[p];
    n = recv->offsetInShared[p+1];
    i = recv_ptr[n] - recv_ptr[m]; 
    if (i > 0) {
if (MY_DEBUG) fprintf(stderr, "rank %d recv message size %d from %d (%d, %d, %d, %d, %d) \n", rank, i, recv->neighbor[p], p, m, n, recv_ptr[n], recv_ptr[m]);
      MPI_Irecv(&(recv_idx[j]), i, MPI_INT, recv->neighbor[p],
		P->mpi_info->msg_tag_counter + recv->neighbor[p],
		P->mpi_info->comm,
		&(P->col_coupler->mpi_requests[p]));
    } else 
if (MY_DEBUG) fprintf(stderr, "rank %d WARNING empty recv message %d %d %d %d %d %d\n", rank, recv_neighbors, p, m, n, recv_ptr[n], recv_ptr[m]);
    j += i;
  }

  j = 0;
  for (p=0; p<send_neighbors; p++) { /* Sending */
    m = send->offsetInShared[p];
    n = send->offsetInShared[p+1];
    i = ptr[n] - ptr[m];
    if (i >0) {
if (MY_DEBUG) fprintf(stderr, "rank %d send message size %d to %d (%d, %d, %d, %d, %d) \n", rank, i, send->neighbor[p], p, m, n, ptr[n], ptr[m]);
	MPI_Issend(&(idx[j]), i, MPI_INT, send->neighbor[p],
		P->mpi_info->msg_tag_counter + rank,
		P->mpi_info->comm,
		&(P->col_coupler->mpi_requests[p+recv_neighbors]));
	j += i;
    } else 
if (MY_DEBUG) fprintf(stderr, "rank %d WARNING empty send message %d %d %d %d %d %d\n", rank, send_neighbors, p, m, n, ptr[n], ptr[m]);
  }
  MPI_Waitall(send_neighbors+recv_neighbors, 
		P->col_coupler->mpi_requests,
		P->col_coupler->mpi_stati);
  P->mpi_info->msg_tag_counter += size;
if (MY_DEBUG) fprintf(stderr, "rank %d CP5_5\n", rank);

  /* Last, send/receive the data array */
  j = 0;
  for (p=0; p<recv_neighbors; p++) { /* Receiving */
    m = recv->offsetInShared[p];
    n = recv->offsetInShared[p+1];
    i = recv_ptr[n] - recv_ptr[m]; 
    if (i > 0) 
      MPI_Irecv(&(recv_val[j]), i, MPI_DOUBLE, recv->neighbor[p],
		P->mpi_info->msg_tag_counter + recv->neighbor[p],
		P->mpi_info->comm,
		&(P->col_coupler->mpi_requests[p]));
    j += i;
  }

  j = 0;
  for (p=0; p<send_neighbors; p++) { /* Sending */
    m = send->offsetInShared[p];
    n = send->offsetInShared[p+1];
    i = ptr[n] - ptr[m];
    if (i >0) {
	MPI_Issend(&(val[j]), i, MPI_DOUBLE, send->neighbor[p],
		P->mpi_info->msg_tag_counter + rank,
		P->mpi_info->comm,
		&(P->col_coupler->mpi_requests[p+recv_neighbors]));
	j += i;
    }
  }
  MPI_Waitall(send_neighbors+recv_neighbors, 
		P->col_coupler->mpi_requests,
		P->col_coupler->mpi_stati);
  P->mpi_info->msg_tag_counter += size;
if (MY_DEBUG) fprintf(stderr, "rank %d CP5_7\n", rank);

  /* Clean up and return with recevied ptr, index and data arrays */
  MEMFREE(ptr);
  MEMFREE(idx);
  MEMFREE(val);
if (MY_DEBUG) fprintf(stderr, "rank %d len %d\n", rank, recv_ptr[recv_rows]);
  *p_ptr = recv_ptr;
  *p_idx = recv_idx;
  *p_val = recv_val;
}

Paso_SystemMatrix* Paso_Preconditioner_AMG_buildInterpolationOperator(
	Paso_SystemMatrix* A, Paso_SystemMatrix* P, Paso_SystemMatrix* R)
{
   Esys_MPIInfo *mpi_info=Esys_MPIInfo_getReference(A->mpi_info);
   Paso_SparseMatrix *R_main=NULL, *R_couple=NULL;
   Paso_SystemMatrix *out=NULL;
   Paso_SystemMatrixPattern *pattern=NULL;
   Paso_Distribution *input_dist=NULL, *output_dist=NULL;
   Paso_SharedComponents *send =NULL, *recv=NULL;
   Paso_Connector *col_connector=NULL, *row_connector=NULL;
   Paso_Coupler *coupler=NULL;
   Paso_Pattern *main_pattern=NULL;
   Paso_Pattern *col_couple_pattern=NULL, *row_couple_pattern =NULL;
   const dim_t row_block_size=A->row_block_size;
   const dim_t col_block_size=A->col_block_size;
   const dim_t num_threads=omp_get_max_threads();
   const double ZERO = 0.0;
   double *RAP_main_val=NULL, *RAP_couple_val=NULL, *RAP_ext_val=NULL;
   double RAP_val, RA_val, R_val, *temp_val=NULL;
   index_t size=mpi_info->size, rank=mpi_info->rank, *dist=NULL;
   index_t *RAP_main_ptr=NULL, *RAP_couple_ptr=NULL, *RAP_ext_ptr=NULL;
   index_t *RAP_main_idx=NULL, *RAP_couple_idx=NULL, *RAP_ext_idx=NULL;
   index_t *offsetInShared=NULL, *row_couple_ptr=NULL, *row_couple_idx=NULL;
   index_t *Pcouple_to_Pext=NULL, *Pext_to_RAP=NULL, *Pcouple_to_RAP=NULL;
   index_t *temp=NULL, *global_id_P=NULL, *global_id_RAP=NULL;
   index_t *shared=NULL, *P_marker=NULL, *A_marker=NULL;
   index_t sum, i, j, k, iptr;
   index_t num_Pmain_cols, num_Pcouple_cols, num_Pext_cols;
   index_t num_A_cols, row_marker, num_RAPext_cols, num_Acouple_cols, offset;
   index_t i_r, i_c, i1, i2, j1, j1_ub, j2, j2_ub, j3, j3_ub, num_RAPext_rows;
   index_t row_marker_ext, *where_p=NULL;
   index_t **send_ptr=NULL, **send_idx=NULL;
   dim_t l, p, q, global_label, num_neighbors;
   dim_t *recv_len=NULL, *send_len=NULL, *len=NULL;
   Esys_MPI_rank *neighbor=NULL;
   #ifdef ESYS_MPI
     MPI_Request* mpi_requests=NULL;
     MPI_Status* mpi_stati=NULL;
   #else
     int *mpi_requests=NULL, *mpi_stati=NULL;
   #endif

   if (size == 1) return NULL;
   /* two sparse matrixes R_main and R_couple will be generate, as the 
      transpose of P_main and P_col_couple, respectively. Note that, 
      R_couple is actually the row_coupleBlock of R (R=P^T) */
   R_main = Paso_SparseMatrix_getTranspose(P->mainBlock);
   R_couple = Paso_SparseMatrix_getTranspose(P->col_coupleBlock);

if (MY_DEBUG) fprintf(stderr, "CP1\n");

   /* generate P_ext, i.e. portion of P that is tored on neighbor procs
      and needed locally for triple matrix product RAP 
      to be specific, P_ext (on processor k) are group of rows in P, where 
      the list of rows from processor q is given by 
	A->col_coupler->connector->send->shared[rPtr...] 
	rPtr=A->col_coupler->connector->send->OffsetInShared[k]
      on q. 
      P_ext is represented by two sparse matrixes P_ext_main and 
      P_ext_couple */
   Paso_Preconditioner_AMG_extendB(A, P);
if (MY_DEBUG1) fprintf(stderr, "rank %d CP2\n", rank);

   /* count the number of cols in P_ext_couple, resize the pattern of 
      sparse matrix P_ext_couple with new compressed order, and then
      build the col id mapping from P->col_coupleBlock to
      P_ext_couple */   
   num_Pmain_cols = P->mainBlock->numCols;
   num_Pcouple_cols = P->col_coupleBlock->numCols;
   num_Acouple_cols = A->col_coupleBlock->numCols;
   num_A_cols = A->mainBlock->numCols + num_Acouple_cols;
   sum = P->remote_coupleBlock->pattern->ptr[P->remote_coupleBlock->numRows];
   offset = P->col_distribution->first_component[rank];
   num_Pext_cols = 0;
   if (P->global_id) {
     /* first count the number of cols "num_Pext_cols" in both P_ext_couple 
	and P->col_coupleBlock */
     iptr = 0;
     if (num_Pcouple_cols || sum > 0) {
	temp = TMPMEMALLOC(num_Pcouple_cols+sum, index_t);
	for (; iptr<sum; iptr++){ 
	  temp[iptr] = P->remote_coupleBlock->pattern->index[iptr];
if (MY_DEBUG && rank ==1)
fprintf(stderr, "rank %d remote_block[%d] = %d\n", rank, iptr, temp[iptr]);
	}
	for (j=0; j<num_Pcouple_cols; j++, iptr++){
	  temp[iptr] = P->global_id[j];
if (MY_DEBUG && rank ==1)
fprintf(stderr, "rank %d global_id[%d] = %d\n", rank, j, P->global_id[j]);
	}
     }
     if (iptr) {
	#ifdef USE_QSORTG
	  qsortG(temp, (size_t)iptr, sizeof(index_t), Paso_comparIndex);
	#else
	  qsort(temp, (size_t)iptr, sizeof(index_t), Paso_comparIndex);
	#endif
	num_Pext_cols = 1;
	i = temp[0];
	for (j=1; j<iptr; j++) {
	  if (temp[j] > i) {
	    i = temp[j];
	    temp[num_Pext_cols++] = i;
	  }
	}
     }

     /* resize the pattern of P_ext_couple */
     if(num_Pext_cols){
	global_id_P = TMPMEMALLOC(num_Pext_cols, index_t);
	for (i=0; i<num_Pext_cols; i++)
	  global_id_P[i] = temp[i];
if (MY_DEBUG && rank == 1) {
int my_i, sum=num_Pext_cols;
char *str1, *str2;
str1 = TMPMEMALLOC(sum*100+100, char);
str2 = TMPMEMALLOC(100, char);
sprintf(str1, "rank %d global_id_P[%d] = (", rank, sum);
for (my_i=0; my_i<sum; my_i++) {
  sprintf(str2, "%d ", global_id_P[my_i]);
  strcat(str1, str2);
}
fprintf(stderr, "%s)\n", str1);
TMPMEMFREE(str1);
TMPMEMFREE(str2);
}

     }
     if (num_Pcouple_cols || sum > 0) 
	TMPMEMFREE(temp);
     for (i=0; i<sum; i++) {
	where_p = (index_t *)bsearch(
			&(P->remote_coupleBlock->pattern->index[i]),
			global_id_P, num_Pext_cols, 
                        sizeof(index_t), Paso_comparIndex);
	P->remote_coupleBlock->pattern->index[i] = 
			(index_t)(where_p -global_id_P);
     }  

     /* build the mapping */
     if (num_Pcouple_cols) {
	Pcouple_to_Pext = TMPMEMALLOC(num_Pcouple_cols, index_t);
	iptr = 0;
	for (i=0; i<num_Pext_cols; i++)
	  if (global_id_P[i] == P->global_id[iptr]) {
	    Pcouple_to_Pext[iptr++] = i;
	    if (iptr == num_Pcouple_cols) break;
	  }
     }
   }

if (MY_DEBUG) fprintf(stderr, "CP3\n");
if (MY_DEBUG && rank == 0) {
fprintf(stderr, "Rank %d Now Prolongation Matrix ************************\n", rank);
Paso_SystemMatrix_print(P);
fprintf(stderr, "Rank %d Now Restriction Matrix ************************\n", rank);
Paso_SystemMatrix_print(R);
}

   /* alloc and initialize the makers */
   sum = num_Pext_cols + num_Pmain_cols;
   P_marker = TMPMEMALLOC(sum, index_t);
   A_marker = TMPMEMALLOC(num_A_cols, index_t);
   #pragma omp parallel for private(i) schedule(static)
   for (i=0; i<sum; i++) P_marker[i] = -1;
   #pragma omp parallel for private(i) schedule(static)
   for (i=0; i<num_A_cols; i++) A_marker[i] = -1;

if (MY_DEBUG) fprintf(stderr, "rank %d CP3_1 %d\n", rank, num_Pcouple_cols);
   /* Now, count the size of RAP_ext. Start with rows in R_couple */
   sum = 0;
   for (i_r=0; i_r<num_Pcouple_cols; i_r++){
     row_marker = sum;
     /* then, loop over elements in row i_r of R_couple */
     j1_ub = R_couple->pattern->ptr[i_r+1];
if (MY_DEBUG) fprintf(stderr, "rank %d CP3_2 %d (%d %d)\n", rank, i_r, R_couple->pattern->ptr[i_r], j1_ub);
     for (j1=R_couple->pattern->ptr[i_r]; j1<j1_ub; j1++){
	i1 = R_couple->pattern->index[j1];
if (MY_DEBUG) fprintf(stderr, "rank %d CP3_3 %d %d\n", rank, j1, i1);
	/* then, loop over elements in row i1 of A->col_coupleBlock */
	j2_ub = A->col_coupleBlock->pattern->ptr[i1+1];
if (MY_DEBUG) fprintf(stderr, "rank %d CP3_4 %d %d %d\n", rank, j1, i1, j2_ub);
	for (j2=A->col_coupleBlock->pattern->ptr[i1]; j2<j2_ub; j2++) {
	  i2 = A->col_coupleBlock->pattern->index[j2];

	  /* check whether entry RA[i_r, i2] has been previously visited.
	     RAP new entry is possible only if entry RA[i_r, i2] has not
	     been visited yet */
	  if (A_marker[i2] != i_r) {
	    /* first, mark entry RA[i_r,i2] as visited */
	    A_marker[i2] = i_r; 

	    /* then loop over elements in row i2 of P_ext_main */
	    j3_ub = P->row_coupleBlock->pattern->ptr[i2+1];
	    for (j3=P->row_coupleBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = P->row_coupleBlock->pattern->index[j3];

		/* check whether entry RAP[i_r,i_c] has been created. 
		   If not yet created, create the entry and increase 
		   the total number of elements in RAP_ext */
		if (P_marker[i_c] < row_marker) {
		  P_marker[i_c] = sum;
		  sum++;
		}
	    }

	    /* loop over elements in row i2 of P_ext_couple, do the same */
	    j3_ub = P->remote_coupleBlock->pattern->ptr[i2+1];
	    for (j3=P->remote_coupleBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = P->remote_coupleBlock->pattern->index[j3] + num_Pmain_cols;
		if (P_marker[i_c] < row_marker) {
		  P_marker[i_c] = sum;
		  sum++;
		}
	    }
	  }
	}

	/* now loop over elements in row i1 of A->mainBlock, repeat
	   the process we have done to block A->col_coupleBlock */
	j2_ub = A->mainBlock->pattern->ptr[i1+1];
	for (j2=A->mainBlock->pattern->ptr[i1]; j2<j2_ub; j2++) {
	  i2 = A->mainBlock->pattern->index[j2];
	  if (A_marker[i2 + num_Acouple_cols] != i_r) {
	    A_marker[i2 + num_Acouple_cols] = i_r;
	    j3_ub = P->mainBlock->pattern->ptr[i2+1];
	    for (j3=P->mainBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = P->mainBlock->pattern->index[j3];
		if (P_marker[i_c] < row_marker) {
		  P_marker[i_c] = sum;
		  sum++;
		}
	    }
	    j3_ub = P->col_coupleBlock->pattern->ptr[i2+1];
	    for (j3=P->col_coupleBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		/* note that we need to map the column index in 
		   P->col_coupleBlock back into the column index in 
		   P_ext_couple */
		i_c = Pcouple_to_Pext[P->col_coupleBlock->pattern->index[j3]] + num_Pmain_cols;
if (MY_DEBUG1 && (i_c < 0 || i_c >= num_Pext_cols + num_Pmain_cols)) fprintf(stderr, "rank %d access to P_marker excceeded( %d-%d out of %d+%d) 5\n", rank, i_c, P->col_coupleBlock->pattern->index[j3], num_Pext_cols, num_Pmain_cols);
		if (P_marker[i_c] < row_marker) {
		  P_marker[i_c] = sum;
		  sum++;
		}
	    }
	  }
	}
     }
   }
if (MY_DEBUG1) fprintf(stderr, "rank %d CP4 num_Pcouple_cols %d sum %d\n", rank, num_Pcouple_cols, sum);

   /* Now we have the number of non-zero elements in RAP_ext, allocate
      PAP_ext_ptr, RAP_ext_idx and RAP_ext_val */
   RAP_ext_ptr = MEMALLOC(num_Pcouple_cols+1, index_t);
   RAP_ext_idx = MEMALLOC(sum, index_t);
   RAP_ext_val = MEMALLOC(sum, double);

   /* Fill in the RAP_ext_ptr, RAP_ext_idx, RAP_val */
   sum = num_Pext_cols + num_Pmain_cols;
   #pragma omp parallel for private(i) schedule(static)
   for (i=0; i<sum; i++) P_marker[i] = -1;
   #pragma omp parallel for private(i) schedule(static)
   for (i=0; i<num_A_cols; i++) A_marker[i] = -1;
if (MY_DEBUG) fprintf(stderr, "rank %d CP4_1 sum %d\n", rank, sum);
   sum = 0;
   RAP_ext_ptr[0] = 0;
   for (i_r=0; i_r<num_Pcouple_cols; i_r++){
     row_marker = sum;
if (MY_DEBUG) fprintf(stderr, "rank %d CP4_2 i_r %d sum %d\n", rank, i_r, sum);
     /* then, loop over elements in row i_r of R_couple */
     j1_ub = R_couple->pattern->ptr[i_r+1];
     for (j1=R_couple->pattern->ptr[i_r]; j1<j1_ub; j1++){
	i1 = R_couple->pattern->index[j1];
	R_val = R_couple->val[j1];

	/* then, loop over elements in row i1 of A->col_coupleBlock */
	j2_ub = A->col_coupleBlock->pattern->ptr[i1+1];
	for (j2=A->col_coupleBlock->pattern->ptr[i1]; j2<j2_ub; j2++) {
	  i2 = A->col_coupleBlock->pattern->index[j2];
	  RA_val = R_val * A->col_coupleBlock->val[j2];

	  /* check whether entry RA[i_r, i2] has been previously visited.
	     RAP new entry is possible only if entry RA[i_r, i2] has not
	     been visited yet */
	  if (A_marker[i2] != i_r) {
	    /* first, mark entry RA[i_r,i2] as visited */
	    A_marker[i2] = i_r; 

	    /* then loop over elements in row i2 of P_ext_main */
	    j3_ub = P->row_coupleBlock->pattern->ptr[i2+1];
	    for (j3=P->row_coupleBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = P->row_coupleBlock->pattern->index[j3];
		RAP_val = RA_val * P->row_coupleBlock->val[j3];

		/* check whether entry RAP[i_r,i_c] has been created. 
		   If not yet created, create the entry and increase 
		   the total number of elements in RAP_ext */
		if (P_marker[i_c] < row_marker) {
		  P_marker[i_c] = sum;
		  RAP_ext_val[sum] = RAP_val;
		  RAP_ext_idx[sum] = i_c + offset; 
if (MY_DEBUG){
if (rank == 1) fprintf(stderr, "1-ext[%d (%d,%d)]=%f\n", sum, i_r, i_c, RAP_ext_val[sum]);
}
		  sum++;
		} else {
		  RAP_ext_val[P_marker[i_c]] += RAP_val;
if (MY_DEBUG){
if (rank == 1) fprintf(stderr, "1-ext[(%d,%d)]+=%f\n", i_r, i_c, RAP_val);
}
		}
	    }

	    /* loop over elements in row i2 of P_ext_couple, do the same */
	    j3_ub = P->remote_coupleBlock->pattern->ptr[i2+1];
	    for (j3=P->remote_coupleBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = P->remote_coupleBlock->pattern->index[j3] + num_Pmain_cols;
		RAP_val = RA_val * P->remote_coupleBlock->val[j3];

		if (P_marker[i_c] < row_marker) {
		  P_marker[i_c] = sum;
		  RAP_ext_val[sum] = RAP_val;
		  RAP_ext_idx[sum] = global_id_P[i_c - num_Pmain_cols];
if (MY_DEBUG){
if (rank == 1) fprintf(stderr, "ext[%d (%d,%d)]=%f (R%f, A(%d %d)%f, P%f)\n", sum, i_r, i_c, RAP_ext_val[sum], R_val, i1, i2, A->col_coupleBlock->val[j2], P->remote_coupleBlock->val[j3]);
}
		  sum++;
		} else {
		  RAP_ext_val[P_marker[i_c]] += RAP_val;
if (MY_DEBUG){
if (rank == 1) fprintf(stderr, "ext[(%d,%d)]+=%f\n", i_r, i_c, RAP_val);
}
		}
	    }

	  /* If entry RA[i_r, i2] is visited, no new RAP entry is created.
	     Only the contributions are added. */
	  } else {
	    j3_ub = P->row_coupleBlock->pattern->ptr[i2+1];
	    for (j3=P->row_coupleBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = P->row_coupleBlock->pattern->index[j3];
		RAP_val = RA_val * P->row_coupleBlock->val[j3];
		RAP_ext_val[P_marker[i_c]] += RAP_val;
if (MY_DEBUG){
if (rank == 1) fprintf(stderr, "Visited 1-ext[%d, %d]+=%f\n", i_r, i_c, RAP_val);
}
	    }
	    j3_ub = P->remote_coupleBlock->pattern->ptr[i2+1];
	    for (j3=P->remote_coupleBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = P->remote_coupleBlock->pattern->index[j3] + num_Pmain_cols;
		RAP_val = RA_val * P->remote_coupleBlock->val[j3];
		RAP_ext_val[P_marker[i_c]] += RAP_val;
if (MY_DEBUG){
if (rank == 1) fprintf(stderr, "Visited ext[%d, %d]+=%f\n", i_r, i_c, RAP_val);
}
	    }
	  }
	}

	/* now loop over elements in row i1 of A->mainBlock, repeat
	   the process we have done to block A->col_coupleBlock */
	j2_ub = A->mainBlock->pattern->ptr[i1+1];
	for (j2=A->mainBlock->pattern->ptr[i1]; j2<j2_ub; j2++) {
	  i2 = A->mainBlock->pattern->index[j2];
	  RA_val = R_val * A->mainBlock->val[j2];
	  if (A_marker[i2 + num_Acouple_cols] != i_r) {
	    A_marker[i2 + num_Acouple_cols] = i_r;
	    j3_ub = P->mainBlock->pattern->ptr[i2+1];
	    for (j3=P->mainBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = P->mainBlock->pattern->index[j3];
		RAP_val = RA_val * P->mainBlock->val[j3];
		if (P_marker[i_c] < row_marker) {
		  P_marker[i_c] = sum;
		  RAP_ext_val[sum] = RAP_val;
		  RAP_ext_idx[sum] = i_c + offset;
if (MY_DEBUG){
if (rank == 1) fprintf(stderr, "Main 1-ext[%d (%d, %d)]=%f\n", sum, i_r, i_c, RAP_ext_val[sum]);
}
		  sum++;
		} else {
		  RAP_ext_val[P_marker[i_c]] += RAP_val;
if (MY_DEBUG){
if (rank == 1) fprintf(stderr, "Main 1-ext[%d, %d]+=%f\n", i_r, i_c, RAP_val);
}
		}
	    }
	    j3_ub = P->col_coupleBlock->pattern->ptr[i2+1];
	    for (j3=P->col_coupleBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = Pcouple_to_Pext[P->col_coupleBlock->pattern->index[j3]] + num_Pmain_cols;
if (MY_DEBUG1 && (i_c < 0 || i_c >= num_Pext_cols + num_Pmain_cols)) fprintf(stderr, "rank %d access to P_marker excceeded( %d out of %d) 1\n", rank, i_c, num_Pext_cols + num_Pmain_cols);
		RAP_val = RA_val * P->col_coupleBlock->val[j3];
		if (P_marker[i_c] < row_marker) {
		  P_marker[i_c] = sum;
		  RAP_ext_val[sum] = RAP_val;
                  RAP_ext_idx[sum] = global_id_P[i_c - num_Pmain_cols];
if (MY_DEBUG){
if (rank == 1) fprintf(stderr, "Main ext[%d (%d, %d)]=%f\n", sum, i_r, i_c, RAP_ext_val[sum]);
}
		  sum++;
		} else {
		  RAP_ext_val[P_marker[i_c]] += RAP_val;
if (MY_DEBUG){
if (rank == 1) fprintf(stderr, "Main ext[%d, %d]+=%f\n", i_r, i_c, RAP_val);
}
		}
	    }
	  } else {
	    j3_ub = P->mainBlock->pattern->ptr[i2+1];
	    for (j3=P->mainBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = P->mainBlock->pattern->index[j3];
		RAP_val = RA_val * P->mainBlock->val[j3];
		RAP_ext_val[P_marker[i_c]] += RAP_val;
if (MY_DEBUG){
if (rank == 1) fprintf(stderr, "Main Visited 1-ext[%d, %d]+=%f\n", i_r, i_c, RAP_val);
}
	    }
	    j3_ub = P->col_coupleBlock->pattern->ptr[i2+1];
	    for (j3=P->col_coupleBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = Pcouple_to_Pext[P->col_coupleBlock->pattern->index[j3]] + num_Pmain_cols;
		RAP_val = RA_val * P->col_coupleBlock->val[j3];
		RAP_ext_val[P_marker[i_c]] += RAP_val;
if (MY_DEBUG){
if (rank == 1) fprintf(stderr, "Main Visited ext[%d, %d]+=%f\n", i_r, i_c, RAP_val);
}
	    }
	  }
	}
     }
     RAP_ext_ptr[i_r+1] = sum;
   }
   TMPMEMFREE(P_marker);
   TMPMEMFREE(Pcouple_to_Pext);
if (MY_DEBUG) fprintf(stderr, "rank %d CP5 num_Pcouple_cols %d sum %d\n", rank, num_Pcouple_cols, sum);

if (MY_DEBUG) {
char *str1, *str2;
str1 = TMPMEMALLOC(sum*100+100, char);
str2 = TMPMEMALLOC(15, char);
sprintf(str1, "rank %d RAP_ext_idx[%d] = (", rank, sum);
for (i=0; i<sum; i++) {
  sprintf(str2, "%d ", RAP_ext_idx[i]);
  strcat(str1, str2);
}
fprintf(stderr, "%s)\n", str1);
TMPMEMFREE(str1);
TMPMEMFREE(str2);
}

   /* Now we have part of RAP[r,c] where row "r" is the list of rows 
      which is given by the column list of P->col_coupleBlock, and 
      column "c" is the list of columns which possiblely covers the
      whole column range of systme martris P. This part of data is to
      be passed to neighboring processors, and added to corresponding
      RAP[r,c] entries in the neighboring processors */
   Paso_Preconditioner_AMG_CopyRemoteData(P, &RAP_ext_ptr, &RAP_ext_idx,
		&RAP_ext_val, global_id_P);
if (MY_DEBUG1) fprintf(stderr, "rank %d CP6\n", rank);

   num_RAPext_rows = P->col_coupler->connector->send->offsetInShared[P->col_coupler->connector->send->numNeighbors];
   sum = RAP_ext_ptr[num_RAPext_rows];
if (MY_DEBUG) fprintf(stderr, "rank %d CP6_0 %d %d (%d %d)\n", rank, sum, num_Pext_cols, num_RAPext_rows, num_Pcouple_cols);
   num_RAPext_cols = 0;
   if (num_Pext_cols || sum > 0) {
     temp = TMPMEMALLOC(num_Pext_cols+sum, index_t);
     j1_ub = offset + num_Pmain_cols;
     for (i=0, iptr=0; i<sum; i++) {
	if (RAP_ext_idx[i] < offset || RAP_ext_idx[i] >= j1_ub)  /* XXX */ {
	  temp[iptr++] = RAP_ext_idx[i];		  /* XXX */
if (MY_DEBUG && rank ==1) fprintf(stderr, "RAP_ext_idx[%d]=%d\n", i, RAP_ext_idx[i]);
}
     }
     for (j=0; j<num_Pext_cols; j++, iptr++){
	temp[iptr] = global_id_P[j];
     }

     if (iptr) {
	#ifdef USE_QSORTG
	  qsortG(temp, (size_t)iptr, sizeof(index_t), Paso_comparIndex);
	#else
	  qsort(temp, (size_t)iptr, sizeof(index_t), Paso_comparIndex);
	#endif
	num_RAPext_cols = 1;
	i = temp[0];
	for (j=1; j<iptr; j++) {
	  if (temp[j] > i) {
	    i = temp[j];
	    temp[num_RAPext_cols++] = i;
	  }
	}

if (MY_DEBUG) {
char *str1, *str2;
index_t my_sum=iptr, i;
str1 = TMPMEMALLOC(my_sum*20+100, char);
str2 = TMPMEMALLOC(15, char);
sprintf(str1, "rank %d temp[%d] = (", rank, my_sum);
for (i=0; i<my_sum; i++){
  sprintf(str2, "%d ", temp[i]);
  strcat(str1, str2);
}
fprintf(stderr, "%s)\n", str1);
TMPMEMFREE(str1);
TMPMEMFREE(str2);
}
     }
   }
if (MY_DEBUG) fprintf(stderr, "rank %d CP6_1 %d %d\n", rank, iptr, num_RAPext_cols);

   /* resize the pattern of P_ext_couple */
   if(num_RAPext_cols){
     global_id_RAP = TMPMEMALLOC(num_RAPext_cols, index_t);
     for (i=0; i<num_RAPext_cols; i++)
	global_id_RAP[i] = temp[i];
   }
if (MY_DEBUG) {
char *str1, *str2;
index_t my_sum=num_RAPext_cols, i;
str1 = TMPMEMALLOC(my_sum*20+100, char);
str2 = TMPMEMALLOC(15, char);
sprintf(str1, "rank %d global_id_RAP[%d] = (", rank, my_sum);
for (i=0; i<my_sum; i++){
  sprintf(str2, "%d ", global_id_RAP[i]);
  strcat(str1, str2);
}
fprintf(stderr, "%s)\n", str1);
TMPMEMFREE(str1);
TMPMEMFREE(str2);
}
   if (num_Pext_cols || sum > 0)
     TMPMEMFREE(temp);
   j1_ub = offset + num_Pmain_cols;
   for (i=0; i<sum; i++) {
     if (RAP_ext_idx[i] < offset || RAP_ext_idx[i] >= j1_ub){
	where_p = (index_t *) bsearch(&(RAP_ext_idx[i]), global_id_RAP,
/*XXX*/			num_RAPext_cols, sizeof(index_t), Paso_comparIndex);
	RAP_ext_idx[i] = num_Pmain_cols + (index_t)(where_p - global_id_RAP);
     } else 
	RAP_ext_idx[i] = RAP_ext_idx[i] - offset;
   }
if (MY_DEBUG) fprintf(stderr, "rank %d CP7 %d\n", rank, num_Pext_cols);

   /* build the mapping */
   if (num_Pcouple_cols) {
     Pcouple_to_RAP = TMPMEMALLOC(num_Pcouple_cols, index_t);
     iptr = 0;
     for (i=0; i<num_RAPext_cols; i++) 
	if (global_id_RAP[i] == P->global_id[iptr]) {
	  Pcouple_to_RAP[iptr++] = i;
	  if (iptr == num_Pcouple_cols) break;
	}
   }

   if (num_Pext_cols) {
     Pext_to_RAP = TMPMEMALLOC(num_Pext_cols, index_t);
     iptr = 0;
     for (i=0; i<num_RAPext_cols; i++)
	if (global_id_RAP[i] == global_id_P[iptr]) {
	  Pext_to_RAP[iptr++] = i;
	  if (iptr == num_Pext_cols) break;
	}
   }

   if (global_id_P){
     TMPMEMFREE(global_id_P);
     global_id_P = NULL;
   }

   /* alloc and initialize the makers */
   sum = num_RAPext_cols + num_Pmain_cols;
   P_marker = TMPMEMALLOC(sum, index_t);
   #pragma omp parallel for private(i) schedule(static)
   for (i=0; i<sum; i++) P_marker[i] = -1;
   #pragma omp parallel for private(i) schedule(static)
   for (i=0; i<num_A_cols; i++) A_marker[i] = -1;
if (MY_DEBUG1) fprintf(stderr, "rank %d CP8 %d\n", rank, num_Pmain_cols);

   /* Now, count the size of RAP. Start with rows in R_main */
   num_neighbors = P->col_coupler->connector->send->numNeighbors;
   offsetInShared = P->col_coupler->connector->send->offsetInShared;
   shared = P->col_coupler->connector->send->shared;
   i = 0;
   j = 0;
   for (i_r=0; i_r<num_Pmain_cols; i_r++){
     /* Mark the start of row for both main block and couple block */
     row_marker = i;
     row_marker_ext = j;

     /* Mark the diagonal element RAP[i_r, i_r], and other elements
	in RAP_ext */
     P_marker[i_r] = i;
     i++;
     for (j1=0; j1<num_neighbors; j1++) {
	for (j2=offsetInShared[j1]; j2<offsetInShared[j1+1]; j2++) {
	  if (shared[j2] == i_r) {
	    for (k=RAP_ext_ptr[j2]; k<RAP_ext_ptr[j2+1]; k++) {
	      i_c = RAP_ext_idx[k];
	      if (i_c < num_Pmain_cols) {
		if (P_marker[i_c] < row_marker) {
		  P_marker[i_c] = i;
		  i++;
		}
	      } else {
		if (P_marker[i_c] < row_marker_ext) {
		  P_marker[i_c] = j;
		  j++;
		}
	      }
	    }
	    break;
	  }
	}
     }

     /* then, loop over elements in row i_r of R_main */
     j1_ub = R_main->pattern->ptr[i_r+1];
     for (j1=R_main->pattern->ptr[i_r]; j1<j1_ub; j1++){
	i1 = R_main->pattern->index[j1];

	/* then, loop over elements in row i1 of A->col_coupleBlock */
	j2_ub = A->col_coupleBlock->pattern->ptr[i1+1];
	for (j2=A->col_coupleBlock->pattern->ptr[i1]; j2<j2_ub; j2++) {
	  i2 = A->col_coupleBlock->pattern->index[j2];

	  /* check whether entry RA[i_r, i2] has been previously visited.
	     RAP new entry is possible only if entry RA[i_r, i2] has not
	     been visited yet */
	  if (A_marker[i2] != i_r) {
	    /* first, mark entry RA[i_r,i2] as visited */
	    A_marker[i2] = i_r; 

	    /* then loop over elements in row i2 of P_ext_main */
	    j3_ub = P->row_coupleBlock->pattern->ptr[i2+1];
	    for (j3=P->row_coupleBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = P->row_coupleBlock->pattern->index[j3];

		/* check whether entry RAP[i_r,i_c] has been created. 
		   If not yet created, create the entry and increase 
		   the total number of elements in RAP_ext */
		if (P_marker[i_c] < row_marker) {
		  P_marker[i_c] = i;
		  i++;
		}
	    }

	    /* loop over elements in row i2 of P_ext_couple, do the same */
	    j3_ub = P->remote_coupleBlock->pattern->ptr[i2+1];
	    for (j3=P->remote_coupleBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = Pext_to_RAP[P->remote_coupleBlock->pattern->index[j3]] + num_Pmain_cols;
		if (P_marker[i_c] < row_marker_ext) {
		  P_marker[i_c] = j;
		  j++;
		}
	    }
	  }
	}

	/* now loop over elements in row i1 of A->mainBlock, repeat
	   the process we have done to block A->col_coupleBlock */
	j2_ub = A->mainBlock->pattern->ptr[i1+1];
	for (j2=A->mainBlock->pattern->ptr[i1]; j2<j2_ub; j2++) {
	  i2 = A->mainBlock->pattern->index[j2];
	  if (A_marker[i2 + num_Acouple_cols] != i_r) {
	    A_marker[i2 + num_Acouple_cols] = i_r;
	    j3_ub = P->mainBlock->pattern->ptr[i2+1];
	    for (j3=P->mainBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = P->mainBlock->pattern->index[j3];
		if (P_marker[i_c] < row_marker) {
		  P_marker[i_c] = i;
		  i++;
		}
	    }
	    j3_ub = P->col_coupleBlock->pattern->ptr[i2+1];
	    for (j3=P->col_coupleBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		/* note that we need to map the column index in 
		   P->col_coupleBlock back into the column index in 
		   P_ext_couple */
		i_c = Pcouple_to_RAP[P->col_coupleBlock->pattern->index[j3]] + num_Pmain_cols;
if (MY_DEBUG1 && (i_c <0 || i_c >= sum)) fprintf(stderr, "rank %d access to P_marker excceeded( %d out of %d) 2\n", rank, i_c, sum);
		if (P_marker[i_c] < row_marker_ext) {
		  P_marker[i_c] = j;
		  j++;
		}
	    }
	  }
	}
     }
   }
if (MY_DEBUG) fprintf(stderr, "rank %d CP9 main_size%d couple_size%d\n", rank, i, j);

   /* Now we have the number of non-zero elements in RAP_main and RAP_couple.
      Allocate RAP_main_ptr, RAP_main_idx and RAP_main_val for RAP_main, 
      and allocate RAP_couple_ptr, RAP_couple_idx and RAP_couple_val for
      RAP_couple */
   RAP_main_ptr = MEMALLOC(num_Pmain_cols+1, index_t);
   RAP_main_idx = MEMALLOC(i, index_t);
   RAP_main_val = TMPMEMALLOC(i, double);
   RAP_couple_ptr = MEMALLOC(num_Pmain_cols+1, index_t);
   RAP_couple_idx = MEMALLOC(j, index_t);
   RAP_couple_val = TMPMEMALLOC(j, double);

   RAP_main_ptr[num_Pmain_cols] = i;
   RAP_couple_ptr[num_Pmain_cols] = j;

   #pragma omp parallel for private(i) schedule(static)
   for (i=0; i<sum; i++) P_marker[i] = -1;
   #pragma omp parallel for private(i) schedule(static)
   for (i=0; i<num_A_cols; i++) A_marker[i] = -1;

   /* Now, Fill in the data for RAP_main and RAP_couple. Start with rows 
      in R_main */
   i = 0;
   j = 0;
   for (i_r=0; i_r<num_Pmain_cols; i_r++){
     /* Mark the start of row for both main block and couple block */
     row_marker = i;
     row_marker_ext = j;
     RAP_main_ptr[i_r] = row_marker;
     RAP_couple_ptr[i_r] = row_marker_ext;

     /* Mark and setup the diagonal element RAP[i_r, i_r], and elements
	in row i_r of RAP_ext */
     P_marker[i_r] = i;
     RAP_main_val[i] = ZERO;
     RAP_main_idx[i] = i_r;
     i++;

     for (j1=0; j1<num_neighbors; j1++) {
	for (j2=offsetInShared[j1]; j2<offsetInShared[j1+1]; j2++) {
	  if (shared[j2] == i_r) {
	    for (k=RAP_ext_ptr[j2]; k<RAP_ext_ptr[j2+1]; k++) {
	      i_c = RAP_ext_idx[k];
	      if (i_c < num_Pmain_cols) {
		if (P_marker[i_c] < row_marker) {
		  P_marker[i_c] = i;
		  RAP_main_val[i] = RAP_ext_val[k];
if (MY_DEBUG){
if (rank == 0 && i == 2) fprintf(stderr, "ext %f\n", RAP_ext_val[k]);
}
		  RAP_main_idx[i] = i_c;
		  i++;
		} else {
		  RAP_main_val[P_marker[i_c]] += RAP_ext_val[k];
if (MY_DEBUG){ 
if (rank == 0 && P_marker[i_c] == 2) fprintf(stderr, "i_c %d ext %f\n", i_c, RAP_ext_val[k]);
}  
		}
	      } else {
		if (P_marker[i_c] < row_marker_ext) {
		  P_marker[i_c] = j;
		  RAP_couple_val[j] = RAP_ext_val[k];
		  RAP_couple_idx[j] = i_c - num_Pmain_cols;
		  j++;
		} else {
		  RAP_couple_val[P_marker[i_c]] += RAP_ext_val[k];
		}
	      }
	    }
	    break;
	  }
	}
     }

     /* then, loop over elements in row i_r of R_main */
     j1_ub = R_main->pattern->ptr[i_r+1];
     for (j1=R_main->pattern->ptr[i_r]; j1<j1_ub; j1++){
	i1 = R_main->pattern->index[j1];
	R_val = R_main->val[j1];

	/* then, loop over elements in row i1 of A->col_coupleBlock */
	j2_ub = A->col_coupleBlock->pattern->ptr[i1+1];
	for (j2=A->col_coupleBlock->pattern->ptr[i1]; j2<j2_ub; j2++) {
	  i2 = A->col_coupleBlock->pattern->index[j2];
	  RA_val = R_val * A->col_coupleBlock->val[j2];

	  /* check whether entry RA[i_r, i2] has been previously visited.
	     RAP new entry is possible only if entry RA[i_r, i2] has not
	     been visited yet */
	  if (A_marker[i2] != i_r) {
	    /* first, mark entry RA[i_r,i2] as visited */
	    A_marker[i2] = i_r; 

	    /* then loop over elements in row i2 of P_ext_main */
	    j3_ub = P->row_coupleBlock->pattern->ptr[i2+1];
	    for (j3=P->row_coupleBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = P->row_coupleBlock->pattern->index[j3];
		RAP_val = RA_val * P->row_coupleBlock->val[j3];

		/* check whether entry RAP[i_r,i_c] has been created. 
		   If not yet created, create the entry and increase 
		   the total number of elements in RAP_ext */
		if (P_marker[i_c] < row_marker) {
		  P_marker[i_c] = i;
		  RAP_main_val[i] = RAP_val;
if (MY_DEBUG){ 
if (rank == 0 && i == 2) fprintf(stderr, "R[%d %d] %f A[%d %d] %f P[%d %d] %f \n", i_r, i1, R_val, i1, i2, A->col_coupleBlock->val[j2], i2, i_c, P->row_coupleBlock->val[j3]);
}  
		  RAP_main_idx[i] = i_c;
		  i++;
		} else {
		  RAP_main_val[P_marker[i_c]] += RAP_val;
if (MY_DEBUG){
if (rank == 0 && P_marker[i_c] == 2) fprintf(stderr, "row_marker R[%d %d] %f A[%d %d] %f P[%d %d] %f \n", i_r, i1 , R_val, i1, i2, A->col_coupleBlock->val[j2], i2, i_c, P->row_coupleBlock->val[j3]);
} 
		}
	    }

	    /* loop over elements in row i2 of P_ext_couple, do the same */
	    j3_ub = P->remote_coupleBlock->pattern->ptr[i2+1];
	    for (j3=P->remote_coupleBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = Pext_to_RAP[P->remote_coupleBlock->pattern->index[j3]] + num_Pmain_cols;
		RAP_val = RA_val * P->remote_coupleBlock->val[j3];
		if (P_marker[i_c] < row_marker_ext) {
		  P_marker[i_c] = j;
		  RAP_couple_val[j] = RAP_val;
		  RAP_couple_idx[j] = i_c - num_Pmain_cols;
		  j++;
		} else {
		  RAP_couple_val[P_marker[i_c]] += RAP_val;
		}
	    }

	  /* If entry RA[i_r, i2] is visited, no new RAP entry is created.
	     Only the contributions are added. */
	  } else {
	    j3_ub = P->row_coupleBlock->pattern->ptr[i2+1];
	    for (j3=P->row_coupleBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = P->row_coupleBlock->pattern->index[j3];
		RAP_val = RA_val * P->row_coupleBlock->val[j3];
		RAP_main_val[P_marker[i_c]] += RAP_val;
if (MY_DEBUG){
if (rank == 0 && P_marker[i_c] == 2) fprintf(stderr, "visited R[%d %d] %f A[%d %d] %f P[%d %d] %f \n", i_r, i1, R_val, i1, i2, A->col_coupleBlock->val[j2], i2, i_c, P->row_coupleBlock->val[j3]);
} 
	    }
	    j3_ub = P->remote_coupleBlock->pattern->ptr[i2+1];
	    for (j3=P->remote_coupleBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = Pext_to_RAP[P->remote_coupleBlock->pattern->index[j3]] + num_Pmain_cols;
		RAP_val = RA_val * P->remote_coupleBlock->val[j3];
		RAP_couple_val[P_marker[i_c]] += RAP_val;
	    }
	  }
	}

	/* now loop over elements in row i1 of A->mainBlock, repeat
	   the process we have done to block A->col_coupleBlock */
	j2_ub = A->mainBlock->pattern->ptr[i1+1];
	for (j2=A->mainBlock->pattern->ptr[i1]; j2<j2_ub; j2++) {
	  i2 = A->mainBlock->pattern->index[j2];
	  RA_val = R_val * A->mainBlock->val[j2];
	  if (A_marker[i2 + num_Acouple_cols] != i_r) {
	    A_marker[i2 + num_Acouple_cols] = i_r;
	    j3_ub = P->mainBlock->pattern->ptr[i2+1];
	    for (j3=P->mainBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = P->mainBlock->pattern->index[j3];
		RAP_val = RA_val * P->mainBlock->val[j3];
		if (P_marker[i_c] < row_marker) {
		  P_marker[i_c] = i;
		  RAP_main_val[i] = RAP_val;
if (MY_DEBUG){
if (rank == 0 && i == 2) fprintf(stderr, "Main R[%d %d] %f A[%d %d] %f P[%d %d] %f \n", i_r, i1, R_val, i1, i2, A->mainBlock->val[j2], i2, i_c, P->mainBlock->val[j3]);
} 
		  RAP_main_idx[i] = i_c;
		  i++;
		} else {
		  RAP_main_val[P_marker[i_c]] += RAP_val;
if (MY_DEBUG){   
if (rank == 0 && P_marker[i_c] == 2) fprintf(stderr, "Main row_marker R[%d %d] %f A[%d %d] %f P[%d %d] %f \n", i_r, i1, R_val, i1, i2, A->mainBlock->val[j2], i2, i_c, P->mainBlock->val[j3]);
}
		}
	    }
	    j3_ub = P->col_coupleBlock->pattern->ptr[i2+1];
	    for (j3=P->col_coupleBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		/* note that we need to map the column index in 
		   P->col_coupleBlock back into the column index in 
		   P_ext_couple */
		i_c = Pcouple_to_RAP[P->col_coupleBlock->pattern->index[j3]] + num_Pmain_cols;
if (MY_DEBUG1 && (i_c < 0 || i_c >= sum)) fprintf(stderr, "rank %d access to P_marker excceeded( %d out of %d--%d,%d) 3\n", rank, i_c, sum, num_RAPext_cols, num_Pmain_cols);
		RAP_val = RA_val * P->col_coupleBlock->val[j3];
		if (P_marker[i_c] < row_marker_ext) {
		  P_marker[i_c] = j;
		  RAP_couple_val[j] = RAP_val;
		  RAP_couple_idx[j] = i_c - num_Pmain_cols;
		  j++;
		} else {
		  RAP_couple_val[P_marker[i_c]] += RAP_val;
		}
	    }

	  } else {
	    j3_ub = P->mainBlock->pattern->ptr[i2+1];
	    for (j3=P->mainBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = P->mainBlock->pattern->index[j3];
		RAP_val = RA_val * P->mainBlock->val[j3];
		RAP_main_val[P_marker[i_c]] += RAP_val;
if (MY_DEBUG){
if (rank == 0 && P_marker[i_c] == 2) fprintf(stderr, "Main Visited R[%d %d] %f A[%d %d] %f P[%d %d] %f \n", i_r, i1, R_val, i1, i2, A->mainBlock->val[j2], i2, i_c, P->mainBlock->val[j3]);
}
	    }
	    j3_ub = P->col_coupleBlock->pattern->ptr[i2+1];
	    for (j3=P->col_coupleBlock->pattern->ptr[i2]; j3<j3_ub; j3++) {
		i_c = Pcouple_to_RAP[P->col_coupleBlock->pattern->index[j3]] + num_Pmain_cols;
if (MY_DEBUG1 && i_c >= sum) fprintf(stderr, "rank %d access to P_marker excceeded( %d out of %d -- %d,%d) 4\n", rank, i_c, sum, num_RAPext_cols, num_Pmain_cols);
		RAP_val = RA_val * P->col_coupleBlock->val[j3];
		RAP_couple_val[P_marker[i_c]] += RAP_val;
	    }
	  }
	}
     }

     /* sort RAP_XXXX_idx and reorder RAP_XXXX_val accordingly */
     if (i > row_marker) {
	offset = i - row_marker;
	temp = TMPMEMALLOC(offset, index_t);
	for (iptr=0; iptr<offset; iptr++)
	  temp[iptr] = RAP_main_idx[row_marker+iptr];
	if (offset > 0) {
	  #ifdef USE_QSORTG
	    qsortG(temp, (size_t)offset, sizeof(index_t), Paso_comparIndex);
	  #else
	    qsort(temp, (size_t)offset, sizeof(index_t), Paso_comparIndex);
	  #endif
	}
	temp_val = TMPMEMALLOC(offset, double);
	for (iptr=0; iptr<offset; iptr++){
	  k = P_marker[temp[iptr]];
	  temp_val[iptr] = RAP_main_val[k];
	  P_marker[temp[iptr]] = iptr + row_marker;
	}
	for (iptr=0; iptr<offset; iptr++){
	  RAP_main_idx[row_marker+iptr] = temp[iptr];
	  RAP_main_val[row_marker+iptr] = temp_val[iptr];
	}
	TMPMEMFREE(temp);
	TMPMEMFREE(temp_val);
     }
     if (j > row_marker_ext) {
        offset = j - row_marker_ext;
        temp = TMPMEMALLOC(offset, index_t);
        for (iptr=0; iptr<offset; iptr++)
          temp[iptr] = RAP_couple_idx[row_marker_ext+iptr];
        if (offset > 0) {
          #ifdef USE_QSORTG
            qsortG(temp, (size_t)offset, sizeof(index_t), Paso_comparIndex);
          #else
            qsort(temp, (size_t)offset, sizeof(index_t), Paso_comparIndex);
          #endif
        }
        temp_val = TMPMEMALLOC(offset, double);
        for (iptr=0; iptr<offset; iptr++){
if (MY_DEBUG1 && temp[iptr] >= num_RAPext_cols) 
fprintf(stderr, "rank%d index %d exceed %d\n", rank, temp[iptr], num_RAPext_cols);
          k = P_marker[temp[iptr] + num_Pmain_cols];
          temp_val[iptr] = RAP_couple_val[k];
          P_marker[temp[iptr] + num_Pmain_cols] = iptr + row_marker_ext;
        }
        for (iptr=0; iptr<offset; iptr++){
          RAP_couple_idx[row_marker_ext+iptr] = temp[iptr];
          RAP_couple_val[row_marker_ext+iptr] = temp_val[iptr];
        }
        TMPMEMFREE(temp);
        TMPMEMFREE(temp_val);
     }
   }

   TMPMEMFREE(A_marker);
   TMPMEMFREE(Pext_to_RAP);
   TMPMEMFREE(Pcouple_to_RAP);
   MEMFREE(RAP_ext_ptr);
   MEMFREE(RAP_ext_idx);
   MEMFREE(RAP_ext_val);
   Paso_SparseMatrix_free(R_main);
   Paso_SparseMatrix_free(R_couple);

if (MY_DEBUG){
if (rank == 0) fprintf(stderr, "A[%d]=%f A[%d]=%f A[%d]=%f A[%d]=%f \n", RAP_main_idx[0], RAP_main_val[0], RAP_main_idx[1], RAP_main_val[1], RAP_main_idx[2], RAP_main_val[2], RAP_main_idx[3], RAP_main_val[3]);
}
if (MY_DEBUG1) fprintf(stderr, "rank %d CP10\n", rank);
if (MY_DEBUG1 && rank == 1) {
int sum = num_RAPext_cols, my_i;
char *str1, *str2;
str1 = TMPMEMALLOC(sum*100+100, char);
str2 = TMPMEMALLOC(100, char);
sprintf(str1, "rank %d global_id_RAP[%d] = (", rank, sum);
for (my_i=0; my_i<sum; my_i++){
  sprintf(str2, "%d ", global_id_RAP[my_i]);
  strcat(str1, str2);
}
fprintf(stderr, "%s)\n", str1);
sprintf(str1, "rank %d distribution = (", rank);
for (my_i=0; my_i<size; my_i++){
  sprintf(str2, "%d ", P->pattern->input_distribution->first_component[my_i+1]);
  strcat(str1, str2);
}
fprintf(stderr, "%s)\n", str1);
TMPMEMFREE(str1);
TMPMEMFREE(str2);
}


   /* Check whether there are empty columns in RAP_couple */ 
   #pragma omp parallel for schedule(static) private(i)
   for (i=0; i<num_RAPext_cols; i++) P_marker[i] = 1;
   /* num of non-empty columns is stored in "k" */
   k = 0;  
   j = RAP_couple_ptr[num_Pmain_cols];
if (MY_DEBUG) fprintf(stderr, "rank %d CP10_1 %d %d %d\n", rank, num_RAPext_cols, num_Pmain_cols, j);
   for (i=0; i<j; i++) {
     i1 = RAP_couple_idx[i];
     if (P_marker[i1]) {
	P_marker[i1] = 0;
	k++;
     }
   }

if (MY_DEBUG) fprintf(stderr, "rank %d CP10_2 %d\n", rank, k);
   /* empty columns is found */
   if (k < num_RAPext_cols) {
     temp = TMPMEMALLOC(k, index_t);
     k = 0;
     for (i=0; i<num_RAPext_cols; i++) 
	if (!P_marker[i]) {
	  P_marker[i] = k;
	  temp[k] = global_id_RAP[i];
	  k++;
	}
     for (i=0; i<j; i++) {
	i1 = RAP_couple_idx[i];
	RAP_couple_idx[i] = P_marker[i1];
     }
     num_RAPext_cols = k;
     TMPMEMFREE(global_id_RAP);
     global_id_RAP = temp;
   }
   TMPMEMFREE(P_marker);
if (MY_DEBUG) fprintf(stderr, "rank %d CP11\n", rank);

   /******************************************************/
   /* Start to create the coasre level System Matrix A_c */
   /******************************************************/
   /* first, prepare the sender/receiver for the col_connector */
   dist = P->pattern->input_distribution->first_component;
   recv_len = TMPMEMALLOC(size, dim_t);
   send_len = TMPMEMALLOC(size, dim_t);
   neighbor = TMPMEMALLOC(size, Esys_MPI_rank);
   offsetInShared = TMPMEMALLOC(size+1, index_t);
   shared = TMPMEMALLOC(num_RAPext_cols, index_t);
   memset(recv_len, 0, sizeof(dim_t) * size);
   memset(send_len, 0, sizeof(dim_t) * size);
   num_neighbors = 0;
   offsetInShared[0] = 0;
   for (i=0, j=0, k=dist[j+1]; i<num_RAPext_cols; i++) {
if (MY_DEBUG && rank == 1) fprintf(stderr, "i%d j%d k%d %d %d\n", i, j, k, global_id_RAP[i], recv_len[j]);
     shared[i] = i + num_Pmain_cols;
     if (k <= global_id_RAP[i]) {
	if (recv_len[j] > 0) {
	  neighbor[num_neighbors] = j;
	  num_neighbors ++;
	  offsetInShared[num_neighbors] = i;
	}
	while (k <= global_id_RAP[i]) {
	  j++;
	  k = dist[j+1];
	}
     }
     recv_len[j] ++;
   }
   if (recv_len[j] > 0) {
     neighbor[num_neighbors] = j;
     num_neighbors ++;
     offsetInShared[num_neighbors] = i;
   }
   recv = Paso_SharedComponents_alloc(num_Pmain_cols, num_neighbors, 
			neighbor, shared, offsetInShared, 1, 0, mpi_info);

   MPI_Alltoall(recv_len, 1, MPI_INT, send_len, 1, MPI_INT, mpi_info->comm);
if (MY_DEBUG1) fprintf(stderr, "rank %d CP12 %d %d\n", rank, num_neighbors, num_RAPext_cols);

   #ifdef ESYS_MPI
     mpi_requests=TMPMEMALLOC(size*2,MPI_Request);
     mpi_stati=TMPMEMALLOC(size*2,MPI_Status);
   #else
     mpi_requests=TMPMEMALLOC(size*2,int);
     mpi_stati=TMPMEMALLOC(size*2,int);
   #endif
   num_neighbors = 0;
   j = 0;
   offsetInShared[0] = 0;
   for (i=0; i<size; i++) {
     if (send_len[i] > 0) {
	neighbor[num_neighbors] = i;
	num_neighbors ++;
	j += send_len[i];
	offsetInShared[num_neighbors] = j;
     }
   }
   TMPMEMFREE(shared);
   shared = TMPMEMALLOC(j, index_t);
   for (i=0, j=0; i<num_neighbors; i++) {
     k = neighbor[i];
     MPI_Irecv(&shared[j], send_len[k] , MPI_INT, k, 
		mpi_info->msg_tag_counter+k,
		mpi_info->comm, &mpi_requests[i]);
     j += send_len[k];
   }
   for (i=0, j=0; i<recv->numNeighbors; i++) {
     k = recv->neighbor[i];
     MPI_Issend(&(global_id_RAP[j]), recv_len[k], MPI_INT, k,
		mpi_info->msg_tag_counter+rank,
		mpi_info->comm, &mpi_requests[i+num_neighbors]);
     j += recv_len[k];
   }
   MPI_Waitall(num_neighbors + recv->numNeighbors, 
		mpi_requests, mpi_stati); 
   mpi_info->msg_tag_counter += size; 
if (MY_DEBUG) fprintf(stderr, "rank %d CP13\n", rank);

   j = offsetInShared[num_neighbors];
   offset = dist[rank];
   for (i=0; i<j; i++) shared[i] = shared[i] - offset;
   send = Paso_SharedComponents_alloc(num_Pmain_cols, num_neighbors, 
			neighbor, shared, offsetInShared, 1, 0, mpi_info);

   col_connector = Paso_Connector_alloc(send, recv);
   TMPMEMFREE(shared);
   Paso_SharedComponents_free(recv);
   Paso_SharedComponents_free(send);
if (MY_DEBUG1) fprintf(stderr, "rank %d CP14\n", rank);

   /* now, create row distribution (output_distri) and col
      distribution (input_distribution) */
   input_dist = Paso_Distribution_alloc(mpi_info, dist, 1, 0);
   output_dist = Paso_Distribution_alloc(mpi_info, dist, 1, 0);
if (MY_DEBUG) fprintf(stderr, "rank %d CP14_1\n", rank);

   /* then, prepare the sender/receiver for the row_connector, first, prepare
      the information for sender */
   sum = RAP_couple_ptr[num_Pmain_cols];
   len = TMPMEMALLOC(size, dim_t);
   send_ptr = TMPMEMALLOC(size, index_t*);
   send_idx = TMPMEMALLOC(size, index_t*);
   for (i=0; i<size; i++) {
     send_ptr[i] = TMPMEMALLOC(num_Pmain_cols, index_t);
     send_idx[i] = TMPMEMALLOC(sum, index_t);
     memset(send_ptr[i], 0, sizeof(index_t) * num_Pmain_cols);
   }
   memset(len, 0, sizeof(dim_t) * size);
if (MY_DEBUG) fprintf(stderr, "rank %d CP14_2 %d\n", rank, sum);
   recv = col_connector->recv;
   sum=0;
   for (i_r=0; i_r<num_Pmain_cols; i_r++) {
     i1 = RAP_couple_ptr[i_r];
     i2 = RAP_couple_ptr[i_r+1];
     if (i2 > i1) {
	/* then row i_r will be in the sender of row_connector, now check
	   how many neighbors i_r needs to be send to */
	for (j=i1; j<i2; j++) {
	  i_c = RAP_couple_idx[j];
	  /* find out the corresponding neighbor "p" of column i_c */
	  for (p=0; p<recv->numNeighbors; p++) {
	    if (i_c < recv->offsetInShared[p+1]) {
	      k = recv->neighbor[p];
if (MY_DEBUG1 && rank == 1 && k == 1)
fprintf(stderr, "******* i_r %d i_c %d p %d ub %d numNeighbors %d\n", i_r, i_c, p, recv->offsetInShared[p+1], recv->numNeighbors);
	      if (send_ptr[k][i_r] == 0) sum++;
	      send_ptr[k][i_r] ++;
	      send_idx[k][len[k]] = global_id_RAP[i_c];
	      len[k] ++;
	      break;
	    }
	  }
	}
     }
   }
if (MY_DEBUG) fprintf(stderr, "rank %d CP14_3 %d\n", rank, send_ptr[1][0]);
   if (global_id_RAP) {
     TMPMEMFREE(global_id_RAP);
     global_id_RAP = NULL;
   }
if (MY_DEBUG) fprintf(stderr, "rank %d CP14_4 %d sum%d cols%d\n", rank, len[0], sum, num_RAPext_cols);

   /* now allocate the sender */
   shared = TMPMEMALLOC(sum, index_t);
   memset(send_len, 0, sizeof(dim_t) * size);
   num_neighbors=0;
   offsetInShared[0] = 0;
if (MY_DEBUG) fprintf(stderr, "rank %d CP14_5 %d\n", rank, size);
   for (p=0, k=0; p<size; p++) {
if (MY_DEBUG && rank == 1) fprintf(stderr, "rank %d CP14_6 %d %d %d\n", rank, p, k, num_neighbors);
     for (i=0; i<num_Pmain_cols; i++) {
	if (send_ptr[p][i] > 0) {
	  shared[k] = i;
	  k++;
	  send_ptr[p][send_len[p]] = send_ptr[p][i];
	  send_len[p]++;
	}
     }
     if (k > offsetInShared[num_neighbors]) {
	neighbor[num_neighbors] = p;
	num_neighbors ++;
	offsetInShared[num_neighbors] = k;
     }
   }
if (MY_DEBUG) fprintf(stderr, "rank %d CP14_8 %d %d %d %d %d\n", rank, k, send_len[0], send_len[1], offsetInShared[1], k);
   send = Paso_SharedComponents_alloc(num_Pmain_cols, num_neighbors,
                        neighbor, shared, offsetInShared, 1, 0, mpi_info);

   /* send/recv number of rows will be sent from current proc 
      recover info for the receiver of row_connector from the sender */
   MPI_Alltoall(send_len, 1, MPI_INT, recv_len, 1, MPI_INT, mpi_info->comm);
if (MY_DEBUG) fprintf(stderr, "rank %d CP15 %d %d\n", rank, recv_len[0], recv_len[1]);
   num_neighbors = 0;
   offsetInShared[0] = 0;
   j = 0;
   for (i=0; i<size; i++) {
     if (i != rank && recv_len[i] > 0) {
	neighbor[num_neighbors] = i;
	num_neighbors ++;
	j += recv_len[i];
	offsetInShared[num_neighbors] = j;
     }
   }
if (MY_DEBUG) fprintf(stderr, "rank %d CP15_1\n", rank);
   TMPMEMFREE(shared);
   TMPMEMFREE(recv_len);
   shared = TMPMEMALLOC(j, index_t);
   k = offsetInShared[num_neighbors];
   for (i=0; i<k; i++) {
     shared[i] = i + num_Pmain_cols; 
   }
if (MY_DEBUG) fprintf(stderr, "rank %d CP15_2\n", rank);
   recv = Paso_SharedComponents_alloc(num_Pmain_cols, num_neighbors,
                        neighbor, shared, offsetInShared, 1, 0, mpi_info);
if (MY_DEBUG) fprintf(stderr, "rank %d CP15_3\n", rank);
   row_connector = Paso_Connector_alloc(send, recv);
   TMPMEMFREE(shared);
   Paso_SharedComponents_free(recv);
   Paso_SharedComponents_free(send);
if (MY_DEBUG1) fprintf(stderr, "rank %d CP16 %d R%d S%d\n", rank, k, num_neighbors, send->numNeighbors);

   /* send/recv pattern->ptr for rowCoupleBlock */
   num_RAPext_rows = offsetInShared[num_neighbors]; 
   row_couple_ptr = MEMALLOC(num_RAPext_rows+1, index_t);
   for (p=0; p<num_neighbors; p++) {
     j = offsetInShared[p];
     i = offsetInShared[p+1];
     MPI_Irecv(&(row_couple_ptr[j]), i-j, MPI_INT, neighbor[p],
		mpi_info->msg_tag_counter+neighbor[p],
		mpi_info->comm, &mpi_requests[p]);
if (MY_DEBUG) fprintf(stderr, "rank %d RECV %d from %d tag %d\n", rank, i-j, neighbor[p], mpi_info->msg_tag_counter+neighbor[p]);
   }
   send = row_connector->send;
   for (p=0; p<send->numNeighbors; p++) {
     MPI_Issend(send_ptr[send->neighbor[p]], send_len[send->neighbor[p]], 
		MPI_INT, send->neighbor[p],
		mpi_info->msg_tag_counter+rank,
		mpi_info->comm, &mpi_requests[p+num_neighbors]);
if (MY_DEBUG) fprintf(stderr, "rank %d SEND %d TO %d tag %d\n", rank, send_len[send->neighbor[p]], send->neighbor[p], mpi_info->msg_tag_counter+rank);
   }
   MPI_Waitall(num_neighbors + send->numNeighbors,
	mpi_requests, mpi_stati); 
   mpi_info->msg_tag_counter += size; 
   TMPMEMFREE(send_len);
if (MY_DEBUG1) fprintf(stderr, "rank %d CP17\n", rank);

   sum = 0;
   for (i=0; i<num_RAPext_rows; i++) {
     k = row_couple_ptr[i];
     row_couple_ptr[i] = sum;
     sum += k;
   }
   row_couple_ptr[num_RAPext_rows] = sum;

   /* send/recv pattern->index for rowCoupleBlock */
   k = row_couple_ptr[num_RAPext_rows];
   row_couple_idx = MEMALLOC(k, index_t);
   for (p=0; p<num_neighbors; p++) {
     j1 = row_couple_ptr[offsetInShared[p]];
     j2 = row_couple_ptr[offsetInShared[p+1]];
     MPI_Irecv(&(row_couple_idx[j1]), j2-j1, MPI_INT, neighbor[p],
		mpi_info->msg_tag_counter+neighbor[p],
		mpi_info->comm, &mpi_requests[p]);
   }
   for (p=0; p<send->numNeighbors; p++) {
     MPI_Issend(send_idx[send->neighbor[p]], len[send->neighbor[p]], 
		MPI_INT, send->neighbor[p],
		mpi_info->msg_tag_counter+rank,
		mpi_info->comm, &mpi_requests[p+num_neighbors]);
   }
   MPI_Waitall(num_neighbors + send->numNeighbors, 
		mpi_requests, mpi_stati); 
   mpi_info->msg_tag_counter += size;

   offset = input_dist->first_component[rank];
   k = row_couple_ptr[num_RAPext_rows];
   for (i=0; i<k; i++) {
     row_couple_idx[i] -= offset;
   }

   for (i=0; i<size; i++) {
     TMPMEMFREE(send_ptr[i]);
     TMPMEMFREE(send_idx[i]);
   }
   TMPMEMFREE(send_ptr);
   TMPMEMFREE(send_idx);
   TMPMEMFREE(len);
   TMPMEMFREE(offsetInShared);
   TMPMEMFREE(neighbor);
   TMPMEMFREE(mpi_requests);
   TMPMEMFREE(mpi_stati);
   Esys_MPIInfo_free(mpi_info);
if (MY_DEBUG1) fprintf(stderr, "rank %d CP18\n", rank);

   /* Now, we can create pattern for mainBlock and coupleBlock */
   main_pattern = Paso_Pattern_alloc(MATRIX_FORMAT_DEFAULT, num_Pmain_cols,
			num_Pmain_cols, RAP_main_ptr, RAP_main_idx);
   col_couple_pattern = Paso_Pattern_alloc(MATRIX_FORMAT_DEFAULT, 
			num_Pmain_cols, num_RAPext_cols, 
			RAP_couple_ptr, RAP_couple_idx);
   row_couple_pattern = Paso_Pattern_alloc(MATRIX_FORMAT_DEFAULT,
			num_RAPext_rows, num_Pmain_cols,
			row_couple_ptr, row_couple_idx); 

   /* next, create the system matrix */
   pattern = Paso_SystemMatrixPattern_alloc(MATRIX_FORMAT_DEFAULT,
                output_dist, input_dist, main_pattern, col_couple_pattern,
                row_couple_pattern, col_connector, row_connector);
   out = Paso_SystemMatrix_alloc(A->type, pattern,
                row_block_size, col_block_size, FALSE);
if (MY_DEBUG) fprintf(stderr, "rank %d CP19\n", rank);

   /* finally, fill in the data*/
   memcpy(out->mainBlock->val, RAP_main_val, 
		out->mainBlock->len* sizeof(double));
   memcpy(out->col_coupleBlock->val, RAP_couple_val,
		out->col_coupleBlock->len * sizeof(double));

   /* Clean up */
   Paso_SystemMatrixPattern_free(pattern);
   Paso_Pattern_free(main_pattern);
   Paso_Pattern_free(col_couple_pattern);
   Paso_Pattern_free(row_couple_pattern);
   Paso_Connector_free(col_connector);
   Paso_Connector_free(row_connector);
   Paso_Distribution_free(output_dist);
   Paso_Distribution_free(input_dist);
   TMPMEMFREE(RAP_main_val);
   TMPMEMFREE(RAP_couple_val);
   if (Esys_noError()) {
     return out;
   } else {
     Paso_SystemMatrix_free(out);
     return NULL;
   }
}
