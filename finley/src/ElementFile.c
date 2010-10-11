
/*******************************************************
*
* Copyright (c) 2003-2010 by University of Queensland
* Earth Systems Science Computational Center (ESSCC)
* http://www.uq.edu.au/esscc
*
* Primary Business: Queensland, Australia
* Licensed under the Open Software License version 3.0
* http://www.opensource.org/licenses/osl-3.0.php
*
*******************************************************/


/**************************************************************/

/*   Finley: ElementFile */

/*   allocates an element file to hold elements of type id and with integration order order. */
/*   use Finley_Mesh_allocElementTable to allocate the element table (Id,Nodes,Tag,Owner). */

/**************************************************************/

#include "ElementFile.h"

/**************************************************************/

Finley_ElementFile* Finley_ElementFile_alloc(Finley_ReferenceElementSet* referenceElementSet, Esys_MPIInfo *MPIInfo)
{
  Finley_ElementFile *out;
  
  
  if (! Finley_noError()) return NULL;
  
  /*  allocate the return value */
  
  out=MEMALLOC(1,Finley_ElementFile);
  if (Finley_checkPtr(out)) return NULL;
  out->referenceElementSet=Finley_ReferenceElementSet_reference(referenceElementSet);
  out->numElements=0;
  out->Id=NULL;
  out->Nodes=NULL;
  out->Tag=NULL;
  out->Color=NULL;
  out->minColor=0;
  out->maxColor=-1;
  out->jacobeans=NULL;
  out->jacobeans_reducedQ=NULL;
  out->jacobeans_reducedS=NULL;
  out->jacobeans_reducedS_reducedQ=NULL;

  out->Owner=NULL;                
  out->numTagsInUse=0;
  out->tagsInUse=NULL;

  out->MPIInfo = Esys_MPIInfo_getReference( MPIInfo );
 
  out->jacobeans=Finley_ElementFile_Jacobeans_alloc(referenceElementSet->referenceElement->BasisFunctions);
  out->jacobeans_reducedQ=Finley_ElementFile_Jacobeans_alloc(referenceElementSet->referenceElementReducedQuadrature->BasisFunctions);
  out->jacobeans_reducedS=Finley_ElementFile_Jacobeans_alloc(referenceElementSet->referenceElement->LinearBasisFunctions);
  out->jacobeans_reducedS_reducedQ=Finley_ElementFile_Jacobeans_alloc(referenceElementSet->referenceElementReducedQuadrature->LinearBasisFunctions);



  if (! Finley_noError()) {
     Finley_ElementFile_free(out);
     return NULL;
  }
  out->numNodes=out->referenceElementSet->numNodes;
  return out;
}

/*  deallocates an element file: */

void Finley_ElementFile_free(Finley_ElementFile* in) {
  if (in!=NULL) {
     Finley_ElementFile_freeTable(in);   
     Finley_ReferenceElementSet_dealloc(in->referenceElementSet);
     Finley_ElementFile_Jacobeans_dealloc(in->jacobeans);
     Finley_ElementFile_Jacobeans_dealloc(in->jacobeans_reducedS);
     Finley_ElementFile_Jacobeans_dealloc(in->jacobeans_reducedQ);
     Finley_ElementFile_Jacobeans_dealloc(in->jacobeans_reducedS_reducedQ);
     Esys_MPIInfo_free( in->MPIInfo );
     MEMFREE(in);      
  }
}
void Finley_ElementFile_setElementDistribution(Finley_ElementFile* in, dim_t* distribution) {
  dim_t local_num_elements,e,num_elements=0, size;
  Esys_MPI_rank myRank;
  if (in == NULL) {
      distribution[0]=num_elements;
  } else {
      if (in->MPIInfo->size>1) {
         num_elements=0;
         myRank=in->MPIInfo->rank;
         size=in->MPIInfo->size;
         #pragma omp parallel private(local_num_elements)
         {
            local_num_elements=0;
            #pragma omp for private(e)
            for (e=0;e<in->numElements;e++) {
               if (in->Owner[e] == myRank) local_num_elements++;
            }
            #pragma omp critical
            num_elements+=local_num_elements;
         }
         #ifdef ESYS_MPI
           MPI_Allgather(&num_elements,1,MPI_INT,distribution,1,MPI_INT,in->MPIInfo->comm);
         #else
           distribution[0]=num_elements;
         #endif
      } else {
        distribution[0]=in->numElements;
      }
  }
}

dim_t Finley_ElementFile_getGlobalNumElements(Finley_ElementFile* in) {
  dim_t size, *distribution=NULL, out, p;
  if (in == NULL) {
      return 0;
  } else {
    size=in->MPIInfo->size;
    distribution=TMPMEMALLOC(size,dim_t);
    Finley_ElementFile_setElementDistribution(in,distribution);
    out=0;
    for (p=0;p<size;++p) out+=distribution[p];
    TMPMEMFREE(distribution);
    return out;
  }
}
dim_t Finley_ElementFile_getMyNumElements(Finley_ElementFile* in) {
  dim_t size, *distribution=NULL, out;
  if (in == NULL) {
      return 0;
  } else {
    size=in->MPIInfo->size;
    distribution=TMPMEMALLOC(size,dim_t);
    Finley_ElementFile_setElementDistribution(in,distribution);
    out=distribution[in->MPIInfo->rank];
    TMPMEMFREE(distribution);
    return out;
  }

}
index_t Finley_ElementFile_getFirstElement(Finley_ElementFile* in){
  dim_t size, *distribution=NULL, out, p;
  if (in == NULL) {
      return 0;
  } else {
    size=in->MPIInfo->size;
    distribution=TMPMEMALLOC(size,dim_t);
    Finley_ElementFile_setElementDistribution(in,distribution);
    out=0;
    for (p=0;p<in->MPIInfo->rank;++p) out+=distribution[p];
    TMPMEMFREE(distribution);
    return out;
  }
}
