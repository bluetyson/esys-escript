
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

/*   Dudley: Reference elements */

/**************************************************************/

#ifndef INC_DUDLEY_REFERENCEELEMENTS
#define INC_DUDLEY_REFERENCEELEMENTS


/**************************************************************/

#include "Dudley.h"
#include "ShapeFunctions.h"
#include "Quadrature.h"

/**************************************************************/

/*     The ids of the allowed reference ellements: */

#define MAX_numNodes 64
#define MAX_numSubElements 8
#define MAX_numSides 2

typedef enum {
  Point1,
  Line2,
  Tri3,
  Tet4,
  Line2Face,
  Tri3Face,
  Tet4Face, 
  NoRef   /* marks end of list */
} ElementTypeId;

/**************************************************************/

/*  this struct holds the definition of the reference element: */

typedef struct Dudley_ReferenceElementInfo {
  ElementTypeId TypeId;                      /* the id */
  char* Name;                                /* the name in text form e.g. Line1,Rec12,... */
  dim_t numNodes;                            /* number of nodes defining the element*/
  dim_t numSubElements;                      /* number of subelements. >1 is macro elements are used. */
  dim_t numSides;							 /* specifies the number of sides the element supports. This =2 if contatact elements are used
                                                otherwise =1. */
                                                

  index_t offsets[MAX_numSides+1];			 /* offset to the side nodes: offsets[s]...offset[s+1]-1 referes to the nodes to be used for side s*/								

  
  ElementTypeId LinearTypeId;                /* id of the linear version of the element */
  
  index_t linearNodes[MAX_numNodes*MAX_numSides];         /* gives the list of nodes defining the linear or macro element */
  
  Dudley_QuadTypeId Quadrature;                /* quadrature scheme */
  Dudley_ShapeFunctionTypeId Parametrization;  /* shape function for parametrization of the element */
  Dudley_ShapeFunctionTypeId BasisFunctions;   /* shape function for the basis functions */ 

  index_t subElementNodes[MAX_numNodes*MAX_numSides*MAX_numSubElements];         /* gives the list of nodes defining the subelements:
																		subElementNodes[INDEX2(i,s,BasisFunctions->numShape*numSides)] is the i-th node in the s-th subelement.*/ 
/*********************************************************************************************************************************** */  
  dim_t numRelevantGeoNodes;                 /* number of nodes used to describe the geometry of the geometrically relevant part of the element
                                                typically this is numNodes but for 'Face' elements where the quadrature points are defined on face of the element 
						this is the number of nodes on the particular face. */
  index_t relevantGeoNodes[MAX_numNodes];    /* list to gather the geometrically relevant nodes (length used is numRelevantGeoNodes)
                                                this list is used for VTK interface */
  
  dim_t numNodesOnFace;                       /* if the element is allowed as a face element, numNodesOnFace defines the number of nodes defining the face */
                                              /* the following lists are only used for face elements defined by numNodesOnFace>0 */
  index_t faceNodes[MAX_numNodes];             /* list of the nodes defining the face */
  index_t shiftNodes[MAX_numNodes];           /* defines a permutation of the nodes which rotates the nodes on the face */
  index_t reverseNodes[MAX_numNodes];         /* reverses the order of the nodes on a face. the permutation has keep 0 fixed. */
                                              /* shiftNodes={-1} or reverseNodes={-1} are ignored. */
}  Dudley_ReferenceElementInfo;


/**************************************************************/

/*  this struct holds the realization of a reference element */

typedef struct Dudley_ReferenceElement {
	Dudley_ReferenceElementInfo* Type;     /* type of the reference element */
	Dudley_ReferenceElementInfo* LinearType;     /* type of the linear reference element */
	index_t reference_counter;	       /* reference counter */
        dim_t integrationOrder;                /* used integration order */
	dim_t numNodes;
        dim_t numLocalDim;
	dim_t numLinearNodes;
	Dudley_ShapeFunction* Parametrization;
	Dudley_ShapeFunction* BasisFunctions;
	Dudley_ShapeFunction* LinearBasisFunctions;
        double* DBasisFunctionDv;                              /* pointer to derivatives to basis function corresponding to the Parametrization quad points */
        bool_t DBasisFunctionDvShared;                /* TRUE to indicate that DBasisFunctionDv is shared with another object which is managing it */

}  Dudley_ReferenceElement;

/**************************************************************/

/*    interfaces: */

Dudley_ReferenceElement* Dudley_ReferenceElement_alloc(ElementTypeId,int);
void Dudley_ReferenceElement_dealloc(Dudley_ReferenceElement*);
ElementTypeId Dudley_ReferenceElement_getTypeId(char*);
Dudley_ReferenceElement* Dudley_ReferenceElement_reference(Dudley_ReferenceElement* in);
Dudley_ReferenceElementInfo* Dudley_ReferenceElement_getInfo(ElementTypeId id);


#define Dudley_ReferenceElement_getNumNodes(__in__) (__in__)->Type->numNodes

#endif /* #ifndef INC_DUDLEY_REFERENCEELEMENTS */
