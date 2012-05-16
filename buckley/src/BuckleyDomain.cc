
/*******************************************************
*
* Copyright (c) 2011 by University of Queensland
* Earth Systems Science Computational Center (ESSCC)
* http://www.uq.edu.au/esscc
*
* Primary Business: Queensland, Australia
* Licensed under the Open Software License version 3.0
* http://www.opensource.org/licenses/osl-3.0.php
*
*******************************************************/

#include <iostream>
#include "BuckleyDomain.h"
#include "BuckleyException.h"

#include <escript/FunctionSpace.h>
#include <escript/FunctionSpaceFactory.h>

#include <pasowrap/PatternBuilder.h>
#include <pasowrap/SystemMatrixAdapter.h>

using namespace buckley;
using namespace std;
using namespace paso;


namespace
{
  // Need to put some thought into how many spaces we actually need
    const int initialspace=1;   
    const int invalidspace=-1;
    
    const int notIMPLEMENTED=-1;
    
    const int ctsfn=initialspace;
    const int discfn=2;
    const int red_discfn=-3;	// reduced discontinuous function  (reduced elements in finley terms)

    const int disc_faces=4;
    const int red_disc_faces=-7;
    
    
    const int ctsfn_DOF = -8;	// place holder for degrees of freedom form of nodes
    const int red_ctsfn = -9;   
    const int red_ctsfn_DOF = -10;	// place holder for reduced degrees of freedom for of nodes
    
    
    const unsigned int NUM_QUAD=8;	// number of quadrature points required
    const unsigned int NUM_FACEQUAD=4;

escript::DataTypes::ShapeType make3()
{
    escript::DataTypes::ShapeType s;
    s.push_back(3);
    return s;
}

escript::DataTypes::ShapeType make3x3()
{
    escript::DataTypes::ShapeType s;
    s.push_back(3);
    s.push_back(3);
    return s;
}


    const escript::DataTypes::ShapeType VECTOR3=make3();
    const escript::DataTypes::ShapeType MAT3X3=make3x3();

}


BuckleyDomain::BuckleyDomain(double x, double y, double z)
:ot(x,y,z),modified(true),leaves(0),numpts(0),samplerefids(0), m_generation(1)
{
    m_mpiInfo = Esys_MPIInfo_alloc(MPI_COMM_WORLD);  
}

BuckleyDomain::~BuckleyDomain()
{
    if (leaves)
    {
       delete[] leaves;
    }
    if (samplerefids)
    {
       delete[] samplerefids;
    }
    Esys_MPIInfo_free(m_mpiInfo);
}

bool BuckleyDomain::isValidFunctionSpaceType(int functionSpaceType) const
{
    return (functionSpaceType==ctsfn || functionSpaceType==discfn || functionSpaceType==disc_faces);
}

bool BuckleyDomain::isValidTagName(const std::string& name) const
{
    return (name=="top") || (name=="bottom") || (name=="left") || (name=="right") || (name=="front") || (name=="back");
}

int BuckleyDomain::getTag(const std::string& name) const
{
    if (name=="top") return 200;
    if (name=="bottom") return 100;
    if (name=="left") return 1;
    if (name=="right") return 2;
    if (name=="front") return 10;
    return 20;
  
  
}



escript::Data BuckleyDomain::getX() const
{
   if (modified)	// is the cached data we have about this domain stale?
   {
      processMods();
   }   
   return escript::continuousFunction(*this).getX();
}


// typedef struct 
// {
//     escript::Vector* vec;  
//     size_t chunksize;
//     unkid id;
//   
// } idstruct;
// 
// 
// void copycoords(const OctCell& c, void* v)
// {
//     idstruct* s=reinterpret_cast<idstruct*>(v);
// #ifdef OMP    
//     if (id / s->chunksize==omp_get_num_thread())
//     {
// #endif      
//         s->vec[id++]
// 
// 
// #ifdef OMP    
//     }
// #endif
// }

bool BuckleyDomain::operator==(const AbstractDomain& other) const
{
  if (dynamic_cast<const BuckleyDomain*>(&other)==0)
  {
      return false;
  }
  return this==&(other);
}

bool BuckleyDomain::operator!=(const AbstractDomain& other) const
{
  if (dynamic_cast<const BuckleyDomain*>(&other)==0)
  {
      return true;
  }
  return this!=&(other);
}


const int* BuckleyDomain::borrowSampleReferenceIDs(int functionSpaceType) const
{
  return samplerefids;
}


// this is not threadsafe due to vector sizing and push back
void BuckleyDomain::processMods() const
{
      if (leaves!=0)
      {
	  delete [] leaves;
	  delete [] samplerefids;
	  face_cells[0].resize(0);
	  face_cells[1].resize(0);
	  face_cells[2].resize(0);
	  face_cells[3].resize(0);
	  face_cells[4].resize(0);
	  face_cells[5].resize(0);
      }
      leaves=ot.process(numpts, face_cells);
      samplerefids=new int[numpts];
      for (int i=0;i<numpts;++i)
      {
	  samplerefids[i]=i;
      }
      modified=false;
}

void BuckleyDomain::setToX(escript::Data& arg) const
{
   const BuckleyDomain& argDomain=dynamic_cast<const BuckleyDomain&>(*(arg.getFunctionSpace().getDomain()));
   if (argDomain!=*this) 
      throw BuckleyException("Error - Illegal domain of data point locations");
   
   if (arg.getFunctionSpace().getTypeCode()==invalidspace)
   {
       throw BuckleyException("Error - Invalid functionspace for coordinate loading");
   }
   if (modified)	// is the cached data we have about this domain stale?
   {
      processMods();
   }
   arg.requireWrite();
   if (arg.getFunctionSpace().getTypeCode()==getContinuousFunctionCode())	// values on nodes
   {
      escript::DataTypes::ValueType::ValueType vec=(&arg.getDataPointRW(0, 0));     
      int i;	// one day OMP-3 will be default
// Why isn't this parallelised??
// well it isn't threadsafe the way it is written
// up to 8 cells could touch a point and try to write its coords
// This can be fixed but I haven't done it yet
//       #pragma omp parallel for private(i) schedule(static)
      for (i=0;i<ot.leafCount();++i)
      {
	  const OctCell& oc=*leaves[i];
	  const double dx=oc.sides[0]/2;
	  const double dy=oc.sides[1]/2;
	  const double dz=oc.sides[2]/2;
	  const double cx=oc.centre[0];
	  const double cy=oc.centre[1];
	  const double cz=oc.centre[2];
	  
	  // So this section is likely to be horribly slow
	  // Once the system is working unroll this
	  for (unsigned int j=0;j<8;++j)
	  {
	      unkid destpoint=oc.leafinfo->pmap[j];
	      if (destpoint<2)
	      {
		  continue;	// it's a hanging node so don't produce
	      }
	      destpoint-=2;
	      double x=cx;
	      double y=cy;
	      double z=cz;
	      if (j>3)
	      {
		  z+=dz;
	      }
	      else
	      {
		  z-=dz;
	      }
	      switch (j)
	      {
		case 0:
		case 3:
		case 4:
		case 7: x-=dx; break;
		  
		default:  x+=dx;
		
	      }
	      
	      switch (j)
	      {
		case 0:
		case 1:
		case 4:
		case 5: y-=dy; break;
		
		default:  y+=dy;
	      }	      
	      
	      vec[destpoint*3]=x;
	      vec[destpoint*3+1]=y;
	      vec[destpoint*3+2]=z;
	  }
	
      }
   }
   else if (arg.getFunctionSpace().getTypeCode()==getFunctionCode())	// not on the nodes
   {
      // This is actually simpler because there is no overlap between different leaves
      escript::DataTypes::ValueType::ValueType vec=(&arg.getDataPointRW(0, 0));     
      int i;	// one day OMP-3 will be default
      #pragma omp parallel for private(i) schedule(static)
      for (i=0;i<ot.leafCount();++i)
      {
	  const OctCell& oc=*leaves[i];
 
	  // So this section is likely to be horribly slow
	  // Once the system is working unroll this
	  for (unsigned int j=0;j<NUM_QUAD;++j)
	  {
	      double x, y, z;
	      oc.quadCoords(j,x,y,z);
	      vec[i*NUM_QUAD*3+3*j]=x;
	      vec[i*NUM_QUAD*3+3*j+1]=y;
	      vec[i*NUM_QUAD*3+3*j+2]=z;
	  }
	
      }      
   }
   else if (arg.getFunctionSpace().getTypeCode()==getFunctionOnBoundaryCode())	// face elements not on the nodes
   {
      // This is actually simpler because there is no overlap between different leaves
      escript::DataTypes::ValueType::ValueType vec=(&arg.getDataPointRW(0, 0));
      
//       int numfaces=face_cells[0].size()+face_cells[1].size()+face_cells[2].size()+face_cells[3].size()
// 		  +face_cells[4].size()+face_cells[5].size();
      
      int partials[6];
      partials[0]=0;
      for (int i=1;i<6;++i)
      {
	  partials[i]=partials[i-1]+face_cells[i].size();
      }
      
      double bb[6];
      ot.getBB(bb);
      
      int f, i;	// one day OMP-3 will be default
      for (f=0;f<6;++f)
      {

	  int c_tosnap;
	  double snapvalue;
	  int fps[4];
	  switch (f)
	  {
	    case 0:  c_tosnap=0;
		     snapvalue=bb[3];
		     fps[0]=1;
		     fps[1]=2;
		     fps[2]=6;
		     fps[3]=5;
		     break;
	    case 1:  c_tosnap=0;
		     snapvalue=bb[0];
		     fps[0]=3;
		     fps[1]=0;
		     fps[2]=4;
		     fps[3]=7;		     
		     break;
	    case 2:  c_tosnap=1;
		     snapvalue=bb[1];
		     fps[0]=2;
		     fps[1]=3;
		     fps[2]=7;
		     fps[3]=6;		     
		     break;
	    case 3:  c_tosnap=1;
		     snapvalue=bb[4];
		     fps[0]=0;
		     fps[1]=1;
		     fps[2]=5;
		     fps[3]=4;		     
		     break;
	    case 4:  c_tosnap=2;
		     snapvalue=bb[2];
		     fps[0]=4;
		     fps[1]=5;
		     fps[2]=6;
		     fps[3]=7;		     
		     break;
	    case 5:  c_tosnap=2;
		     snapvalue=bb[5];
		     fps[0]=3;
		     fps[1]=2;
		     fps[2]=1;
		     fps[3]=0;		     
		     break;
	  }	  
          #pragma omp parallel for private(i) schedule(static)
	  for (i=0;i<face_cells[f].size();++i)
	  {
	      double coords[3];	    
	      const OctCell& oc=*(face_cells[f][i]);
	      for (unsigned int j=0;j<4;++j)
	      {
		  oc.quadCoords(fps[j], coords[0], coords[1], coords[2]);
		  coords[c_tosnap]=snapvalue;
		  vec[(i+partials[f])*NUM_FACEQUAD*3+3*j]=coords[0];
	          vec[(i+partials[f])*NUM_FACEQUAD*3+3*j+1]=coords[1];
	          vec[(i+partials[f])*NUM_FACEQUAD*3+3*j+2]=coords[2];
	      }
	    
	  }
      } 
   }   
   else		
   {
      throw BuckleyException("Please don't use other functionspaces on this yet.");
     
   }
     
//    Dudley_Mesh* mesh=m_dudleyMesh.get();
//    // in case of values node coordinates we can do the job directly:
//    if (arg.getFunctionSpace().getTypeCode()==Nodes) {
//       escriptDataC _arg=arg.getDataC();
//       Dudley_Assemble_NodeCoordinates(mesh->Nodes,&_arg);
//    } else {
//       escript::Data tmp_data=Vector(0.0,continuousFunction(*this),true);
//       escriptDataC _tmp_data=tmp_data.getDataC();
//       Dudley_Assemble_NodeCoordinates(mesh->Nodes,&_tmp_data);
//       // this is then interpolated onto arg:
//       interpolateOnDomain(arg,tmp_data);
//    }
//    checkDudleyError();
}


void BuckleyDomain::refineAll(unsigned min_depth)
{
    if (!modified)
    {
        modified=true;
	m_generation++;
    }  
    ot.allSplit(min_depth);  
}



std::string BuckleyDomain::getDescription() const
{
    return "Buckley domain";
}

int BuckleyDomain::getContinuousFunctionCode() const
{
    return ctsfn;    
}

int BuckleyDomain::getReducedContinuousFunctionCode() const
{
    return initialspace;
}

int BuckleyDomain::getFunctionCode() const
{
    return discfn;  
}

int BuckleyDomain::getReducedFunctionCode() const
{
    return invalidspace;  
}

int BuckleyDomain::getFunctionOnBoundaryCode() const
{
    return disc_faces;  
}

int BuckleyDomain::getReducedFunctionOnBoundaryCode() const
{
    return invalidspace;  
}

int BuckleyDomain::getFunctionOnContactZeroCode() const
{
    return invalidspace;  
}

int BuckleyDomain::getReducedFunctionOnContactZeroCode() const
{
    return invalidspace;
}

int BuckleyDomain::getFunctionOnContactOneCode() const
{
    return invalidspace;
}

int BuckleyDomain::getReducedFunctionOnContactOneCode() const
{
    return invalidspace;  
}

int BuckleyDomain::getSolutionCode() const
{
    return initialspace;  
}

int BuckleyDomain::getReducedSolutionCode() const
{
    return initialspace;  
}

// hahaha - no 
int BuckleyDomain::getDiracDeltaFunctionsCode() const
{
    return invalidspace  ;
}

int BuckleyDomain::getSystemMatrixTypeId(const int solver, const int preconditioner, const int package, const bool symmetry) const
{
    return notIMPLEMENTED;  
}

int BuckleyDomain::getTransportTypeId(const int solver, const int preconditioner, const int package, const bool symmetry) const
{
    return notIMPLEMENTED;  
}

void BuckleyDomain::setToIntegrals(std::vector<double>& integrals,const escript::Data& arg) const
{
    throw BuckleyException("Not Implemented ::setToIntegrals ");
}

bool BuckleyDomain::canTag(int functionspacecode) const
{
    switch(functionspacecode) {
        case ctsfn:
        case discfn:
        case red_discfn:
        case disc_faces:
        case red_disc_faces:
            return true;
	default:
	    return false;
    }
}

int BuckleyDomain::getTagFromSampleNo(int functionSpaceType, int sampleNo) const
{
    if ((functionSpaceType==disc_faces) || (functionSpaceType==red_disc_faces))
    {
        // now we check which of the faces that sample lies on  
	// left, right, front, back, bottom, top
	if (sampleNo<0) return 0;
	if (sampleNo<face_cells[0].size()) return 1;
	if (sampleNo<face_cells[1].size()) return 2;
	if (sampleNo<face_cells[2].size()) return 10;
	if (sampleNo<face_cells[3].size()) return 20;
	if (sampleNo<face_cells[4].size()) return 100;
	if (sampleNo<face_cells[5].size()) return 200;
	return 0;
    }
    throw BuckleyException("FunctionSpace Not supported::getTagFromSampleNo");
}


// The following comment is taken from the finley version of this function
// at the moment some of these function spaces are not supported or are duplicates
// To make some comments in the python tests keep making sense [eg references to "lines" in run_escriptOnBuckley]
// I'm not re-numbering here
   /* The idea is to use equivalence classes. [Types which can be interpolated back and forth]
	class 1: DOF <-> Nodes       [Currently there is no distinction between these]
	class 2: ReducedDOF <-> ReducedNodes  [At the moment these are using class 1]
	class 3: Points				[nope]
	class 4: Elements			
	class 5: ReducedElements
	class 6: FaceElements
	class 7: ReducedFaceElements
	class 8: ContactElementZero <-> ContactElementOne		[nein, nyet, non]
	class 9: ReducedContactElementZero <-> ReducedContactElementOne [nein, nyet, non]

   There is also a set of lines. Interpolation is possible down a line but not between lines.
   class 1 and 2 belong to all lines so aren't considered.
	line 0: class 3		[No using]
	line 1: class 4,5
	line 2: class 6,7
	line 3: class 8,9	[not using]

   For classes with multiple members (eg class 2) we have vars to record if there is at least one instance.
   eg hasnodes is true if we have at least one instance of Nodes.
   */
bool BuckleyDomain::commonFunctionSpace(const std::vector<int>& fs, int& resultcode) const
{

    if (fs.empty())
    {
        return false;
    }
    vector<int> hasclass(10);
    vector<int> hasline(4);	
    bool hasnodes=false;
    bool hasrednodes=false;
    for (int i=0;i<fs.size();++i)
    {
	switch(fs[i])
	{
	case(ctsfn):   hasnodes=true;	// no break is deliberate
	case(ctsfn_DOF):
		hasclass[1]=1;
		break;
	case(red_ctsfn):    hasrednodes=true;	// no break is deliberate
	case(red_ctsfn_DOF):
		hasclass[2]=1;
		break;
	case(discfn):
		hasclass[4]=1;
		hasline[1]=1;
		break;
	case(red_discfn):
		hasclass[5]=1;
		hasline[1]=1;
		break;
	case(disc_faces):
		hasclass[6]=1;
		hasline[2]=1;
		break;
	case(red_disc_faces):
		hasclass[7]=1;
		hasline[2]=1;
		break;
	default:
		return false;
	}
    }
    int totlines=hasline[0]+hasline[1]+hasline[2]+hasline[3];
    // fail if we have more than one leaf group

    if (totlines>1)
    {
	return false;	// there are at least two branches we can't interpolate between
    }
    else if (totlines==1)
    {
	if (hasline[0]==1)		// we have points
	{
	    //resultcode=Points;
	    return false;	// should not ever happen
	}
	else if (hasline[1]==1)
	{
	    if (hasclass[5]==1)
	    {
		resultcode=red_discfn;
	    }
	    else
	    {
		resultcode=discfn;
	    }
	}
	else if (hasline[2]==1)
	{
	    if (hasclass[7]==1)
	    {
		resultcode=red_disc_faces;
	    }
	    else
	    {
		resultcode=disc_faces;
	    }
	}
	else	// so we must be in line3
	{
	    return false;	// not doing contact elements
	}
    }
    else	// totlines==0
    {
	if (hasclass[2]==1)
	{
		// something from class 2
		resultcode=(hasrednodes?red_ctsfn:red_ctsfn_DOF);
	}
	else
	{	// something from class 1
		resultcode=(hasnodes?ctsfn:ctsfn_DOF);
	}
    }
    return true;
}


#define ISEMPTY(X) (X==0 || X->isEmpty())

#if 0

void BuckleyDomain::AssemblePDE(Finley_NodeFile* nodes,Finley_ElementFile* elements,Paso_SystemMatrix* S, escriptDataC* F,
			 Data* A, Data* B, escriptDataC* C, escriptDataC* D, escriptDataC* X, escriptDataC* Y ) {

  bool_t reducedIntegrationOrder=FALSE;
  char error_msg[LenErrorMsg_MAX];
  Finley_Assemble_Parameters p;
  dim_t dimensions[ESCRIPT_MAX_DATA_RANK];
  type_t funcspace;
  double blocktimer_start = blocktimer_time();


  if (nodes==NULL || elements==NULL) return;
  if (S==NULL && ISEMPTY(F)) return;
  if ((ISEMPTY(F) && ( !ISEMPTY(X) || !ISEMPTY(Y) ) )
        throw BuckleyException("Finley_Assemble_PDE: right hand side coefficients are non-zero but no right hand side vector given.");
  }

  if (S==NULL && !ISEMPTY(A) && !ISEMPTY(B) && !ISEMPTY(C) && !ISEMPTY(D)) {
        throw BuckleyException("Finley_Assemble_PDE: coefficients are non-zero but no matrix is given.");
  }

  if (Y==0)
  {
      // this is just an implementation convienience to allow me to grab the FS from Y
      throw BuckleyException("Coefficient Y should not be NULL.");
  }
  /*  get the functionspace for this assemblage call */
  
  // note that we are not just checking the functionspace type  --- we also need to check the generation
  FunctionSpace& functionspace=Y->getFunctionSpace();	// since Y is never null  
  if ((X!=0 && X->getFunctionSpace()!=functionspace) ||
      (D!=0 && D->getFunctionSpace()!=functionspace) ||
      (C!=0 && C->getFunctionSpace()!=functionspace) ||
      (B!=0 && B->getFunctionSpace()!=functionspace) ||
      (A!=0 && A->getFunctionSpace()!=functionspace))
  {
      throw BuckleyException("All functionspaces must be the same in AssemblePDE");
  }

  if (funcspace.getTypeCode()==discfn) {
       reducedIntegrationOrder=FALSE;
  } else if (funcspace.getTypeCode()==disc_faces)  {
       reducedIntegrationOrder=FALSE;
  } else if (funcspace.getTypeCode()==red_discfn) {
       reducedIntegrationOrder=TRUE;
  } else if (funcspace.getTypeCode()==red_disc_faces)  {
       reducedIntegrationOrder=TRUE;
  } else {
       throw BuckleyException("Assemble_PDE: illegal functionspace.");
  }


//  Need to look at the parameters in more depth.
//  I was just going to port the version from finley but I borrows Jacobeans and such (which I don't generate).



  /* set all parameters in p*/
  Finley_Assemble_getAssembleParameters(nodes,elements,S,F, reducedIntegrationOrder, &p);
  if (! Finley_noError()) return;

  /* check if sample numbers are the same */

  if (! numSamplesEqual(A,p.numQuadTotal,elements->numElements) ) {
        sprintf(error_msg,"Finley_Assemble_PDE: sample points of coefficient A don't match (%d,%d)",p.numQuadTotal,elements->numElements);
        Finley_setError(TYPE_ERROR,error_msg);
  }

  if (! numSamplesEqual(B,p.numQuadTotal,elements->numElements) ) {
        sprintf(error_msg,"Finley_Assemble_PDE: sample points of coefficient B don't match (%d,%d)",p.numQuadTotal,elements->numElements);
        Finley_setError(TYPE_ERROR,error_msg);
  }

  if (! numSamplesEqual(C,p.numQuadTotal,elements->numElements) ) {
        sprintf(error_msg,"Finley_Assemble_PDE: sample points of coefficient C don't match (%d,%d)",p.numQuadTotal,elements->numElements);
        Finley_setError(TYPE_ERROR,error_msg);
  }

  if (! numSamplesEqual(D,p.numQuadTotal,elements->numElements) ) {
        sprintf(error_msg,"Finley_Assemble_PDE: sample points of coefficient D don't match (%d,%d)",p.numQuadTotal,elements->numElements);
        Finley_setError(TYPE_ERROR,error_msg);
  }

  if (! numSamplesEqual(X,p.numQuadTotal,elements->numElements) ) {
        sprintf(error_msg,"Finley_Assemble_PDE: sample points of coefficient X don't match (%d,%d)",p.numQuadTotal,elements->numElements);
        Finley_setError(TYPE_ERROR,error_msg);
  }

  if (! numSamplesEqual(Y,p.numQuadTotal,elements->numElements) ) {
        sprintf(error_msg,"Finley_Assemble_PDE: sample points of coefficient Y don't match (%d,%d)",p.numQuadTotal,elements->numElements);
        Finley_setError(TYPE_ERROR,error_msg);
  }

  /*  check the dimensions: */

  if (p.numEqu==1 && p.numComp==1) {
    if (!isEmpty(A)) {
      dimensions[0]=p.numDim;
      dimensions[1]=p.numDim;
      if (!isDataPointShapeEqual(A,2,dimensions)) {
          sprintf(error_msg,"Finley_Assemble_PDE: coefficient A: illegal shape, expected shape (%d,%d)",dimensions[0],dimensions[1]);
          Finley_setError(TYPE_ERROR,error_msg);
      }
    }
    if (!isEmpty(B)) {
       dimensions[0]=p.numDim;
       if (!isDataPointShapeEqual(B,1,dimensions)) {
          sprintf(error_msg,"Finley_Assemble_PDE: coefficient B: illegal shape (%d,)",dimensions[0]);
          Finley_setError(TYPE_ERROR,error_msg);
       }
    }
    if (!isEmpty(C)) {
       dimensions[0]=p.numDim;
       if (!isDataPointShapeEqual(C,1,dimensions)) {
          sprintf(error_msg,"Finley_Assemble_PDE: coefficient C, expected shape (%d,)",dimensions[0]);
          Finley_setError(TYPE_ERROR,error_msg);
       }
    }
    if (!isEmpty(D)) {
       if (!isDataPointShapeEqual(D,0,dimensions)) {
          Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE: coefficient D, rank 0 expected.");
       }
    }
    if (!isEmpty(X)) {
       dimensions[0]=p.numDim;
       if (!isDataPointShapeEqual(X,1,dimensions)) {
          sprintf(error_msg,"Finley_Assemble_PDE: coefficient X, expected shape (%d,",dimensions[0]);
          Finley_setError(TYPE_ERROR,error_msg);
       }
    }
    if (!isEmpty(Y)) {
       if (!isDataPointShapeEqual(Y,0,dimensions)) {
          Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE: coefficient Y, rank 0 expected.");
       }
    }
  } else {
    if (!isEmpty(A)) {
      dimensions[0]=p.numEqu;
      dimensions[1]=p.numDim;
      dimensions[2]=p.numComp;
      dimensions[3]=p.numDim;
      if (!isDataPointShapeEqual(A,4,dimensions)) {
          sprintf(error_msg,"Finley_Assemble_PDE: coefficient A, expected shape (%d,%d,%d,%d)",dimensions[0],dimensions[1],dimensions[2],dimensions[3]);
          Finley_setError(TYPE_ERROR,error_msg);
      }
    }
    if (!isEmpty(B)) {
      dimensions[0]=p.numEqu;
      dimensions[1]=p.numDim;
      dimensions[2]=p.numComp;
      if (!isDataPointShapeEqual(B,3,dimensions)) {
          sprintf(error_msg,"Finley_Assemble_PDE: coefficient B, expected shape (%d,%d,%d)",dimensions[0],dimensions[1],dimensions[2]);
          Finley_setError(TYPE_ERROR,error_msg);
      }
    }
    if (!isEmpty(C)) {
      dimensions[0]=p.numEqu;
      dimensions[1]=p.numComp;
      dimensions[2]=p.numDim;
      if (!isDataPointShapeEqual(C,3,dimensions)) {
          sprintf(error_msg,"Finley_Assemble_PDE: coefficient C, expected shape (%d,%d,%d)",dimensions[0],dimensions[1],dimensions[2]);
          Finley_setError(TYPE_ERROR,error_msg);
      }
    }
    if (!isEmpty(D)) {
      dimensions[0]=p.numEqu;
      dimensions[1]=p.numComp;
      if (!isDataPointShapeEqual(D,2,dimensions)) {
          sprintf(error_msg,"Finley_Assemble_PDE: coefficient D, expected shape (%d,%d)",dimensions[0],dimensions[1]);
          Finley_setError(TYPE_ERROR,error_msg);
      }
    }
    if (!isEmpty(X)) {
      dimensions[0]=p.numEqu;
      dimensions[1]=p.numDim;
      if (!isDataPointShapeEqual(X,2,dimensions)) {
          sprintf(error_msg,"Finley_Assemble_PDE: coefficient X, expected shape (%d,%d)",dimensions[0],dimensions[1]);
          Finley_setError(TYPE_ERROR,error_msg);
      }
    }
    if (!isEmpty(Y)) {
      dimensions[0]=p.numEqu;
      if (!isDataPointShapeEqual(Y,1,dimensions)) {
          sprintf(error_msg,"Finley_Assemble_PDE: coefficient Y, expected shape (%d,)",dimensions[0]);
          Finley_setError(TYPE_ERROR,error_msg);
      }
    }
  }
  if (Finley_noError()) {
     if (p.numEqu == p. numComp) {
        if (p.numEqu > 1) {
          /* system of PDEs */
          if (p.numDim==3) {
            if ( p.numSides == 1 ) {

	       if (funcspace==FINLEY_POINTS) {
		  if ( !isEmpty(A) || !isEmpty(B) || !isEmpty(C) || !isEmpty(X) ) {
                         Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE: Point elements require A, B, C and X to be empty.");
                  } else {
	              Finley_Assemble_PDE_Points(p, elements,S,F, D, Y);
		  }
	       } else {
                   Finley_Assemble_PDE_System2_3D(p,elements,S,F,A,B,C,D,X,Y);
	       }
	       
            } else if ( p.numSides == 2 ) {
               if ( !isEmpty(A) || !isEmpty(B) || !isEmpty(C) || !isEmpty(X) ) {
                  Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE: Contact elements require A, B, C and X to be empty.");
               } else {
                  Finley_Assemble_PDE_System2_C(p,elements,S,F,D,Y);
               }
            } else {
               Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE supports numShape=NumNodes or 2*numShape=NumNodes only.");
            }
          } else if (p.numDim==2) {
            if ( p.numSides == 1 ) {
	       if (funcspace==FINLEY_POINTS) {
		  if ( !isEmpty(A) || !isEmpty(B) || !isEmpty(C) || !isEmpty(X) ) {
                         Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE: Point elements require A, B, C and X to be empty.");
                  } else {
	              Finley_Assemble_PDE_Points(p, elements,S,F, D, Y);
		  }
	       } else {
                  Finley_Assemble_PDE_System2_2D(p,elements,S,F,A,B,C,D,X,Y);
	       }
            } else if (  p.numSides == 2 ) {
               if ( !isEmpty(A) || !isEmpty(B) || !isEmpty(C) || !isEmpty(X) ) {
                  Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE: Contact elements require A, B, C and X to be empty.");
               } else {
                  Finley_Assemble_PDE_System2_C(p,elements,S,F,D,Y);
               }
            } else {
               Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE supports numShape=NumNodes or 2*numShape=NumNodes only.");
            }
          } else if (p.numDim==1) {
            if ( p.numSides == 1  ) {
	       if (funcspace==FINLEY_POINTS) {
		  if ( !isEmpty(A) || !isEmpty(B) || !isEmpty(C) || !isEmpty(X) ) {
                         Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE: Point elements require A, B, C and X to be empty.");
                  } else {
	              Finley_Assemble_PDE_Points(p, elements,S,F, D, Y);
		  }
	       } else {
                  Finley_Assemble_PDE_System2_1D(p,elements,S,F,A,B,C,D,X,Y);
	       }
            } else if ( p.numSides == 2 ) {
               if ( !isEmpty(A) || !isEmpty(B) || !isEmpty(C) || !isEmpty(X) ) {
                  Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE: Contact elements require A, B, C and X to be empty.");
               } else {
                  Finley_Assemble_PDE_System2_C(p,elements,S,F,D,Y);
               }
            } else {
               Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE supports numShape=NumNodes or 2*numShape=NumNodes only.");
            }
          } else {
            Finley_setError(VALUE_ERROR,"Finley_Assemble_PDE supports spatial dimensions 1,2,3 only.");
          }
        } else {
          /* single PDE */
          if (p.numDim==3) {
            if ( p.numSides == 1  ) {
	       if (funcspace==FINLEY_POINTS) {
		  if ( !isEmpty(A) || !isEmpty(B) || !isEmpty(C) || !isEmpty(X) ) {
                         Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE: Point elements require A, B, C and X to be empty.");
                  } else {
	              Finley_Assemble_PDE_Points(p, elements,S,F, D, Y);
		  }
	       } else {
                   Finley_Assemble_PDE_Single2_3D(p,elements,S,F,A,B,C,D,X,Y);
	       }
            } else if ( p.numSides == 2 ) {
               if ( !isEmpty(A) || !isEmpty(B) || !isEmpty(C) || !isEmpty(X) ) {
                  Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE: Contact elements require A, B, C and X to be empty.");
               } else {
                  Finley_Assemble_PDE_Single2_C(p,elements,S,F,D,Y);
               }
            } else {
               Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE supports numShape=NumNodes or 2*numShape=NumNodes only.");
            }
          } else if (p.numDim==2) {
            if ( p.numSides == 1 ) {
	       if (funcspace==FINLEY_POINTS) {
		  if ( !isEmpty(A) || !isEmpty(B) || !isEmpty(C) || !isEmpty(X) ) {
                         Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE: Point elements require A, B, C and X to be empty.");
                  } else {
	              Finley_Assemble_PDE_Points(p, elements,S,F, D, Y);
		  }
	       } else {
                  Finley_Assemble_PDE_Single2_2D(p,elements,S,F,A,B,C,D,X,Y);
	       }
            } else if ( p.numSides == 2 ) {
               if ( !isEmpty(A) || !isEmpty(B) || !isEmpty(C) || !isEmpty(X) ) {
                  Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE: Contact elements require A, B, C and X to be empty.");
               } else {
                  Finley_Assemble_PDE_Single2_C(p,elements,S,F,D,Y);
               }
            } else {
               Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE supports numShape=NumNodes or 2*numShape=NumNodes only.");
            }
          } else if (p.numDim==1) {
            if ( p.numSides == 1 ) {
	       if (funcspace==FINLEY_POINTS) {
		  if ( !isEmpty(A) || !isEmpty(B) || !isEmpty(C) || !isEmpty(X) ) {
                         Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE: Point elements require A, B, C and X to be empty.");
                  } else {
	              Finley_Assemble_PDE_Points(p, elements,S,F, D, Y);
		  }
	       } else {
                   Finley_Assemble_PDE_Single2_1D(p,elements,S,F,A,B,C,D,X,Y);
	       }
            } else if ( p.numSides == 2  ) {
               if ( !isEmpty(A) || !isEmpty(B) || !isEmpty(C) || !isEmpty(X) ) {
                  Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE: Contact elements require A, B, C and X to be empty.");
               } else {
                  Finley_Assemble_PDE_Single2_C(p,elements,S,F,D,Y);
               }
            } else {
               Finley_setError(TYPE_ERROR,"Finley_Assemble_PDE supports numShape=NumNodes or 2*numShape=NumNodes only.");
            }
          } else {
            Finley_setError(VALUE_ERROR,"Finley_Assemble_PDE supports spatial dimensions 1,2,3 only.");
          }
        }
     } else {
          Finley_setError(VALUE_ERROR,"Finley_Assemble_PDE requires number of equations == number of solutions  .");
     }
  }
  blocktimer_increment("Finley_Assemble_PDE()", blocktimer_start);
}
#endif 
#undef ISEMPTY

void BuckleyDomain::addPDEToSystem(
                     escript::AbstractSystemMatrix& mat, escript::Data& rhs,
                     const escript::Data& A, const escript::Data& B, const escript::Data& C, 
                     const escript::Data& D, const escript::Data& X, const escript::Data& Y,
                     const escript::Data& d, const escript::Data& y,
                     const escript::Data& d_contact, const escript::Data& y_contact, 
                     const escript::Data& d_dirac, const escript::Data& y_dirac) const
{
   BuckleyDomain* smat=dynamic_cast<BuckleyDomain*>(&mat);
   if (smat==0)
   {
	throw BuckleyException("Buckley will only process its own system matrices.");
   }
   
   
   
   
   
   
/*   
   escriptDataC _rhs=rhs.getDataC();
   escriptDataC _A  =A.getDataC();
   escriptDataC _B=B.getDataC();
   escriptDataC _C=C.getDataC();
   escriptDataC _D=D.getDataC();
   escriptDataC _X=X.getDataC();
   escriptDataC _Y=Y.getDataC();
   escriptDataC _d=d.getDataC();
   escriptDataC _y=y.getDataC();
   escriptDataC _d_contact=d_contact.getDataC();
   escriptDataC _y_contact=y_contact.getDataC();
   escriptDataC _d_dirac=d_dirac.getDataC();
   escriptDataC _y_dirac=y_dirac.getDataC();*/

//   Finley_Mesh* mesh=m_finleyMesh.get();

   throw BuckleyException("Not Implemented ::addPDEToSystem");

#if 0
   
   
   Assemble_PDE(mesh->Nodes,mesh->Elements,smat->getPaso_SystemMatrix(), &_rhs, &_A, &_B, &_C, &_D, &_X, &_Y );

   Assemble_PDE(mesh->Nodes,mesh->FaceElements, smat->getPaso_SystemMatrix(), &_rhs, 0, 0, 0, &_d, 0, &_y );
#endif  
}




void BuckleyDomain::addPDEToRHS(escript::Data& rhs,
                     const escript::Data& X, const escript::Data& Y,
                     const escript::Data& y, const escript::Data& y_contact, const escript::Data& y_dirac) const
{
    throw BuckleyException("Not Implemented ::addPDEToRHS");
}

void BuckleyDomain::addPDEToTransportProblem(
                     escript::AbstractTransportProblem& tp, escript::Data& source, 
                     const escript::Data& M,
                     const escript::Data& A, const escript::Data& B, const escript::Data& C,const  escript::Data& D,
                     const  escript::Data& X,const  escript::Data& Y,
                     const escript::Data& d, const escript::Data& y,
                     const escript::Data& d_contact,const escript::Data& y_contact,
                     const escript::Data& d_dirac,const escript::Data& y_dirac) const
{
    throw BuckleyException("Not Implemented ::addPDEToTransportProblem");
}





escript::ASM_ptr BuckleyDomain::newSystemMatrix(
                      const int row_blocksize,
                      const escript::FunctionSpace& row_functionspace,
                      const int column_blocksize,
                      const escript::FunctionSpace& column_functionspace,
                      const int type) const
{
   // This setup chunk copied from finley
   int reduceRowOrder=0;
   int reduceColOrder=0;
   // is the domain right?
   const BuckleyDomain& row_domain=dynamic_cast<const BuckleyDomain&>(*(row_functionspace.getDomain()));
   if (row_domain!=*this) 
      throw BuckleyException("Error - domain of row function space does not match the domain of matrix generator.");
   const BuckleyDomain& col_domain=dynamic_cast<const BuckleyDomain&>(*(column_functionspace.getDomain()));
   if (col_domain!=*this) 
      throw BuckleyException("Error - domain of columnn function space does not match the domain of matrix generator.");
   // is the function space type right 
   
//    if (row_functionspace.getTypeCode()==DegreesOfFreedom) {
      reduceRowOrder=0;
//    } else if (row_functionspace.getTypeCode()==ReducedDegreesOfFreedom) {
//       reduceRowOrder=1;
//    } else {
//       throw FinleyAdapterException("Error - illegal function space type for system matrix rows.");
//    }
//    if (column_functionspace.getTypeCode()==DegreesOfFreedom) {
      reduceColOrder=0;
/*   } else if (column_functionspace.getTypeCode()==ReducedDegreesOfFreedom) {
      reduceColOrder=1;*/
//    } else {
//       throw FinleyAdapterException("Error - illegal function space type for system matrix columns.");
//    }
   // generate matrix:
   
   // This is just here to remind me that I need to consider multiple ranks
   const unsigned int numranks=1;
   PatternBuilder* pb=makePB( m_mpiInfo, numpts/numranks,26);
   
   index_t dummy[2];
   dummy[0]=0;
   dummy[1]=numpts;
   
   pb->setDistribution(dummy, 1, 0, false);
   pb->setDistribution(dummy, 1, 0, true);
   
   Paso_SharedComponents* send=Paso_SharedComponents_alloc( 0, 0, 0, 0, 0, 0, 0, m_mpiInfo);
   Paso_SharedComponents* recv=Paso_SharedComponents_alloc(0, 0, 0, 0, 0, 0, 0, m_mpiInfo);
   
   Paso_Connector* conn=Paso_Connector_alloc(send, recv);
   
   // again, dummy values for a sole rank
   Paso_SystemMatrixPattern* psystemMatrixPattern=pb->generatePattern(0, numpts, conn ); 
   Paso_SystemMatrix* sm=Paso_SystemMatrix_alloc(MATRIX_FORMAT_DEFAULT, psystemMatrixPattern, 1, 1, true);
   Paso_SystemMatrixPattern_free(psystemMatrixPattern);

   SystemMatrixAdapter* sma=paso::makeSystemMatrixAdapter(sm, row_blocksize, row_functionspace, column_blocksize, column_functionspace);
   
   return escript::ASM_ptr(sma);
}

escript::ATP_ptr BuckleyDomain::newTransportProblem(
                      const bool useBackwardEuler,
                      const int blocksize,
                      const escript::FunctionSpace& functionspace,
                      const int type) const
{
    throw BuckleyException("Not Implemented ::newTransportProblem");
}

int BuckleyDomain::getNumDataPointsGlobal() const
{
    throw BuckleyException("Not Implemented ::getNumDataPointsGlobal");
}


  BUCKLEY_DLL_API
std::pair<int,int> BuckleyDomain::getDataShape(int functionSpaceCode) const
{
   if (modified)	// is the cached data we have about this domain stale?
   {
      processMods();
   }  
   int count=0;
   switch (functionSpaceCode)
   {
     case ctsfn: return std::pair<int,int>(1,numpts);  
     case discfn: return std::pair<int, int>(NUM_QUAD,ot.leafCount());
     case disc_faces: 
        for (int i=0;i<6;++i)
        {
	  count+=face_cells[i].size();
	}
	return std::pair<int, int>(4, count);	
   }
   throw BuckleyException("Not Implemented ::getDataShape");  
  
}

BUCKLEY_DLL_API
void BuckleyDomain::setNewX(const escript::Data& arg)
{
    throw BuckleyException("This domain does not support changing coordinates");  
}

void BuckleyDomain::setToSize(escript::Data& out) const
{
  throw BuckleyException("Not Implemented ::setToSize");
  return;
}


BUCKLEY_DLL_API
void BuckleyDomain::Print_Mesh_Info(const bool full) const
{
    if (modified)
    {
        processMods();
    }
    std::cout << "Buckley tree with " << ot.leafCount() << " ";
    int count=0;
    for (int i=0;i<6;++i)
    {
        count+=face_cells[i].size();
    }
    std::cout << ((ot.leafCount()>1)?"leaves":"leaf") << ", "; 
    std::cout << count << " face elements and " << numpts << " unknowns\n";
    std::cout << " Generation=" << m_generation << std::endl;
}

void BuckleyDomain::refinePoint(double x, double y, double z, unsigned int desdepth)
{
    if (!modified)
    {
        modified=true;
	m_generation++;
    }
    ot.splitPoint(x, y, z, desdepth);  
}



void BuckleyDomain::collapseAll(unsigned desdepth)
{
    if (!modified)
    {
        modified=true;
	m_generation++;
    }    
    ot.allCollapse(desdepth);
}

void BuckleyDomain::collapsePoint(double x, double y, double z, unsigned int desdepth)
{
    if (!modified)
    {
        modified=true;
	m_generation++;
    }
    ot.collapsePoint(x, y, z, desdepth);  
}

unsigned BuckleyDomain::getGeneration() const
{
    return m_generation;
}


std::string BuckleyDomain::functionSpaceTypeAsString(int functionSpaceType) const
{
    switch (functionSpaceType)
    {
    case ctsfn: return "ContinuousFunction";
    case discfn: return "DiscontinuousFunction";
    case disc_faces: return "DiscontinuousFunction on Faces";
    default: return "Invalid";  
    };
}

/*
// interpolate an element from ctsfn to discfn
// qvalues and values2 must be big enough to hold a datapoint for each quadrature point
// the results end up in the values2 array
void BuckleyDomain::interpolateElementFromCtsToDisc(const LeafInfo* li, size_t ptsize, 
						    double* qvalues, double* values2) const
{
    // qvalues does double duty.
    // first it is used to hold the values of the corner points

    // we will do this with a sequence of interpolations    
    // first allong the positive x-lines [0->1], [3->2], [4->5], [6->7]
    
    for (size_t k=0;k<ptsize;++k)
    {
        values2[k]=qvalues[k]+0.25*(qvalues[ptsize+k]-qvalues[k]);	//0
        values2[ptsize+k]=qvalues[k]+0.75*(qvalues[ptsize+k]-qvalues[k]); //1
      
        values2[3*ptsize+k]=qvalues[3*ptsize+k]+0.25*(qvalues[2*ptsize+k]-qvalues[3*ptsize+k]);	//3
        values2[2*ptsize+k]=qvalues[3*ptsize+k]+0.75*(qvalues[2*ptsize+k]-qvalues[3*ptsize+k]); //2 
	
        values2[4*ptsize+k]=qvalues[4*ptsize+k]+0.25*(qvalues[5*ptsize+k]-qvalues[4*ptsize+k]);	//4
        values2[5*ptsize+k]=qvalues[4*ptsize+k]+0.75*(qvalues[5*ptsize+k]-qvalues[4*ptsize+k]); //5
      
        values2[7*ptsize+k]=qvalues[7*ptsize+k]+0.25*(qvalues[6*ptsize+k]-qvalues[7*ptsize+k]);	//7
        values2[6*ptsize+k]=qvalues[7*ptsize+k]+0.75*(qvalues[6*ptsize+k]-qvalues[7*ptsize+k]); //6     		
    }
    
    // now along the postive y-lines from values2 to qvalues
    for (size_t k=0;k<ptsize;++k)
    {
        qvalues[k]=values2[k]+0.25*(values2[3*ptsize+k]-values2[k]);	//0
        qvalues[3*ptsize+k]=values2[k]+0.75*(values2[3*ptsize+k]-values2[k]); //3
      
        qvalues[1*ptsize+k]=values2[1*ptsize+k]+0.25*(values2[2*ptsize+k]-values2[1*ptsize+k]);	//1
        qvalues[2*ptsize+k]=values2[1*ptsize+k]+0.75*(values2[2*ptsize+k]-values2[1*ptsize+k]); //2 
	
        qvalues[4*ptsize+k]=values2[4*ptsize+k]+0.25*(values2[7*ptsize+k]-values2[4*ptsize+k]);	//4
        qvalues[7*ptsize+k]=values2[4*ptsize+k]+0.75*(values2[7*ptsize+k]-values2[4*ptsize+k]); //7
      
        qvalues[5*ptsize+k]=values2[5*ptsize+k]+0.25*(values2[6*ptsize+k]-values2[5*ptsize+k]);	//5
        qvalues[6*ptsize+k]=values2[5*ptsize+k]+0.75*(values2[6*ptsize+k]-values2[5*ptsize+k]); //6     		
    }    
    
    // now the z-order
    for (size_t k=0;k<ptsize;++k)
    {
        values2[k]=qvalues[k]+0.25*(qvalues[4*ptsize+k]-qvalues[k]);	//0
        values2[4*ptsize+k]=qvalues[k]+0.75*(qvalues[4*ptsize+k]-qvalues[k]); //4
      
        values2[1*ptsize+k]=qvalues[1*ptsize+k]+0.25*(qvalues[5*ptsize+k]-qvalues[1*ptsize+k]);	//1
        values2[5*ptsize+k]=qvalues[1*ptsize+k]+0.75*(qvalues[5*ptsize+k]-qvalues[1*ptsize+k]); //5 
	
        values2[2*ptsize+k]=qvalues[2*ptsize+k]+0.25*(qvalues[6*ptsize+k]-qvalues[2*ptsize+k]);	//2
        values2[6*ptsize+k]=qvalues[2*ptsize+k]+0.75*(qvalues[6*ptsize+k]-qvalues[2*ptsize+k]); //6
      
        values2[3*ptsize+k]=qvalues[3*ptsize+k]+0.25*(qvalues[7*ptsize+k]-qvalues[3*ptsize+k]);	//3
        values2[7*ptsize+k]=qvalues[3*ptsize+k]+0.75*(qvalues[7*ptsize+k]-qvalues[3*ptsize+k]); //7     		
    }        
       
}*/



#if 0

#define GETHANGSAMPLE(LEAF, KID, VNAME) const register double* VNAME; if (li->pmap[KID]<2) {\
VNAME=buffer+buffcounter;\
if (whichchild<0) {whichchild=LEAF->whichChild();\
src1=const_cast<escript::Data&>(in).getSampleDataRO(li->pmap[whichchild]-2);}\
const double* src2=const_cast<escript::Data&>(in).getSampleDataRO(LEAF->parent->kids[KID]->leafinfo->pmap[KID]-2);\
for (int k=0;k<numComp;++k)\
{\
    buffer[buffcounter++]=(src1[k]+src2[k])/2;\
}\
} else {VNAME=in.getSampleDataRO(li->pmap[KID]-2);}

#endif


#if 0
// Code from Lutz' magic generator
void BuckleyDomain::setToGradient(escript::Data& out, const escript::Data& cIn) const
{
    escript::Data& in = *const_cast<escript::Data*>(&cIn);
    const dim_t numComp = in.getDataPointSize();
    if (modified)	// is the cached data we have about this domain stale?
    {
        processMods();
    }
    bool err=false;
#pragma omp parallel
    {
      double* buffer=new double[numComp*8];	// we will never have 8 hanging nodes
      if (out.getFunctionSpace().getTypeCode() == discfn) {


#pragma omp for
        for (index_t leaf=0; leaf < ot.leafCount(); ++leaf) {
	    const LeafInfo* li=leaves[leaf]->leafinfo;
	  
	    const double h0 = leaves[leaf]->sides[0];
	    const double h1 = leaves[leaf]->sides[1];
	    const double h2 = leaves[leaf]->sides[2];

	    /* GENERATOR SNIP_GRAD_ELEMENTS TOP */
	    const double tmp0_22 = -0.044658198738520451079/h1;
	    const double tmp0_16 = 0.16666666666666666667/h0;
	    const double tmp0_33 = -0.62200846792814621559/h1;
	    const double tmp0_0 = -0.62200846792814621559/h0;
	    const double tmp0_21 = -0.16666666666666666667/h1;
	    const double tmp0_17 = 0.62200846792814621559/h0;
	    const double tmp0_52 = -0.044658198738520451079/h2;
	    const double tmp0_1 = -0.16666666666666666667/h0;
	    const double tmp0_20 = -0.62200846792814621559/h1;
	    const double tmp0_14 = -0.044658198738520451079/h0;
	    const double tmp0_53 = -0.62200846792814621559/h2;
	    const double tmp0_49 = 0.16666666666666666667/h2;
	    const double tmp0_2 = 0.16666666666666666667/h0;
	    const double tmp0_27 = -0.044658198738520451079/h1;
	    const double tmp0_15 = -0.16666666666666666667/h0;
	    const double tmp0_50 = -0.16666666666666666667/h2;
	    const double tmp0_48 = 0.62200846792814621559/h2;
	    const double tmp0_3 = 0.044658198738520451079/h0;
	    const double tmp0_26 = -0.16666666666666666667/h1;
	    const double tmp0_12 = -0.62200846792814621559/h0;
	    const double tmp0_51 = 0.044658198738520451079/h2;
	    const double tmp0_25 = 0.62200846792814621559/h1;
	    const double tmp0_13 = 0.16666666666666666667/h0;
	    const double tmp0_56 = 0.16666666666666666667/h2;
	    const double tmp0_24 = 0.16666666666666666667/h1;
	    const double tmp0_10 = 0.62200846792814621559/h0;
	    const double tmp0_57 = 0.62200846792814621559/h2;
	    const double tmp0_11 = -0.16666666666666666667/h0;
	    const double tmp0_54 = -0.044658198738520451079/h2;
	    const double tmp0_38 = 0.16666666666666666667/h1;
	    const double tmp0_34 = -0.044658198738520451079/h1;
	    const double tmp0_42 = 0.16666666666666666667/h2;
	    const double tmp0_35 = -0.16666666666666666667/h1;
	    const double tmp0_36 = -0.62200846792814621559/h1;
	    const double tmp0_41 = 0.62200846792814621559/h2;
	    const double tmp0_8 = 0.044658198738520451079/h0;
	    const double tmp0_37 = 0.62200846792814621559/h1;
	    const double tmp0_29 = 0.16666666666666666667/h1;
	    const double tmp0_40 = -0.62200846792814621559/h2;
	    const double tmp0_9 = 0.16666666666666666667/h0;
	    const double tmp0_30 = 0.62200846792814621559/h1;
	    const double tmp0_28 = -0.16666666666666666667/h1;
	    const double tmp0_43 = 0.044658198738520451079/h2;
	    const double tmp0_32 = 0.16666666666666666667/h1;
	    const double tmp0_31 = 0.044658198738520451079/h1;
	    const double tmp0_39 = 0.044658198738520451079/h1;
	    const double tmp0_58 = -0.62200846792814621559/h2;
	    const double tmp0_55 = 0.044658198738520451079/h2;
	    const double tmp0_18 = -0.62200846792814621559/h0;
	    const double tmp0_45 = -0.16666666666666666667/h2;
	    const double tmp0_59 = -0.16666666666666666667/h2;
	    const double tmp0_4 = -0.044658198738520451079/h0;
	    const double tmp0_19 = 0.044658198738520451079/h0;
	    const double tmp0_44 = -0.044658198738520451079/h2;
	    const double tmp0_5 = 0.62200846792814621559/h0;
	    const double tmp0_47 = 0.16666666666666666667/h2;
	    const double tmp0_6 = -0.16666666666666666667/h0;
	    const double tmp0_23 = 0.044658198738520451079/h1;
	    const double tmp0_46 = -0.16666666666666666667/h2;
	    const double tmp0_7 = -0.044658198738520451079/h0;
	  
	    int buffcounter=0;
	    const double* src1=0;
	    int whichchild=-1;

	    GETHANGSAMPLE(leaves[leaf], 0, f_000)
	    GETHANGSAMPLE(leaves[leaf], 4, f_001)
	    GETHANGSAMPLE(leaves[leaf], 5, f_101)
	    GETHANGSAMPLE(leaves[leaf], 6, f_111)
	    GETHANGSAMPLE(leaves[leaf], 2, f_110)
	    GETHANGSAMPLE(leaves[leaf], 7, f_011)
	    GETHANGSAMPLE(leaves[leaf], 3, f_010)
	    GETHANGSAMPLE(leaves[leaf], 1, f_100)
//	    const register double* f_000 = (li->pmap[0]>1)?in.getSampleDataRO(li->pmap[0]-2):HANG_INTERPOLATE(leaves[leaf], 0);	    
// 	    const register double* f_001 = (li->pmap[1]>1)?in.getSampleDataRO(li->pmap[1]-2):HANG_INTERPOLATE(leaves[leaf], 1);
// 	    const register double* f_101 = (li->pmap[5]>1)?in.getSampleDataRO(li->pmap[5]-2):HANG_INTERPOLATE(leaves[leaf], 5);
// 	    const register double* f_111 = (li->pmap[6]>1)?in.getSampleDataRO(li->pmap[6]-2):HANG_INTERPOLATE(leaves[leaf], 6);
// 	    const register double* f_110 = (li->pmap[7]>1)?in.getSampleDataRO(li->pmap[7]-2):HANG_INTERPOLATE(leaves[leaf], 7);
// 	    const register double* f_011 = (li->pmap[2]>1)?in.getSampleDataRO(li->pmap[2]-2):HANG_INTERPOLATE(leaves[leaf], 2);
// 	    const register double* f_010 = (li->pmap[3]>1)?in.getSampleDataRO(li->pmap[3]-2):HANG_INTERPOLATE(leaves[leaf], 3);
// 	    const register double* f_100 = (li->pmap[4]>1)?in.getSampleDataRO(li->pmap[4]-2):HANG_INTERPOLATE(leaves[leaf], 4);

	    
	    double* o = out.getSampleDataRW(leaf);

 
	    for (index_t i=0; i < numComp; ++i) {
		o[INDEX3(i,0,0,numComp,3)] = f_000[i]*tmp0_0 + f_011[i]*tmp0_4 + f_100[i]*tmp0_5 + f_111[i]*tmp0_3 + tmp0_1*(f_001[i] + f_010[i]) + tmp0_2*(f_101[i] + f_110[i]);
		o[INDEX3(i,1,0,numComp,3)] = f_000[i]*tmp0_20 + f_010[i]*tmp0_25 + f_101[i]*tmp0_22 + f_111[i]*tmp0_23 + tmp0_21*(f_001[i] + f_100[i]) + tmp0_24*(f_011[i] + f_110[i]);
		o[INDEX3(i,2,0,numComp,3)] = f_000[i]*tmp0_40 + f_001[i]*tmp0_41 + f_110[i]*tmp0_44 + f_111[i]*tmp0_43 + tmp0_42*(f_011[i] + f_101[i]) + tmp0_45*(f_010[i] + f_100[i]);
		o[INDEX3(i,0,1,numComp,3)] = f_000[i]*tmp0_0 + f_011[i]*tmp0_4 + f_100[i]*tmp0_5 + f_111[i]*tmp0_3 + tmp0_1*(f_001[i] + f_010[i]) + tmp0_2*(f_101[i] + f_110[i]);
		o[INDEX3(i,1,1,numComp,3)] = f_000[i]*tmp0_26 + f_001[i]*tmp0_27 + f_010[i]*tmp0_32 + f_011[i]*tmp0_31 + f_100[i]*tmp0_33 + f_101[i]*tmp0_28 + f_110[i]*tmp0_30 + f_111[i]*tmp0_29;
		o[INDEX3(i,2,1,numComp,3)] = f_000[i]*tmp0_46 + f_001[i]*tmp0_47 + f_010[i]*tmp0_52 + f_011[i]*tmp0_51 + f_100[i]*tmp0_53 + f_101[i]*tmp0_48 + f_110[i]*tmp0_50 + f_111[i]*tmp0_49;
		o[INDEX3(i,0,2,numComp,3)] = f_000[i]*tmp0_6 + f_001[i]*tmp0_7 + f_010[i]*tmp0_12 + f_011[i]*tmp0_11 + f_100[i]*tmp0_13 + f_101[i]*tmp0_8 + f_110[i]*tmp0_10 + f_111[i]*tmp0_9;
		o[INDEX3(i,1,2,numComp,3)] = f_000[i]*tmp0_20 + f_010[i]*tmp0_25 + f_101[i]*tmp0_22 + f_111[i]*tmp0_23 + tmp0_21*(f_001[i] + f_100[i]) + tmp0_24*(f_011[i] + f_110[i]);
		o[INDEX3(i,2,2,numComp,3)] = f_000[i]*tmp0_46 + f_001[i]*tmp0_47 + f_010[i]*tmp0_53 + f_011[i]*tmp0_48 + f_100[i]*tmp0_52 + f_101[i]*tmp0_51 + f_110[i]*tmp0_50 + f_111[i]*tmp0_49;
		o[INDEX3(i,0,3,numComp,3)] = f_000[i]*tmp0_6 + f_001[i]*tmp0_7 + f_010[i]*tmp0_12 + f_011[i]*tmp0_11 + f_100[i]*tmp0_13 + f_101[i]*tmp0_8 + f_110[i]*tmp0_10 + f_111[i]*tmp0_9;
		o[INDEX3(i,1,3,numComp,3)] = f_000[i]*tmp0_26 + f_001[i]*tmp0_27 + f_010[i]*tmp0_32 + f_011[i]*tmp0_31 + f_100[i]*tmp0_33 + f_101[i]*tmp0_28 + f_110[i]*tmp0_30 + f_111[i]*tmp0_29;
		o[INDEX3(i,2,3,numComp,3)] = f_000[i]*tmp0_54 + f_001[i]*tmp0_55 + f_110[i]*tmp0_58 + f_111[i]*tmp0_57 + tmp0_56*(f_011[i] + f_101[i]) + tmp0_59*(f_010[i] + f_100[i]);
		o[INDEX3(i,0,4,numComp,3)] = f_000[i]*tmp0_6 + f_001[i]*tmp0_12 + f_010[i]*tmp0_7 + f_011[i]*tmp0_11 + f_100[i]*tmp0_13 + f_101[i]*tmp0_10 + f_110[i]*tmp0_8 + f_111[i]*tmp0_9;
		o[INDEX3(i,1,4,numComp,3)] = f_000[i]*tmp0_26 + f_001[i]*tmp0_33 + f_010[i]*tmp0_32 + f_011[i]*tmp0_30 + f_100[i]*tmp0_27 + f_101[i]*tmp0_28 + f_110[i]*tmp0_31 + f_111[i]*tmp0_29;
		o[INDEX3(i,2,4,numComp,3)] = f_000[i]*tmp0_40 + f_001[i]*tmp0_41 + f_110[i]*tmp0_44 + f_111[i]*tmp0_43 + tmp0_42*(f_011[i] + f_101[i]) + tmp0_45*(f_010[i] + f_100[i]);
		o[INDEX3(i,0,5,numComp,3)] = f_000[i]*tmp0_6 + f_001[i]*tmp0_12 + f_010[i]*tmp0_7 + f_011[i]*tmp0_11 + f_100[i]*tmp0_13 + f_101[i]*tmp0_10 + f_110[i]*tmp0_8 + f_111[i]*tmp0_9;
		o[INDEX3(i,1,5,numComp,3)] = f_000[i]*tmp0_34 + f_010[i]*tmp0_39 + f_101[i]*tmp0_36 + f_111[i]*tmp0_37 + tmp0_35*(f_001[i] + f_100[i]) + tmp0_38*(f_011[i] + f_110[i]);
		o[INDEX3(i,2,5,numComp,3)] = f_000[i]*tmp0_46 + f_001[i]*tmp0_47 + f_010[i]*tmp0_52 + f_011[i]*tmp0_51 + f_100[i]*tmp0_53 + f_101[i]*tmp0_48 + f_110[i]*tmp0_50 + f_111[i]*tmp0_49;
		o[INDEX3(i,0,6,numComp,3)] = f_000[i]*tmp0_14 + f_011[i]*tmp0_18 + f_100[i]*tmp0_19 + f_111[i]*tmp0_17 + tmp0_15*(f_001[i] + f_010[i]) + tmp0_16*(f_101[i] + f_110[i]);
		o[INDEX3(i,1,6,numComp,3)] = f_000[i]*tmp0_26 + f_001[i]*tmp0_33 + f_010[i]*tmp0_32 + f_011[i]*tmp0_30 + f_100[i]*tmp0_27 + f_101[i]*tmp0_28 + f_110[i]*tmp0_31 + f_111[i]*tmp0_29;
		o[INDEX3(i,2,6,numComp,3)] = f_000[i]*tmp0_46 + f_001[i]*tmp0_47 + f_010[i]*tmp0_53 + f_011[i]*tmp0_48 + f_100[i]*tmp0_52 + f_101[i]*tmp0_51 + f_110[i]*tmp0_50 + f_111[i]*tmp0_49;
		o[INDEX3(i,0,7,numComp,3)] = f_000[i]*tmp0_14 + f_011[i]*tmp0_18 + f_100[i]*tmp0_19 + f_111[i]*tmp0_17 + tmp0_15*(f_001[i] + f_010[i]) + tmp0_16*(f_101[i] + f_110[i]);
		o[INDEX3(i,1,7,numComp,3)] = f_000[i]*tmp0_34 + f_010[i]*tmp0_39 + f_101[i]*tmp0_36 + f_111[i]*tmp0_37 + tmp0_35*(f_001[i] + f_100[i]) + tmp0_38*(f_011[i] + f_110[i]);
		o[INDEX3(i,2,7,numComp,3)] = f_000[i]*tmp0_54 + f_001[i]*tmp0_55 + f_110[i]*tmp0_58 + f_111[i]*tmp0_57 + tmp0_56*(f_011[i] + f_101[i]) + tmp0_59*(f_010[i] + f_100[i]);
	    } /* end of component loop i */
        } /* end of leaf loop */
        /* GENERATOR SNIP_GRAD_ELEMENTS BOTTOM */
    } else if (out.getFunctionSpace().getTypeCode() == red_discfn) {
        /* GENERATOR SNIP_GRAD_REDUCED_ELEMENTS TOP */
#pragma omp for
        for (index_t leaf=0; leaf < ot.leafCount(); ++leaf) {
	    const LeafInfo* li=leaves[leaf]->leafinfo;
	  
	    const double h0 = leaves[leaf]->sides[0];
	    const double h1 = leaves[leaf]->sides[1];
	    const double h2 = leaves[leaf]->sides[2];
	    
	    const double tmp0_0 = -0.25/h0;
	    const double tmp0_4 = -0.25/h2;
	    const double tmp0_1 = 0.25/h0;
	    const double tmp0_5 = 0.25/h2;
	    const double tmp0_2 = -0.25/h1;
	    const double tmp0_3 = 0.25/h1;

	    int buffcounter=0;
	    const double* src1=0;
	    int whichchild=-1;	    
	    GETHANGSAMPLE(leaves[leaf], 0, f_000)
	    GETHANGSAMPLE(leaves[leaf], 4, f_001)
	    GETHANGSAMPLE(leaves[leaf], 5, f_101)
	    GETHANGSAMPLE(leaves[leaf], 6, f_111)
	    GETHANGSAMPLE(leaves[leaf], 2, f_110)
	    GETHANGSAMPLE(leaves[leaf], 7, f_011)
	    GETHANGSAMPLE(leaves[leaf], 3, f_010)
	    GETHANGSAMPLE(leaves[leaf], 1, f_100)

	    
// 	    const register double* f_000 = (li->pmap[0]>1)?in.getSampleDataRO(li->pmap[0]-2):HANG_INTERPOLATE(leaves[leaf], 0);
// 	    const register double* f_001 = (li->pmap[1]>1)?in.getSampleDataRO(li->pmap[1]-2):HANG_INTERPOLATE(leaves[leaf], 1);
// 	    const register double* f_101 = (li->pmap[5]>1)?in.getSampleDataRO(li->pmap[5]-2):HANG_INTERPOLATE(leaves[leaf], 5);
// 	    const register double* f_111 = (li->pmap[6]>1)?in.getSampleDataRO(li->pmap[6]-2):HANG_INTERPOLATE(leaves[leaf], 6);
// 	    const register double* f_110 = (li->pmap[7]>1)?in.getSampleDataRO(li->pmap[7]-2):HANG_INTERPOLATE(leaves[leaf], 7);
// 	    const register double* f_011 = (li->pmap[2]>1)?in.getSampleDataRO(li->pmap[2]-2):HANG_INTERPOLATE(leaves[leaf], 2);
// 	    const register double* f_010 = (li->pmap[3]>1)?in.getSampleDataRO(li->pmap[3]-2):HANG_INTERPOLATE(leaves[leaf], 3);
// 	    const register double* f_100 = (li->pmap[4]>1)?in.getSampleDataRO(li->pmap[4]-2):HANG_INTERPOLATE(leaves[leaf], 4);	    

            double* o = out.getSampleDataRW(leaf);
	    for (index_t i=0; i < numComp; ++i) {
		o[INDEX3(i,0,0,numComp,3)] = tmp0_0*(f_000[i] + f_001[i] + f_010[i] + f_011[i]) + tmp0_1*(f_100[i] + f_101[i] + f_110[i] + f_111[i]);
		o[INDEX3(i,1,0,numComp,3)] = tmp0_2*(f_000[i] + f_001[i] + f_100[i] + f_101[i]) + tmp0_3*(f_010[i] + f_011[i] + f_110[i] + f_111[i]);
		o[INDEX3(i,2,0,numComp,3)] = tmp0_4*(f_000[i] + f_010[i] + f_100[i] + f_110[i]) + tmp0_5*(f_001[i] + f_011[i] + f_101[i] + f_111[i]);
	    } /* end of component loop i */
        } /* end of leaf loop */
        /* GENERATOR SNIP_GRAD_REDUCED_ELEMENTS BOTTOM */
    } else if (out.getFunctionSpace().getTypeCode() == disc_faces) {
        /* GENERATOR SNIP_GRAD_FACES TOP */
        if (face_cells[0].size() > 0) {		// left face

#pragma omp for
          for (index_t leaf=0; leaf < face_cells[0].size(); ++leaf) {
	    const LeafInfo* li=face_cells[0][leaf]->leafinfo;		// we aren't iterating over all leaves this time
	  
	    const double h0 = face_cells[0][leaf]->sides[0];
	    const double h1 = face_cells[0][leaf]->sides[1];
	    const double h2 = face_cells[0][leaf]->sides[2];

            const double tmp0_22 = 0.21132486540518711775/h1;
            const double tmp0_16 = 0.16666666666666666667/h0;
            const double tmp0_33 = 0.21132486540518711775/h2;
            const double tmp0_0 = -0.62200846792814621559/h0;
            const double tmp0_21 = -0.21132486540518711775/h1;
            const double tmp0_17 = 0.62200846792814621559/h0;
            const double tmp0_1 = -0.16666666666666666667/h0;
            const double tmp0_20 = -0.78867513459481288225/h1;
            const double tmp0_14 = -0.044658198738520451079/h0;
            const double tmp0_2 = 0.16666666666666666667/h0;
            const double tmp0_27 = 0.21132486540518711775/h1;
            const double tmp0_15 = -0.16666666666666666667/h0;
            const double tmp0_3 = 0.044658198738520451079/h0;
            const double tmp0_26 = 0.78867513459481288225/h1;
            const double tmp0_12 = -0.62200846792814621559/h0;
            const double tmp0_25 = -0.78867513459481288225/h1;
            const double tmp0_13 = 0.16666666666666666667/h0;
            const double tmp0_24 = -0.21132486540518711775/h1;
            const double tmp0_10 = 0.62200846792814621559/h0;
            const double tmp0_11 = -0.16666666666666666667/h0;
            const double tmp0_34 = 0.78867513459481288225/h2;
            const double tmp0_35 = -0.78867513459481288225/h2;
            const double tmp0_8 = 0.044658198738520451079/h0;
            const double tmp0_29 = 0.78867513459481288225/h2;
            const double tmp0_9 = 0.16666666666666666667/h0;
            const double tmp0_30 = 0.21132486540518711775/h2;
            const double tmp0_28 = -0.78867513459481288225/h2;
            const double tmp0_32 = -0.21132486540518711775/h2;
            const double tmp0_31 = -0.21132486540518711775/h2;
            const double tmp0_18 = -0.62200846792814621559/h0;
            const double tmp0_4 = -0.044658198738520451079/h0;
            const double tmp0_19 = 0.044658198738520451079/h0;
            const double tmp0_5 = 0.62200846792814621559/h0;
            const double tmp0_6 = -0.16666666666666666667/h0;
            const double tmp0_23 = 0.78867513459481288225/h1;
            const double tmp0_7 = -0.044658198738520451079/h0;	  
	    
	    int buffcounter=0;
	    const double* src1=0;
	    int whichchild=-1;	    
	    
	    GETHANGSAMPLE(face_cells[0][leaf], 0, f_000)
	    GETHANGSAMPLE(face_cells[0][leaf], 4, f_001)
	    GETHANGSAMPLE(face_cells[0][leaf], 5, f_101)
	    GETHANGSAMPLE(face_cells[0][leaf], 6, f_111)
	    GETHANGSAMPLE(face_cells[0][leaf], 2, f_110)
	    GETHANGSAMPLE(face_cells[0][leaf], 7, f_011)
	    GETHANGSAMPLE(face_cells[0][leaf], 3, f_010)
	    GETHANGSAMPLE(face_cells[0][leaf], 1, f_100)

/*	    const register double* f_000 = (li->pmap[0]>1)?in.getSampleDataRO(li->pmap[0]-2):HANG_INTERPOLATE(face_cells[0][leaf], 0);
	    const register double* f_001 = (li->pmap[1]>1)?in.getSampleDataRO(li->pmap[1]-2):HANG_INTERPOLATE(face_cells[0][leaf], 1);
	    const register double* f_101 = (li->pmap[5]>1)?in.getSampleDataRO(li->pmap[5]-2):HANG_INTERPOLATE(face_cells[0][leaf], 5);
	    const register double* f_111 = (li->pmap[6]>1)?in.getSampleDataRO(li->pmap[6]-2):HANG_INTERPOLATE(face_cells[0][leaf], 6);
	    const register double* f_110 = (li->pmap[7]>1)?in.getSampleDataRO(li->pmap[7]-2):HANG_INTERPOLATE(face_cells[0][leaf], 7);
	    const register double* f_011 = (li->pmap[2]>1)?in.getSampleDataRO(li->pmap[2]-2):HANG_INTERPOLATE(face_cells[0][leaf], 2);
	    const register double* f_010 = (li->pmap[3]>1)?in.getSampleDataRO(li->pmap[3]-2):HANG_INTERPOLATE(face_cells[0][leaf], 3);
	    const register double* f_100 = (li->pmap[4]>1)?in.getSampleDataRO(li->pmap[4]-2):HANG_INTERPOLATE(face_cells[0][leaf], 4);	  	*/    
	    

            double* o = out.getSampleDataRW(leaf);	// face 0 start at the beginning of the object
	    for (index_t i=0; i < numComp; ++i) {
		o[INDEX3(i,0,0,numComp,3)] = f_000[i]*tmp0_0 + f_011[i]*tmp0_4 + f_100[i]*tmp0_5 + f_111[i]*tmp0_3 + tmp0_1*(f_001[i] + f_010[i]) + tmp0_2*(f_101[i] + f_110[i]);
		o[INDEX3(i,1,0,numComp,3)] = f_000[i]*tmp0_20 + f_001[i]*tmp0_21 + f_010[i]*tmp0_23 + f_011[i]*tmp0_22;
		o[INDEX3(i,2,0,numComp,3)] = f_000[i]*tmp0_28 + f_001[i]*tmp0_29 + f_010[i]*tmp0_31 + f_011[i]*tmp0_30;
		o[INDEX3(i,0,1,numComp,3)] = f_000[i]*tmp0_6 + f_001[i]*tmp0_7 + f_010[i]*tmp0_12 + f_011[i]*tmp0_11 + f_100[i]*tmp0_13 + f_101[i]*tmp0_8 + f_110[i]*tmp0_10 + f_111[i]*tmp0_9;
		o[INDEX3(i,1,1,numComp,3)] = f_000[i]*tmp0_20 + f_001[i]*tmp0_21 + f_010[i]*tmp0_23 + f_011[i]*tmp0_22;
		o[INDEX3(i,2,1,numComp,3)] = f_000[i]*tmp0_32 + f_001[i]*tmp0_33 + f_010[i]*tmp0_35 + f_011[i]*tmp0_34;
		o[INDEX3(i,0,2,numComp,3)] = f_000[i]*tmp0_6 + f_001[i]*tmp0_12 + f_010[i]*tmp0_7 + f_011[i]*tmp0_11 + f_100[i]*tmp0_13 + f_101[i]*tmp0_10 + f_110[i]*tmp0_8 + f_111[i]*tmp0_9;
		o[INDEX3(i,1,2,numComp,3)] = f_000[i]*tmp0_24 + f_001[i]*tmp0_25 + f_010[i]*tmp0_27 + f_011[i]*tmp0_26;
		o[INDEX3(i,2,2,numComp,3)] = f_000[i]*tmp0_28 + f_001[i]*tmp0_29 + f_010[i]*tmp0_31 + f_011[i]*tmp0_30;
		o[INDEX3(i,0,3,numComp,3)] = f_000[i]*tmp0_14 + f_011[i]*tmp0_18 + f_100[i]*tmp0_19 + f_111[i]*tmp0_17 + tmp0_15*(f_001[i] + f_010[i]) + tmp0_16*(f_101[i] + f_110[i]);
		o[INDEX3(i,1,3,numComp,3)] = f_000[i]*tmp0_24 + f_001[i]*tmp0_25 + f_010[i]*tmp0_27 + f_011[i]*tmp0_26;
		o[INDEX3(i,2,3,numComp,3)] = f_000[i]*tmp0_32 + f_001[i]*tmp0_33 + f_010[i]*tmp0_35 + f_011[i]*tmp0_34;
	    } /* end of component loop i */
	  }    
        } /* end of face 0 */
        int baseoffset=face_cells[0].size();
        if (face_cells[1].size() > 0) {
#pragma omp for
          for (index_t leaf=0; leaf < face_cells[1].size(); ++leaf) {	  
	    const LeafInfo* li=face_cells[1][leaf]->leafinfo;		// we aren't iterating over all leaves this time
	  
	    const double h0 = face_cells[1][leaf]->sides[0];
	    const double h1 = face_cells[1][leaf]->sides[1];
	    const double h2 = face_cells[1][leaf]->sides[2];
	    
            const double tmp0_22 = 0.78867513459481288225/h1;
            const double tmp0_16 = 0.16666666666666666667/h0;
            const double tmp0_33 = 0.78867513459481288225/h2;
            const double tmp0_0 = -0.62200846792814621559/h0;
            const double tmp0_21 = 0.21132486540518711775/h1;
            const double tmp0_17 = 0.62200846792814621559/h0;
            const double tmp0_1 = -0.16666666666666666667/h0;
            const double tmp0_20 = -0.21132486540518711775/h1;
            const double tmp0_14 = -0.044658198738520451079/h0;
            const double tmp0_2 = 0.16666666666666666667/h0;
            const double tmp0_27 = -0.21132486540518711775/h1;
            const double tmp0_15 = -0.16666666666666666667/h0;
            const double tmp0_3 = 0.044658198738520451079/h0;
            const double tmp0_26 = 0.21132486540518711775/h1;
            const double tmp0_12 = -0.62200846792814621559/h0;
            const double tmp0_25 = 0.78867513459481288225/h1;
            const double tmp0_13 = 0.16666666666666666667/h0;
            const double tmp0_24 = -0.78867513459481288225/h1;
            const double tmp0_10 = 0.62200846792814621559/h0;
            const double tmp0_11 = -0.16666666666666666667/h0;
            const double tmp0_34 = -0.78867513459481288225/h2;
            const double tmp0_35 = -0.21132486540518711775/h2;
            const double tmp0_8 = 0.044658198738520451079/h0;
            const double tmp0_29 = 0.21132486540518711775/h2;
            const double tmp0_9 = 0.16666666666666666667/h0;
            const double tmp0_30 = -0.21132486540518711775/h2;
            const double tmp0_28 = 0.78867513459481288225/h2;
            const double tmp0_32 = 0.21132486540518711775/h2;
            const double tmp0_31 = -0.78867513459481288225/h2;
            const double tmp0_18 = -0.62200846792814621559/h0;
            const double tmp0_4 = -0.044658198738520451079/h0;
            const double tmp0_19 = 0.044658198738520451079/h0;
            const double tmp0_5 = 0.62200846792814621559/h0;
            const double tmp0_6 = -0.16666666666666666667/h0;
            const double tmp0_23 = -0.78867513459481288225/h1;
            const double tmp0_7 = -0.044658198738520451079/h0;

	    int buffcounter=0;
	    const double* src1=0;
	    int whichchild=-1;	    

	    GETHANGSAMPLE(face_cells[1][leaf], 0, f_000)
	    GETHANGSAMPLE(face_cells[1][leaf], 4, f_001)
	    GETHANGSAMPLE(face_cells[1][leaf], 5, f_101)
	    GETHANGSAMPLE(face_cells[1][leaf], 6, f_111)
	    GETHANGSAMPLE(face_cells[1][leaf], 2, f_110)
	    GETHANGSAMPLE(face_cells[1][leaf], 7, f_011)
	    GETHANGSAMPLE(face_cells[1][leaf], 3, f_010)
	    GETHANGSAMPLE(face_cells[1][leaf], 1, f_100)	    
	    
	    
/*	    const register double* f_000 = (li->pmap[0]>1)?in.getSampleDataRO(li->pmap[0]-2):HANG_INTERPOLATE(face_cells[1][leaf], 0);
	    const register double* f_001 = (li->pmap[1]>1)?in.getSampleDataRO(li->pmap[1]-2):HANG_INTERPOLATE(face_cells[1][leaf], 1);
	    const register double* f_101 = (li->pmap[5]>1)?in.getSampleDataRO(li->pmap[5]-2):HANG_INTERPOLATE(face_cells[1][leaf], 5);
	    const register double* f_111 = (li->pmap[6]>1)?in.getSampleDataRO(li->pmap[6]-2):HANG_INTERPOLATE(face_cells[1][leaf], 6);
	    const register double* f_110 = (li->pmap[7]>1)?in.getSampleDataRO(li->pmap[7]-2):HANG_INTERPOLATE(face_cells[1][leaf], 7);
	    const register double* f_011 = (li->pmap[2]>1)?in.getSampleDataRO(li->pmap[2]-2):HANG_INTERPOLATE(face_cells[1][leaf], 2);
	    const register double* f_010 = (li->pmap[3]>1)?in.getSampleDataRO(li->pmap[3]-2):HANG_INTERPOLATE(face_cells[1][leaf], 3);
	    const register double* f_100 = (li->pmap[4]>1)?in.getSampleDataRO(li->pmap[4]-2):HANG_INTERPOLATE(face_cells[1][leaf], 4);	*/    

            double* o = out.getSampleDataRW(baseoffset+leaf);
	    for (index_t i=0; i < numComp; ++i) {
		o[INDEX3(i,0,0,numComp,3)] = f_000[i]*tmp0_0 + f_011[i]*tmp0_4 + f_100[i]*tmp0_5 + f_111[i]*tmp0_3 + tmp0_1*(f_001[i] + f_010[i]) + tmp0_2*(f_101[i] + f_110[i]);
		o[INDEX3(i,1,0,numComp,3)] = f_100[i]*tmp0_23 + f_101[i]*tmp0_20 + f_110[i]*tmp0_22 + f_111[i]*tmp0_21;
		o[INDEX3(i,2,0,numComp,3)] = f_100[i]*tmp0_31 + f_101[i]*tmp0_28 + f_110[i]*tmp0_30 + f_111[i]*tmp0_29;
		o[INDEX3(i,0,1,numComp,3)] = f_000[i]*tmp0_6 + f_001[i]*tmp0_7 + f_010[i]*tmp0_12 + f_011[i]*tmp0_11 + f_100[i]*tmp0_13 + f_101[i]*tmp0_8 + f_110[i]*tmp0_10 + f_111[i]*tmp0_9;
		o[INDEX3(i,1,1,numComp,3)] = f_100[i]*tmp0_23 + f_101[i]*tmp0_20 + f_110[i]*tmp0_22 + f_111[i]*tmp0_21;
		o[INDEX3(i,2,1,numComp,3)] = f_100[i]*tmp0_35 + f_101[i]*tmp0_32 + f_110[i]*tmp0_34 + f_111[i]*tmp0_33;
		o[INDEX3(i,0,2,numComp,3)] = f_000[i]*tmp0_6 + f_001[i]*tmp0_12 + f_010[i]*tmp0_7 + f_011[i]*tmp0_11 + f_100[i]*tmp0_13 + f_101[i]*tmp0_10 + f_110[i]*tmp0_8 + f_111[i]*tmp0_9;
		o[INDEX3(i,1,2,numComp,3)] = f_100[i]*tmp0_27 + f_101[i]*tmp0_24 + f_110[i]*tmp0_26 + f_111[i]*tmp0_25;
		o[INDEX3(i,2,2,numComp,3)] = f_100[i]*tmp0_31 + f_101[i]*tmp0_28 + f_110[i]*tmp0_30 + f_111[i]*tmp0_29;
		o[INDEX3(i,0,3,numComp,3)] = f_000[i]*tmp0_14 + f_011[i]*tmp0_18 + f_100[i]*tmp0_19 + f_111[i]*tmp0_17 + tmp0_15*(f_001[i] + f_010[i]) + tmp0_16*(f_101[i] + f_110[i]);
		o[INDEX3(i,1,3,numComp,3)] = f_100[i]*tmp0_27 + f_101[i]*tmp0_24 + f_110[i]*tmp0_26 + f_111[i]*tmp0_25;
		o[INDEX3(i,2,3,numComp,3)] = f_100[i]*tmp0_35 + f_101[i]*tmp0_32 + f_110[i]*tmp0_34 + f_111[i]*tmp0_33;
	    } /* end of component loop i */
	  }    
        } /* end of face 1 */
        baseoffset+=face_cells[1].size();
        if (face_cells[2].size() > 0) {
#pragma omp for
          for (index_t leaf=0; leaf < face_cells[2].size(); ++leaf) {	  
	    const LeafInfo* li=face_cells[2][leaf]->leafinfo;		// we aren't iterating over all leaves this time
	    
	    const double h0 = face_cells[2][leaf]->sides[0];
	    const double h1 = face_cells[2][leaf]->sides[1];
	    const double h2 = face_cells[2][leaf]->sides[2];	    
	    
            const double tmp0_22 = -0.044658198738520451079/h1;
            const double tmp0_16 = -0.16666666666666666667/h1;
            const double tmp0_33 = 0.21132486540518711775/h2;
            const double tmp0_0 = -0.78867513459481288225/h0;
            const double tmp0_21 = 0.16666666666666666667/h1;
            const double tmp0_17 = -0.62200846792814621559/h1;
            const double tmp0_1 = -0.21132486540518711775/h0;
            const double tmp0_20 = 0.044658198738520451079/h1;
            const double tmp0_14 = -0.16666666666666666667/h1;
            const double tmp0_2 = 0.21132486540518711775/h0;
            const double tmp0_27 = 0.044658198738520451079/h1;
            const double tmp0_15 = -0.044658198738520451079/h1;
            const double tmp0_3 = 0.78867513459481288225/h0;
            const double tmp0_26 = 0.16666666666666666667/h1;
            const double tmp0_12 = 0.16666666666666666667/h1;
            const double tmp0_25 = 0.62200846792814621559/h1;
            const double tmp0_13 = 0.62200846792814621559/h1;
            const double tmp0_24 = -0.62200846792814621559/h1;
            const double tmp0_10 = -0.044658198738520451079/h1;
            const double tmp0_11 = 0.044658198738520451079/h1;
            const double tmp0_34 = 0.78867513459481288225/h2;
            const double tmp0_35 = -0.78867513459481288225/h2;
            const double tmp0_8 = -0.62200846792814621559/h1;
            const double tmp0_29 = 0.78867513459481288225/h2;
            const double tmp0_9 = -0.16666666666666666667/h1;
            const double tmp0_30 = 0.21132486540518711775/h2;
            const double tmp0_28 = -0.78867513459481288225/h2;
            const double tmp0_32 = -0.21132486540518711775/h2;
            const double tmp0_31 = -0.21132486540518711775/h2;
            const double tmp0_18 = 0.16666666666666666667/h1;
            const double tmp0_4 = -0.21132486540518711775/h0;
            const double tmp0_19 = 0.62200846792814621559/h1;
            const double tmp0_5 = -0.78867513459481288225/h0;
            const double tmp0_6 = 0.78867513459481288225/h0;
            const double tmp0_23 = -0.16666666666666666667/h1;
            const double tmp0_7 = 0.21132486540518711775/h0;
	    
	    int buffcounter=0;
	    const double* src1=0;
	    int whichchild=-1;	    

	    GETHANGSAMPLE(face_cells[2][leaf], 0, f_000)
	    GETHANGSAMPLE(face_cells[2][leaf], 4, f_001)
	    GETHANGSAMPLE(face_cells[2][leaf], 5, f_101)
	    GETHANGSAMPLE(face_cells[2][leaf], 6, f_111)
	    GETHANGSAMPLE(face_cells[2][leaf], 2, f_110)
	    GETHANGSAMPLE(face_cells[2][leaf], 7, f_011)
	    GETHANGSAMPLE(face_cells[2][leaf], 3, f_010)
	    GETHANGSAMPLE(face_cells[2][leaf], 1, f_100)	    
	    
	    
/*	    const register double* f_000 = (li->pmap[0]>1)?in.getSampleDataRO(li->pmap[0]-2):HANG_INTERPOLATE(face_cells[2][leaf], 0);
	    const register double* f_001 = (li->pmap[1]>1)?in.getSampleDataRO(li->pmap[1]-2):HANG_INTERPOLATE(face_cells[2][leaf], 1);
	    const register double* f_101 = (li->pmap[5]>1)?in.getSampleDataRO(li->pmap[5]-2):HANG_INTERPOLATE(face_cells[2][leaf], 5);
	    const register double* f_111 = (li->pmap[6]>1)?in.getSampleDataRO(li->pmap[6]-2):HANG_INTERPOLATE(face_cells[2][leaf], 6);
	    const register double* f_110 = (li->pmap[7]>1)?in.getSampleDataRO(li->pmap[7]-2):HANG_INTERPOLATE(face_cells[2][leaf], 7);
	    const register double* f_011 = (li->pmap[2]>1)?in.getSampleDataRO(li->pmap[2]-2):HANG_INTERPOLATE(face_cells[2][leaf], 2);
	    const register double* f_010 = (li->pmap[3]>1)?in.getSampleDataRO(li->pmap[3]-2):HANG_INTERPOLATE(face_cells[2][leaf], 3);
	    const register double* f_100 = (li->pmap[4]>1)?in.getSampleDataRO(li->pmap[4]-2):HANG_INTERPOLATE(face_cells[2][leaf], 4);	*/   	    
	    
	    double* o = out.getSampleDataRW(baseoffset+leaf);

	    for (index_t i=0; i < numComp; ++i) {
		o[INDEX3(i,0,0,numComp,3)] = f_000[i]*tmp0_0 + f_001[i]*tmp0_1 + f_100[i]*tmp0_3 + f_101[i]*tmp0_2;
		o[INDEX3(i,1,0,numComp,3)] = f_000[i]*tmp0_8 + f_010[i]*tmp0_13 + f_101[i]*tmp0_10 + f_111[i]*tmp0_11 + tmp0_12*(f_011[i] + f_110[i]) + tmp0_9*(f_001[i] + f_100[i]);
		o[INDEX3(i,2,0,numComp,3)] = f_000[i]*tmp0_28 + f_001[i]*tmp0_29 + f_100[i]*tmp0_31 + f_101[i]*tmp0_30;
		o[INDEX3(i,0,1,numComp,3)] = f_000[i]*tmp0_0 + f_001[i]*tmp0_1 + f_100[i]*tmp0_3 + f_101[i]*tmp0_2;
		o[INDEX3(i,1,1,numComp,3)] = f_000[i]*tmp0_14 + f_001[i]*tmp0_15 + f_010[i]*tmp0_21 + f_011[i]*tmp0_20 + f_100[i]*tmp0_17 + f_101[i]*tmp0_16 + f_110[i]*tmp0_19 + f_111[i]*tmp0_18;
		o[INDEX3(i,2,1,numComp,3)] = f_000[i]*tmp0_32 + f_001[i]*tmp0_33 + f_100[i]*tmp0_35 + f_101[i]*tmp0_34;
		o[INDEX3(i,0,2,numComp,3)] = f_000[i]*tmp0_4 + f_001[i]*tmp0_5 + f_100[i]*tmp0_7 + f_101[i]*tmp0_6;
		o[INDEX3(i,1,2,numComp,3)] = f_000[i]*tmp0_14 + f_001[i]*tmp0_17 + f_010[i]*tmp0_21 + f_011[i]*tmp0_19 + f_100[i]*tmp0_15 + f_101[i]*tmp0_16 + f_110[i]*tmp0_20 + f_111[i]*tmp0_18;
		o[INDEX3(i,2,2,numComp,3)] = f_000[i]*tmp0_28 + f_001[i]*tmp0_29 + f_100[i]*tmp0_31 + f_101[i]*tmp0_30;
		o[INDEX3(i,0,3,numComp,3)] = f_000[i]*tmp0_4 + f_001[i]*tmp0_5 + f_100[i]*tmp0_7 + f_101[i]*tmp0_6;
		o[INDEX3(i,1,3,numComp,3)] = f_000[i]*tmp0_22 + f_010[i]*tmp0_27 + f_101[i]*tmp0_24 + f_111[i]*tmp0_25 + tmp0_23*(f_001[i] + f_100[i]) + tmp0_26*(f_011[i] + f_110[i]);
		o[INDEX3(i,2,3,numComp,3)] = f_000[i]*tmp0_32 + f_001[i]*tmp0_33 + f_100[i]*tmp0_35 + f_101[i]*tmp0_34;
	    } /* end of component loop i */
	  }
        } /* end of face 2 */
        baseoffset+=face_cells[2].size();
        if (face_cells[3].size() > 0) {
#pragma omp for
          for (index_t leaf=0; leaf < face_cells[3].size(); ++leaf) {	  
	    const LeafInfo* li=face_cells[3][leaf]->leafinfo;		// we aren't iterating over all leaves this time	
	    
	    const double h0 = face_cells[3][leaf]->sides[0];
	    const double h1 = face_cells[3][leaf]->sides[1];
	    const double h2 = face_cells[3][leaf]->sides[2];
	    
            const double tmp0_22 = 0.16666666666666666667/h1;
            const double tmp0_16 = 0.16666666666666666667/h1;
            const double tmp0_33 = -0.78867513459481288225/h2;
            const double tmp0_0 = -0.21132486540518711775/h0;
            const double tmp0_21 = -0.62200846792814621559/h1;
            const double tmp0_17 = 0.16666666666666666667/h1;
            const double tmp0_1 = 0.78867513459481288225/h0;
            const double tmp0_20 = -0.16666666666666666667/h1;
            const double tmp0_14 = 0.044658198738520451079/h1;
            const double tmp0_2 = -0.78867513459481288225/h0;
            const double tmp0_27 = -0.62200846792814621559/h1;
            const double tmp0_15 = 0.62200846792814621559/h1;
            const double tmp0_3 = 0.21132486540518711775/h0;
            const double tmp0_26 = -0.16666666666666666667/h1;
            const double tmp0_12 = -0.16666666666666666667/h1;
            const double tmp0_25 = -0.044658198738520451079/h1;
            const double tmp0_13 = -0.044658198738520451079/h1;
            const double tmp0_24 = 0.62200846792814621559/h1;
            const double tmp0_10 = 0.044658198738520451079/h1;
            const double tmp0_11 = -0.62200846792814621559/h1;
            const double tmp0_34 = -0.21132486540518711775/h2;
            const double tmp0_35 = 0.78867513459481288225/h2;
            const double tmp0_8 = 0.16666666666666666667/h1;
            const double tmp0_29 = -0.21132486540518711775/h2;
            const double tmp0_9 = 0.62200846792814621559/h1;
            const double tmp0_30 = -0.78867513459481288225/h2;
            const double tmp0_28 = 0.78867513459481288225/h2;
            const double tmp0_32 = 0.21132486540518711775/h2;
            const double tmp0_31 = 0.21132486540518711775/h2;
            const double tmp0_18 = -0.16666666666666666667/h1;
            const double tmp0_4 = -0.78867513459481288225/h0;
            const double tmp0_19 = -0.044658198738520451079/h1;
            const double tmp0_5 = 0.21132486540518711775/h0;
            const double tmp0_6 = -0.21132486540518711775/h0;
            const double tmp0_23 = 0.044658198738520451079/h1;
            const double tmp0_7 = 0.78867513459481288225/h0;
	    
	    int buffcounter=0;
	    const double* src1=0;
	    int whichchild=-1;	    

	    GETHANGSAMPLE(face_cells[3][leaf], 0, f_000)
	    GETHANGSAMPLE(face_cells[3][leaf], 4, f_001)
	    GETHANGSAMPLE(face_cells[3][leaf], 5, f_101)
	    GETHANGSAMPLE(face_cells[3][leaf], 6, f_111)
	    GETHANGSAMPLE(face_cells[3][leaf], 2, f_110)
	    GETHANGSAMPLE(face_cells[3][leaf], 7, f_011)
	    GETHANGSAMPLE(face_cells[3][leaf], 3, f_010)
	    GETHANGSAMPLE(face_cells[3][leaf], 1, f_100)	    
	    
	    
/*	    const register double* f_000 = (li->pmap[0]>1)?in.getSampleDataRO(li->pmap[0]-2):HANG_INTERPOLATE(face_cells[3][leaf], 0);
	    const register double* f_001 = (li->pmap[1]>1)?in.getSampleDataRO(li->pmap[1]-2):HANG_INTERPOLATE(face_cells[3][leaf], 1);
	    const register double* f_101 = (li->pmap[5]>1)?in.getSampleDataRO(li->pmap[5]-2):HANG_INTERPOLATE(face_cells[3][leaf], 5);
	    const register double* f_111 = (li->pmap[6]>1)?in.getSampleDataRO(li->pmap[6]-2):HANG_INTERPOLATE(face_cells[3][leaf], 6);
	    const register double* f_110 = (li->pmap[7]>1)?in.getSampleDataRO(li->pmap[7]-2):HANG_INTERPOLATE(face_cells[3][leaf], 7);
	    const register double* f_011 = (li->pmap[2]>1)?in.getSampleDataRO(li->pmap[2]-2):HANG_INTERPOLATE(face_cells[3][leaf], 2);
	    const register double* f_010 = (li->pmap[3]>1)?in.getSampleDataRO(li->pmap[3]-2):HANG_INTERPOLATE(face_cells[3][leaf], 3);
	    const register double* f_100 = (li->pmap[4]>1)?in.getSampleDataRO(li->pmap[4]-2):HANG_INTERPOLATE(face_cells[3][leaf], 4);	   	*/    
	    
	    double* o = out.getSampleDataRW(baseoffset+leaf);	    
	    for (index_t i=0; i < numComp; ++i) {
		o[INDEX3(i,0,0,numComp,3)] = f_010[i]*tmp0_2 + f_011[i]*tmp0_0 + f_110[i]*tmp0_1 + f_111[i]*tmp0_3;
		o[INDEX3(i,1,0,numComp,3)] = f_000[i]*tmp0_11 + f_010[i]*tmp0_9 + f_101[i]*tmp0_13 + f_111[i]*tmp0_10 + tmp0_12*(f_001[i] + f_100[i]) + tmp0_8*(f_011[i] + f_110[i]);
		o[INDEX3(i,2,0,numComp,3)] = f_010[i]*tmp0_30 + f_011[i]*tmp0_28 + f_110[i]*tmp0_29 + f_111[i]*tmp0_31;
		o[INDEX3(i,0,1,numComp,3)] = f_010[i]*tmp0_2 + f_011[i]*tmp0_0 + f_110[i]*tmp0_1 + f_111[i]*tmp0_3;
		o[INDEX3(i,1,1,numComp,3)] = f_000[i]*tmp0_18 + f_001[i]*tmp0_19 + f_010[i]*tmp0_16 + f_011[i]*tmp0_14 + f_100[i]*tmp0_21 + f_101[i]*tmp0_20 + f_110[i]*tmp0_15 + f_111[i]*tmp0_17;
		o[INDEX3(i,2,1,numComp,3)] = f_010[i]*tmp0_34 + f_011[i]*tmp0_32 + f_110[i]*tmp0_33 + f_111[i]*tmp0_35;
		o[INDEX3(i,0,2,numComp,3)] = f_010[i]*tmp0_6 + f_011[i]*tmp0_4 + f_110[i]*tmp0_5 + f_111[i]*tmp0_7;
		o[INDEX3(i,1,2,numComp,3)] = f_000[i]*tmp0_18 + f_001[i]*tmp0_21 + f_010[i]*tmp0_16 + f_011[i]*tmp0_15 + f_100[i]*tmp0_19 + f_101[i]*tmp0_20 + f_110[i]*tmp0_14 + f_111[i]*tmp0_17;
		o[INDEX3(i,2,2,numComp,3)] = f_010[i]*tmp0_30 + f_011[i]*tmp0_28 + f_110[i]*tmp0_29 + f_111[i]*tmp0_31;
		o[INDEX3(i,0,3,numComp,3)] = f_010[i]*tmp0_6 + f_011[i]*tmp0_4 + f_110[i]*tmp0_5 + f_111[i]*tmp0_7;
		o[INDEX3(i,1,3,numComp,3)] = f_000[i]*tmp0_25 + f_010[i]*tmp0_23 + f_101[i]*tmp0_27 + f_111[i]*tmp0_24 + tmp0_22*(f_011[i] + f_110[i]) + tmp0_26*(f_001[i] + f_100[i]);
		o[INDEX3(i,2,3,numComp,3)] = f_010[i]*tmp0_34 + f_011[i]*tmp0_32 + f_110[i]*tmp0_33 + f_111[i]*tmp0_35;
	    } /* end of component loop i */
	  }
        } /* end of face 3 */
        baseoffset+=face_cells[3].size();
        if (face_cells[4].size() > 0) {
#pragma omp for
          for (index_t leaf=0; leaf < face_cells[4].size(); ++leaf) {	  
	    const LeafInfo* li=face_cells[4][leaf]->leafinfo;		// we aren't iterating over all leaves this time
	    
	    const double h0 = face_cells[4][leaf]->sides[0];
	    const double h1 = face_cells[4][leaf]->sides[1];
	    const double h2 = face_cells[4][leaf]->sides[2];        

	    const double tmp0_22 = -0.16666666666666666667/h2;
            const double tmp0_16 = -0.62200846792814621559/h2;
            const double tmp0_33 = 0.044658198738520451079/h2;
            const double tmp0_0 = -0.78867513459481288225/h0;
            const double tmp0_21 = 0.044658198738520451079/h2;
            const double tmp0_17 = -0.16666666666666666667/h2;
            const double tmp0_1 = 0.78867513459481288225/h0;
            const double tmp0_20 = 0.16666666666666666667/h2;
            const double tmp0_14 = 0.78867513459481288225/h1;
            const double tmp0_2 = 0.21132486540518711775/h0;
            const double tmp0_27 = 0.62200846792814621559/h2;
            const double tmp0_15 = 0.21132486540518711775/h1;
            const double tmp0_3 = -0.21132486540518711775/h0;
            const double tmp0_26 = 0.16666666666666666667/h2;
            const double tmp0_12 = -0.21132486540518711775/h1;
            const double tmp0_25 = -0.044658198738520451079/h2;
            const double tmp0_13 = -0.78867513459481288225/h1;
            const double tmp0_24 = -0.16666666666666666667/h2;
            const double tmp0_10 = 0.21132486540518711775/h1;
            const double tmp0_11 = 0.78867513459481288225/h1;
            const double tmp0_34 = 0.16666666666666666667/h2;
            const double tmp0_35 = 0.62200846792814621559/h2;
            const double tmp0_8 = -0.78867513459481288225/h1;
            const double tmp0_29 = 0.16666666666666666667/h2;
            const double tmp0_9 = -0.21132486540518711775/h1;
            const double tmp0_30 = -0.044658198738520451079/h2;
            const double tmp0_28 = 0.044658198738520451079/h2;
            const double tmp0_32 = -0.62200846792814621559/h2;
            const double tmp0_31 = -0.16666666666666666667/h2;
            const double tmp0_18 = -0.044658198738520451079/h2;
            const double tmp0_4 = -0.21132486540518711775/h0;
            const double tmp0_19 = 0.62200846792814621559/h2;
            const double tmp0_5 = 0.21132486540518711775/h0;
            const double tmp0_6 = 0.78867513459481288225/h0;
            const double tmp0_23 = -0.62200846792814621559/h2;
            const double tmp0_7 = -0.78867513459481288225/h0;
	    
	    int buffcounter=0;
	    const double* src1=0;
	    int whichchild=-1;	    
	    
	    GETHANGSAMPLE(face_cells[4][leaf], 0, f_000)
	    GETHANGSAMPLE(face_cells[4][leaf], 4, f_001)
	    GETHANGSAMPLE(face_cells[4][leaf], 5, f_101)
	    GETHANGSAMPLE(face_cells[4][leaf], 6, f_111)
	    GETHANGSAMPLE(face_cells[4][leaf], 2, f_110)
	    GETHANGSAMPLE(face_cells[4][leaf], 7, f_011)
	    GETHANGSAMPLE(face_cells[4][leaf], 3, f_010)
	    GETHANGSAMPLE(face_cells[4][leaf], 1, f_100)	    
	    
	    
/*	    const register double* f_000 = (li->pmap[0]>1)?in.getSampleDataRO(li->pmap[0]-2):HANG_INTERPOLATE(face_cells[4][leaf], 0);
	    const register double* f_001 = (li->pmap[1]>1)?in.getSampleDataRO(li->pmap[1]-2):HANG_INTERPOLATE(face_cells[4][leaf], 1);
	    const register double* f_101 = (li->pmap[5]>1)?in.getSampleDataRO(li->pmap[5]-2):HANG_INTERPOLATE(face_cells[4][leaf], 5);
	    const register double* f_111 = (li->pmap[6]>1)?in.getSampleDataRO(li->pmap[6]-2):HANG_INTERPOLATE(face_cells[4][leaf], 6);
	    const register double* f_110 = (li->pmap[7]>1)?in.getSampleDataRO(li->pmap[7]-2):HANG_INTERPOLATE(face_cells[4][leaf], 7);
	    const register double* f_011 = (li->pmap[2]>1)?in.getSampleDataRO(li->pmap[2]-2):HANG_INTERPOLATE(face_cells[4][leaf], 2);
	    const register double* f_010 = (li->pmap[3]>1)?in.getSampleDataRO(li->pmap[3]-2):HANG_INTERPOLATE(face_cells[4][leaf], 3);
	    const register double* f_100 = (li->pmap[4]>1)?in.getSampleDataRO(li->pmap[4]-2):HANG_INTERPOLATE(face_cells[4][leaf], 4);	   */	    
	    
	    double* o = out.getSampleDataRW(baseoffset+leaf);	    
	    for (index_t i=0; i < numComp; ++i) {
		o[INDEX3(i,0,0,numComp,3)] = f_000[i]*tmp0_0 + f_010[i]*tmp0_3 + f_100[i]*tmp0_1 + f_110[i]*tmp0_2;
		o[INDEX3(i,1,0,numComp,3)] = f_000[i]*tmp0_8 + f_010[i]*tmp0_11 + f_100[i]*tmp0_9 + f_110[i]*tmp0_10;
		o[INDEX3(i,2,0,numComp,3)] = f_000[i]*tmp0_16 + f_001[i]*tmp0_19 + f_110[i]*tmp0_18 + f_111[i]*tmp0_21 + tmp0_17*(f_010[i] + f_100[i]) + tmp0_20*(f_011[i] + f_101[i]);
		o[INDEX3(i,0,1,numComp,3)] = f_000[i]*tmp0_0 + f_010[i]*tmp0_3 + f_100[i]*tmp0_1 + f_110[i]*tmp0_2;
		o[INDEX3(i,1,1,numComp,3)] = f_000[i]*tmp0_12 + f_010[i]*tmp0_15 + f_100[i]*tmp0_13 + f_110[i]*tmp0_14;
		o[INDEX3(i,2,1,numComp,3)] = f_000[i]*tmp0_22 + f_001[i]*tmp0_26 + f_010[i]*tmp0_25 + f_011[i]*tmp0_28 + f_100[i]*tmp0_23 + f_101[i]*tmp0_27 + f_110[i]*tmp0_24 + f_111[i]*tmp0_29;
		o[INDEX3(i,0,2,numComp,3)] = f_000[i]*tmp0_4 + f_010[i]*tmp0_7 + f_100[i]*tmp0_5 + f_110[i]*tmp0_6;
		o[INDEX3(i,1,2,numComp,3)] = f_000[i]*tmp0_8 + f_010[i]*tmp0_11 + f_100[i]*tmp0_9 + f_110[i]*tmp0_10;
		o[INDEX3(i,2,2,numComp,3)] = f_000[i]*tmp0_22 + f_001[i]*tmp0_26 + f_010[i]*tmp0_23 + f_011[i]*tmp0_27 + f_100[i]*tmp0_25 + f_101[i]*tmp0_28 + f_110[i]*tmp0_24 + f_111[i]*tmp0_29;
		o[INDEX3(i,0,3,numComp,3)] = f_000[i]*tmp0_4 + f_010[i]*tmp0_7 + f_100[i]*tmp0_5 + f_110[i]*tmp0_6;
		o[INDEX3(i,1,3,numComp,3)] = f_000[i]*tmp0_12 + f_010[i]*tmp0_15 + f_100[i]*tmp0_13 + f_110[i]*tmp0_14;
		o[INDEX3(i,2,3,numComp,3)] = f_000[i]*tmp0_30 + f_001[i]*tmp0_33 + f_110[i]*tmp0_32 + f_111[i]*tmp0_35 + tmp0_31*(f_010[i] + f_100[i]) + tmp0_34*(f_011[i] + f_101[i]);
	    } /* end of component loop i */
	  }  
        } /* end of face 4 */
        baseoffset+=face_cells[4].size();
        if (face_cells[5].size() > 0) {
#pragma omp for
        for (index_t leaf=0; leaf < face_cells[5].size(); ++leaf) {	  
	    const LeafInfo* li=face_cells[5][leaf]->leafinfo;		// we aren't iterating over all leaves this time
	    
	    const double h0 = face_cells[5][leaf]->sides[0];
	    const double h1 = face_cells[5][leaf]->sides[1];
	    const double h2 = face_cells[5][leaf]->sides[2];        

            const double tmp0_22 = 0.16666666666666666667/h2;
            const double tmp0_16 = 0.62200846792814621559/h2;
            const double tmp0_33 = -0.044658198738520451079/h2;
            const double tmp0_0 = -0.78867513459481288225/h0;
            const double tmp0_21 = -0.16666666666666666667/h2;
            const double tmp0_17 = 0.16666666666666666667/h2;
            const double tmp0_1 = 0.78867513459481288225/h0;
            const double tmp0_20 = -0.044658198738520451079/h2;
            const double tmp0_14 = 0.21132486540518711775/h1;
            const double tmp0_2 = -0.21132486540518711775/h0;
            const double tmp0_27 = -0.16666666666666666667/h2;
            const double tmp0_15 = 0.78867513459481288225/h1;
            const double tmp0_3 = 0.21132486540518711775/h0;
            const double tmp0_26 = -0.16666666666666666667/h2;
            const double tmp0_12 = -0.21132486540518711775/h1;
            const double tmp0_25 = 0.16666666666666666667/h2;
            const double tmp0_13 = -0.78867513459481288225/h1;
            const double tmp0_24 = 0.044658198738520451079/h2;
            const double tmp0_10 = 0.78867513459481288225/h1;
            const double tmp0_11 = 0.21132486540518711775/h1;
            const double tmp0_34 = -0.62200846792814621559/h2;
            const double tmp0_35 = -0.16666666666666666667/h2;
            const double tmp0_8 = -0.78867513459481288225/h1;
            const double tmp0_29 = -0.62200846792814621559/h2;
            const double tmp0_9 = -0.21132486540518711775/h1;
            const double tmp0_30 = 0.044658198738520451079/h2;
            const double tmp0_28 = -0.044658198738520451079/h2;
            const double tmp0_32 = 0.62200846792814621559/h2;
            const double tmp0_31 = 0.16666666666666666667/h2;
            const double tmp0_18 = 0.044658198738520451079/h2;
            const double tmp0_4 = -0.21132486540518711775/h0;
            const double tmp0_19 = -0.62200846792814621559/h2;
            const double tmp0_5 = 0.21132486540518711775/h0;
            const double tmp0_6 = -0.78867513459481288225/h0;
            const double tmp0_23 = 0.62200846792814621559/h2;
            const double tmp0_7 = 0.78867513459481288225/h0;
	    
	    int buffcounter=0;
	    const double* src1=0;
	    int whichchild=-1;	    

	    GETHANGSAMPLE(face_cells[5][leaf], 0, f_000)
	    GETHANGSAMPLE(face_cells[5][leaf], 4, f_001)
	    GETHANGSAMPLE(face_cells[5][leaf], 5, f_101)
	    GETHANGSAMPLE(face_cells[5][leaf], 6, f_111)
	    GETHANGSAMPLE(face_cells[5][leaf], 2, f_110)
	    GETHANGSAMPLE(face_cells[5][leaf], 7, f_011)
	    GETHANGSAMPLE(face_cells[5][leaf], 3, f_010)
	    GETHANGSAMPLE(face_cells[5][leaf], 1, f_100)	    
	    
/*	    const register double* f_000 = (li->pmap[0]>1)?in.getSampleDataRO(li->pmap[0]-2):HANG_INTERPOLATE(face_cells[5][leaf], 0);
	    const register double* f_001 = (li->pmap[1]>1)?in.getSampleDataRO(li->pmap[1]-2):HANG_INTERPOLATE(face_cells[5][leaf], 1);
	    const register double* f_101 = (li->pmap[5]>1)?in.getSampleDataRO(li->pmap[5]-2):HANG_INTERPOLATE(face_cells[5][leaf], 5);
	    const register double* f_111 = (li->pmap[6]>1)?in.getSampleDataRO(li->pmap[6]-2):HANG_INTERPOLATE(face_cells[5][leaf], 6);
	    const register double* f_110 = (li->pmap[7]>1)?in.getSampleDataRO(li->pmap[7]-2):HANG_INTERPOLATE(face_cells[5][leaf], 7);
	    const register double* f_011 = (li->pmap[2]>1)?in.getSampleDataRO(li->pmap[2]-2):HANG_INTERPOLATE(face_cells[5][leaf], 2);
	    const register double* f_010 = (li->pmap[3]>1)?in.getSampleDataRO(li->pmap[3]-2):HANG_INTERPOLATE(face_cells[5][leaf], 3);
	    const register double* f_100 = (li->pmap[4]>1)?in.getSampleDataRO(li->pmap[4]-2):HANG_INTERPOLATE(face_cells[5][leaf], 4);	  */ 	    
	    
	    double* o = out.getSampleDataRW(baseoffset+leaf);	    
	    for (index_t i=0; i < numComp; ++i) {
		o[INDEX3(i,0,0,numComp,3)] = f_001[i]*tmp0_0 + f_011[i]*tmp0_2 + f_101[i]*tmp0_1 + f_111[i]*tmp0_3;
		o[INDEX3(i,1,0,numComp,3)] = f_001[i]*tmp0_8 + f_011[i]*tmp0_10 + f_101[i]*tmp0_9 + f_111[i]*tmp0_11;
		o[INDEX3(i,2,0,numComp,3)] = f_000[i]*tmp0_19 + f_001[i]*tmp0_16 + f_110[i]*tmp0_20 + f_111[i]*tmp0_18 + tmp0_17*(f_011[i] + f_101[i]) + tmp0_21*(f_010[i] + f_100[i]);
		o[INDEX3(i,0,1,numComp,3)] = f_001[i]*tmp0_0 + f_011[i]*tmp0_2 + f_101[i]*tmp0_1 + f_111[i]*tmp0_3;
		o[INDEX3(i,1,1,numComp,3)] = f_001[i]*tmp0_12 + f_011[i]*tmp0_14 + f_101[i]*tmp0_13 + f_111[i]*tmp0_15;
		o[INDEX3(i,2,1,numComp,3)] = f_000[i]*tmp0_26 + f_001[i]*tmp0_22 + f_010[i]*tmp0_28 + f_011[i]*tmp0_24 + f_100[i]*tmp0_29 + f_101[i]*tmp0_23 + f_110[i]*tmp0_27 + f_111[i]*tmp0_25;
		o[INDEX3(i,0,2,numComp,3)] = f_001[i]*tmp0_4 + f_011[i]*tmp0_6 + f_101[i]*tmp0_5 + f_111[i]*tmp0_7;
		o[INDEX3(i,1,2,numComp,3)] = f_001[i]*tmp0_8 + f_011[i]*tmp0_10 + f_101[i]*tmp0_9 + f_111[i]*tmp0_11;
		o[INDEX3(i,2,2,numComp,3)] = f_000[i]*tmp0_26 + f_001[i]*tmp0_22 + f_010[i]*tmp0_29 + f_011[i]*tmp0_23 + f_100[i]*tmp0_28 + f_101[i]*tmp0_24 + f_110[i]*tmp0_27 + f_111[i]*tmp0_25;
		o[INDEX3(i,0,3,numComp,3)] = f_001[i]*tmp0_4 + f_011[i]*tmp0_6 + f_101[i]*tmp0_5 + f_111[i]*tmp0_7;
		o[INDEX3(i,1,3,numComp,3)] = f_001[i]*tmp0_12 + f_011[i]*tmp0_14 + f_101[i]*tmp0_13 + f_111[i]*tmp0_15;
		o[INDEX3(i,2,3,numComp,3)] = f_000[i]*tmp0_33 + f_001[i]*tmp0_30 + f_110[i]*tmp0_34 + f_111[i]*tmp0_32 + tmp0_31*(f_011[i] + f_101[i]) + tmp0_35*(f_010[i] + f_100[i]);
	    } /* end of component loop i */
          } /* end of face 5 */
	}  
        /* GENERATOR SNIP_GRAD_FACES BOTTOM */
    } else if (out.getFunctionSpace().getTypeCode() == red_disc_faces) {
      throw BuckleyException("Haven't done reduced face function yet.");
#if 0      
        /* GENERATOR SNIP_GRAD_REDUCED_FACES TOP */
        if (m_faceOffset[0] > -1) {
            const double tmp0_0 = -0.25/h0;
            const double tmp0_4 = -0.5/h2;
            const double tmp0_1 = 0.25/h0;
            const double tmp0_5 = 0.5/h2;
            const double tmp0_2 = -0.5/h1;
            const double tmp0_3 = 0.5/h1;
#pragma omp parallel for
            for (index_t k2=0; k2 < m_NE2; ++k2) {
                for (index_t k1=0; k1 < m_NE1; ++k1) {
                    const register double* f_000 = in.getSampleDataRO(INDEX3(0,k1,k2, m_N0,m_N1));
                    const register double* f_001 = in.getSampleDataRO(INDEX3(0,k1,k2+1, m_N0,m_N1));
                    const register double* f_101 = in.getSampleDataRO(INDEX3(1,k1,k2+1, m_N0,m_N1));
                    const register double* f_011 = in.getSampleDataRO(INDEX3(0,k1+1,k2+1, m_N0,m_N1));
                    const register double* f_100 = in.getSampleDataRO(INDEX3(1,k1,k2, m_N0,m_N1));
                    const register double* f_110 = in.getSampleDataRO(INDEX3(1,k1+1,k2, m_N0,m_N1));
                    const register double* f_010 = in.getSampleDataRO(INDEX3(0,k1+1,k2, m_N0,m_N1));
                    const register double* f_111 = in.getSampleDataRO(INDEX3(1,k1+1,k2+1, m_N0,m_N1));
                    double* o = out.getSampleDataRW(m_faceOffset[0]+INDEX2(k1,k2,m_NE1));
                    for (index_t i=0; i < numComp; ++i) {
                        o[INDEX3(i,0,0,numComp,3)] = tmp0_0*(f_000[i] + f_001[i] + f_010[i] + f_011[i]) + tmp0_1*(f_100[i] + f_101[i] + f_110[i] + f_111[i]);
                        o[INDEX3(i,1,0,numComp,3)] = tmp0_2*(f_000[i] + f_001[i]) + tmp0_3*(f_010[i] + f_011[i]);
                        o[INDEX3(i,2,0,numComp,3)] = tmp0_4*(f_000[i] + f_010[i]) + tmp0_5*(f_001[i] + f_011[i]);
                    } /* end of component loop i */
                } /* end of k1 loop */
            } /* end of k2 loop */
        } /* end of face 0 */
        if (m_faceOffset[1] > -1) {
            const double tmp0_0 = -0.25/h0;
            const double tmp0_4 = 0.5/h2;
            const double tmp0_1 = 0.25/h0;
            const double tmp0_5 = -0.5/h2;
            const double tmp0_2 = -0.5/h1;
            const double tmp0_3 = 0.5/h1;
#pragma omp parallel for
            for (index_t k2=0; k2 < m_NE2; ++k2) {
                for (index_t k1=0; k1 < m_NE1; ++k1) {
                    const register double* f_000 = in.getSampleDataRO(INDEX3(m_N0-2,k1,k2, m_N0,m_N1));
                    const register double* f_001 = in.getSampleDataRO(INDEX3(m_N0-2,k1,k2+1, m_N0,m_N1));
                    const register double* f_101 = in.getSampleDataRO(INDEX3(m_N0-1,k1,k2+1, m_N0,m_N1));
                    const register double* f_011 = in.getSampleDataRO(INDEX3(m_N0-2,k1+1,k2+1, m_N0,m_N1));
                    const register double* f_100 = in.getSampleDataRO(INDEX3(m_N0-1,k1,k2, m_N0,m_N1));
                    const register double* f_110 = in.getSampleDataRO(INDEX3(m_N0-1,k1+1,k2, m_N0,m_N1));
                    const register double* f_010 = in.getSampleDataRO(INDEX3(m_N0-2,k1+1,k2, m_N0,m_N1));
                    const register double* f_111 = in.getSampleDataRO(INDEX3(m_N0-1,k1+1,k2+1, m_N0,m_N1));
                    double* o = out.getSampleDataRW(m_faceOffset[1]+INDEX2(k1,k2,m_NE1));
                    for (index_t i=0; i < numComp; ++i) {
                        o[INDEX3(i,0,0,numComp,3)] = tmp0_0*(f_000[i] + f_001[i] + f_010[i] + f_011[i]) + tmp0_1*(f_100[i] + f_101[i] + f_110[i] + f_111[i]);
                        o[INDEX3(i,1,0,numComp,3)] = tmp0_2*(f_100[i] + f_101[i]) + tmp0_3*(f_110[i] + f_111[i]);
                        o[INDEX3(i,2,0,numComp,3)] = tmp0_4*(f_101[i] + f_111[i]) + tmp0_5*(f_100[i] + f_110[i]);
                    } /* end of component loop i */
                } /* end of k1 loop */
            } /* end of k2 loop */
        } /* end of face 1 */
        if (m_faceOffset[2] > -1) {
            const double tmp0_0 = -0.5/h0;
            const double tmp0_4 = -0.5/h2;
            const double tmp0_1 = 0.5/h0;
            const double tmp0_5 = 0.5/h2;
            const double tmp0_2 = -0.25/h1;
            const double tmp0_3 = 0.25/h1;
#pragma omp parallel for
            for (index_t k2=0; k2 < m_NE2; ++k2) {
                for (index_t k0=0; k0 < m_NE0; ++k0) {
                    const register double* f_000 = in.getSampleDataRO(INDEX3(k0,0,k2, m_N0,m_N1));
                    const register double* f_001 = in.getSampleDataRO(INDEX3(k0,0,k2+1, m_N0,m_N1));
                    const register double* f_101 = in.getSampleDataRO(INDEX3(k0+1,0,k2+1, m_N0,m_N1));
                    const register double* f_100 = in.getSampleDataRO(INDEX3(k0+1,0,k2, m_N0,m_N1));
                    const register double* f_011 = in.getSampleDataRO(INDEX3(k0,1,k2+1, m_N0,m_N1));
                    const register double* f_110 = in.getSampleDataRO(INDEX3(k0+1,1,k2, m_N0,m_N1));
                    const register double* f_010 = in.getSampleDataRO(INDEX3(k0,1,k2, m_N0,m_N1));
                    const register double* f_111 = in.getSampleDataRO(INDEX3(k0+1,1,k2+1, m_N0,m_N1));
                    double* o = out.getSampleDataRW(m_faceOffset[2]+INDEX2(k0,k2,m_NE0));
                    for (index_t i=0; i < numComp; ++i) {
                        o[INDEX3(i,0,0,numComp,3)] = tmp0_0*(f_000[i] + f_001[i]) + tmp0_1*(f_100[i] + f_101[i]);
                        o[INDEX3(i,1,0,numComp,3)] = tmp0_2*(f_000[i] + f_001[i] + f_100[i] + f_101[i]) + tmp0_3*(f_010[i] + f_011[i] + f_110[i] + f_111[i]);
                        o[INDEX3(i,2,0,numComp,3)] = tmp0_4*(f_000[i] + f_100[i]) + tmp0_5*(f_001[i] + f_101[i]);
                    } /* end of component loop i */
                } /* end of k0 loop */
            } /* end of k2 loop */
        } /* end of face 2 */
        if (m_faceOffset[3] > -1) {
            const double tmp0_0 = -0.5/h0;
            const double tmp0_4 = 0.5/h2;
            const double tmp0_1 = 0.5/h0;
            const double tmp0_5 = -0.5/h2;
            const double tmp0_2 = 0.25/h1;
            const double tmp0_3 = -0.25/h1;
#pragma omp parallel for
            for (index_t k2=0; k2 < m_NE2; ++k2) {
                for (index_t k0=0; k0 < m_NE0; ++k0) {
                    const register double* f_011 = in.getSampleDataRO(INDEX3(k0,m_N1-1,k2+1, m_N0,m_N1));
                    const register double* f_110 = in.getSampleDataRO(INDEX3(k0+1,m_N1-1,k2, m_N0,m_N1));
                    const register double* f_010 = in.getSampleDataRO(INDEX3(k0,m_N1-1,k2, m_N0,m_N1));
                    const register double* f_111 = in.getSampleDataRO(INDEX3(k0+1,m_N1-1,k2+1, m_N0,m_N1));
                    const register double* f_000 = in.getSampleDataRO(INDEX3(k0,m_N1-2,k2, m_N0,m_N1));
                    const register double* f_101 = in.getSampleDataRO(INDEX3(k0+1,m_N1-2,k2+1, m_N0,m_N1));
                    const register double* f_001 = in.getSampleDataRO(INDEX3(k0,m_N1-2,k2+1, m_N0,m_N1));
                    const register double* f_100 = in.getSampleDataRO(INDEX3(k0+1,m_N1-2,k2, m_N0,m_N1));
                    double* o = out.getSampleDataRW(m_faceOffset[3]+INDEX2(k0,k2,m_NE0));
                    for (index_t i=0; i < numComp; ++i) {
                        o[INDEX3(i,0,0,numComp,3)] = tmp0_0*(f_010[i] + f_011[i]) + tmp0_1*(f_110[i] + f_111[i]);
                        o[INDEX3(i,1,0,numComp,3)] = tmp0_2*(f_010[i] + f_011[i] + f_110[i] + f_111[i]) + tmp0_3*(f_000[i] + f_001[i] + f_100[i] + f_101[i]);
                        o[INDEX3(i,2,0,numComp,3)] = tmp0_4*(f_011[i] + f_111[i]) + tmp0_5*(f_010[i] + f_110[i]);
                    } /* end of component loop i */
                } /* end of k0 loop */
            } /* end of k2 loop */
        } /* end of face 3 */
        if (m_faceOffset[4] > -1) {
            const double tmp0_0 = -0.5/h0;
            const double tmp0_4 = -0.25/h2;
            const double tmp0_1 = 0.5/h0;
            const double tmp0_5 = 0.25/h2;
            const double tmp0_2 = -0.5/h1;
            const double tmp0_3 = 0.5/h1;
#pragma omp parallel for
            for (index_t k1=0; k1 < m_NE1; ++k1) {
                for (index_t k0=0; k0 < m_NE0; ++k0) {
                    const register double* f_000 = in.getSampleDataRO(INDEX3(k0,k1,0, m_N0,m_N1));
                    const register double* f_100 = in.getSampleDataRO(INDEX3(k0+1,k1,0, m_N0,m_N1));
                    const register double* f_110 = in.getSampleDataRO(INDEX3(k0+1,k1+1,0, m_N0,m_N1));
                    const register double* f_010 = in.getSampleDataRO(INDEX3(k0,k1+1,0, m_N0,m_N1));
                    const register double* f_001 = in.getSampleDataRO(INDEX3(k0,k1,1, m_N0,m_N1));
                    const register double* f_101 = in.getSampleDataRO(INDEX3(k0+1,k1,1, m_N0,m_N1));
                    const register double* f_011 = in.getSampleDataRO(INDEX3(k0,k1+1,1, m_N0,m_N1));
                    const register double* f_111 = in.getSampleDataRO(INDEX3(k0+1,k1+1,1, m_N0,m_N1));
                    double* o = out.getSampleDataRW(m_faceOffset[4]+INDEX2(k0,k1,m_NE0));
                    for (index_t i=0; i < numComp; ++i) {
                        o[INDEX3(i,0,0,numComp,3)] = tmp0_0*(f_000[i] + f_010[i]) + tmp0_1*(f_100[i] + f_110[i]);
                        o[INDEX3(i,1,0,numComp,3)] = tmp0_2*(f_000[i] + f_100[i]) + tmp0_3*(f_010[i] + f_110[i]);
                        o[INDEX3(i,2,0,numComp,3)] = tmp0_4*(f_000[i] + f_010[i] + f_100[i] + f_110[i]) + tmp0_5*(f_001[i] + f_011[i] + f_101[i] + f_111[i]);
                    } /* end of component loop i */
                } /* end of k0 loop */
            } /* end of k1 loop */
        } /* end of face 4 */
        if (m_faceOffset[5] > -1) {
            const double tmp0_0 = -0.5/h0;
            const double tmp0_4 = 0.25/h2;
            const double tmp0_1 = 0.5/h0;
            const double tmp0_5 = -0.25/h2;
            const double tmp0_2 = -0.5/h1;
            const double tmp0_3 = 0.5/h1;
#pragma omp parallel for
            for (index_t k1=0; k1 < m_NE1; ++k1) {
                for (index_t k0=0; k0 < m_NE0; ++k0) {
                    const register double* f_001 = in.getSampleDataRO(INDEX3(k0,k1,m_N2-1, m_N0,m_N1));
                    const register double* f_101 = in.getSampleDataRO(INDEX3(k0+1,k1,m_N2-1, m_N0,m_N1));
                    const register double* f_011 = in.getSampleDataRO(INDEX3(k0,k1+1,m_N2-1, m_N0,m_N1));
                    const register double* f_111 = in.getSampleDataRO(INDEX3(k0+1,k1+1,m_N2-1, m_N0,m_N1));
                    const register double* f_000 = in.getSampleDataRO(INDEX3(k0,k1,m_N2-2, m_N0,m_N1));
                    const register double* f_100 = in.getSampleDataRO(INDEX3(k0+1,k1,m_N2-2, m_N0,m_N1));
                    const register double* f_110 = in.getSampleDataRO(INDEX3(k0+1,k1+1,m_N2-2, m_N0,m_N1));
                    const register double* f_010 = in.getSampleDataRO(INDEX3(k0,k1+1,m_N2-2, m_N0,m_N1));
                    double* o = out.getSampleDataRW(m_faceOffset[5]+INDEX2(k0,k1,m_NE0));
                    for (index_t i=0; i < numComp; ++i) {
                        o[INDEX3(i,0,0,numComp,3)] = tmp0_0*(f_001[i] + f_011[i]) + tmp0_1*(f_101[i] + f_111[i]);
                        o[INDEX3(i,1,0,numComp,3)] = tmp0_2*(f_001[i] + f_101[i]) + tmp0_3*(f_011[i] + f_111[i]);
                        o[INDEX3(i,2,0,numComp,3)] = tmp0_4*(f_001[i] + f_011[i] + f_101[i] + f_111[i]) + tmp0_5*(f_000[i] + f_010[i] + f_100[i] + f_110[i]);
                    } /* end of component loop i */
                } /* end of k0 loop */
            } /* end of k1 loop */
        } /* end of face 5 */
        /* GENERATOR SNIP_GRAD_REDUCED_FACES BOTTOM */
#endif	
      } else {
        err=true;
      }	
    }  // end omp parallel
    if (err)
    {
        stringstream msg;
        msg << "setToGradient() not implemented for "
            << functionSpaceTypeAsString(out.getFunctionSpace().getTypeCode());
        throw BuckleyException(msg.str());
    }
}

	    
#undef GETHANGSAMPLE
#endif



#if 0
void BuckleyDomain::setToGradient(escript::Data& grad, const escript::Data& arg) const
{
    // sanity checks
    if (!grad.getFunctionSpace().upToDate() ||  !arg.getFunctionSpace().upToDate())
    {
        throw BuckleyException("Stale functionspaces passed to setToGradient");
    }
    if ((grad.getFunctionSpace().getDomain().get()!=this) || (arg.getFunctionSpace().getDomain().get()!=this))
    {
        throw BuckleyException("Domain mismatch in setToGradient");
    }
    if ((grad.getFunctionSpace().getTypeCode()!=discfn) || (arg.getFunctionSpace().getTypeCode()!=ctsfn))
    {
        throw BuckleyException("Invalid functionspaces passed to setToGradient");  
    }
    if ((grad.getDataPointShape()!=MAT3X3) || (arg.getDataPointShape()!=VECTOR3))
    {
        throw BuckleyException("Expected shapes for grad inputs are (3,3) and (3,)");
    }
    if (modified)
    {
        processMods();
    }
    
       
    double* dest=grad.getSampleDataRW(0);
    for (int i=0;i<ot.leafCount();++i)
    {
        double quadpts[NUM_QUAD][3];
	for (int j=0;j<NUM_QUAD;++j)
	{
	    leaves[i]->quadCoords(j, quadpts[j][0], quadpts[j][1], quadpts[j][2]);
	}
        double values[8*3];	// the values of the corner points of this cell
        double values2[8*3];
	double valuetemp[8];
	double safepoint[3];
	const LeafInfo* li=leaves[i]->leafinfo;
	int whichchild=-1;	// which child of our parent are we
	for (int j=0;j<8;++j)
	{
	    // now we need to collate the values at the corner points
	    if (li->pmap[j]<2)	// hanging node so we will need to interpolate
	    {
	        // We know that if a node is hanging, then there must be at least one face
		// touching that edge which is less refined.
		// This means that nothing else touching this edge can be further refined.
		// based on this, if we know which child we are, then we can find the opposite
		// corner which we can use to interpolate with
		
		// to understand this code please see the hangskip table in FaceConsts.h
		if (whichchild==-1)   // we don't know which child we are yet
		{
		    whichchild=leaves[i]->whichChild();
		    const double* src=const_cast<escript::Data&>(arg).getSampleDataRO(li->pmap[whichchild]-2);
		    safepoint[0]=src[0];	// any hanging nodes in this cell can use the same
		    safepoint[1]=src[1];	// safepoint as the start of interpolation
		    safepoint[2]=src[2];
		}
		// The other end of the interpolation line matches the number of the hanging node
		const double* src=const_cast<escript::Data&>(arg).getSampleDataRO(leaves[i]->parent->kids[j]->leafinfo->pmap[j]-2);
		values[j*3+0]=(safepoint[0]+src[0])/2;
		values[j*3+1]=(safepoint[1]+src[1])/2;
		values[j*3+2]=(safepoint[2]+src[2])/2;
	    }
	    else
	    {
	        // for ctsfn each point is a sample
		const double* src=const_cast<escript::Data&>(arg).getSampleDataRO(li->pmap[j]-2);
	        values[j*3+0]=src[0];
		values[j*3+1]=src[1];
	        values[j*3+2]=src[2];
		// sort out that const_cast situation.
		// Why isn't getSampleDataRO  const?
		// Do I need to be more agressive with mutables?
		// Finley dodges this issue by asking for DataC and using that --- which is a constant operation
	    }
	}
	
	valuetemp[0]=values[0];
	valuetemp[1]=values[1];
	valuetemp[2]=values[2];
	
	// now we have all the values we can interpolate
        
	interpolateElementFromCtsToDisc(li, 3, values, values2);
        // ok so now values2 has the vectors for each of the points in it
	// first we want to translate so that the value from point_0 is at the origin	
	for (int j=0;j<8;++j)
	{
	    values2[j*3]-=valuetemp[0];
	    values2[j*3+1]-=valuetemp[1];
	    values2[j*3+2]-=valuetemp[2];  
	}
	// still need to compute each matrix

        double dx=leaves[i]->sides[0]/4;
	double dy=leaves[i]->sides[1]/4;
	double dz=leaves[i]->sides[2]/4;
	using namespace escript::DataTypes;
	
	for (int j=0; j<8;++j)
	{
	    double lx=dx, ly=dy, lz=dz;
	    if (j>3){lz*=3;}
	    if (j==1 || j==2 || j==5 || j==6) {lx*=3;}
	    if (j==2 || j==3 || j==6 || j==7) {ly*=3;}
	    
	    // is an element with a kink in it a problem here?
	    
	    dest[getRelIndex(MAT3X3, 0, 0)]=values2[j*3+0]/lx;
	    dest[getRelIndex(MAT3X3, 0, 1)]=values2[j*3+0]/ly;
	    dest[getRelIndex(MAT3X3, 0, 2)]=values2[j*3+0]/lz;
	    dest[getRelIndex(MAT3X3, 1, 0)]=values2[j*3+1]/lx;
	    dest[getRelIndex(MAT3X3, 1, 1)]=values2[j*3+1]/ly;
	    dest[getRelIndex(MAT3X3, 1, 2)]=values2[j*3+1]/lz;
	    dest[getRelIndex(MAT3X3, 2, 0)]=values2[j*3+2]/lx;
	    dest[getRelIndex(MAT3X3, 2, 1)]=values2[j*3+2]/ly;
	    dest[getRelIndex(MAT3X3, 2, 2)]=values2[j*3+2]/lz;
	    dest+=9;
	}

//	throw BuckleyException("This still does not poopulate the arrays corresponding to each point.");
	

//	dest+=3*3*8;		// move on to next element
    }
    
    
}

#endif




// ------------------------------------------------------------------------------------------------------------
// Functions copied/adapted from Cihan's Ripley::Brick
// -------------------------------------------------------------------------------------------------------------


#define GETHANGSAMPLE(LEAF, KID, VNAME) const register double* VNAME; if (li->pmap[KID]<2) {\
VNAME=buffer+buffcounter;\
if (whichchild<0) {whichchild=LEAF.whichChild();\
src1=const_cast<escript::Data&>(in).getSampleDataRO(li->pmap[whichchild]-2);}\
const double* src2=const_cast<escript::Data&>(in).getSampleDataRO(LEAF.parent->kids[KID]->leafinfo->pmap[KID]-2);\
for (int k=0;k<numComp;++k)\
{\
    buffer[buffcounter++]=(src1[k]+src2[k])/2;\
}\
} else {VNAME=const_cast<escript::Data&>(in).getSampleDataRO(li->pmap[KID]-2);}


//protected
void BuckleyDomain::setToGradient(escript::Data& out, const escript::Data& in) const
{
  
    // need a whole bunch of sanity checks about stale functionspaces etc
  
  
    const dim_t numComp = in.getDataPointSize();

    const double C0 = .044658198738520451079;
    const double C1 = .16666666666666666667;
    const double C2 = .21132486540518711775;
    const double C3 = .25;
    const double C4 = .5;
    const double C5 = .62200846792814621559;
    const double C6 = .78867513459481288225;

    double* buffer=new double[numComp*8];	// we will never have 8 hanging nodes    
    out.requireWrite();
    if (out.getFunctionSpace().getTypeCode() == discfn)
    {
      
      // now we need to walk
      
      
      
//      escript::DataTypes::ValueType::ValueType vec=(&in.getDataPointRO(0, 0));       
      int k;	// one day OMP-3 will be default
// Why isn't this parallelised??
// well it isn't threadsafe the way it is written
// up to 8 cells could touch a point and try to write its coords
// This can be fixed but I haven't done it yet
//       #pragma omp parallel for private(i) schedule(static)


      // this should be inside the omp parallel region
      double* buffer=new double[numComp*8];	// we will never have 8 hanging nodes   
      index_t limit=ot.leafCount();
      for (k=0;k<limit;++k)
      {
	  const OctCell& oc=*leaves[k];
	  const LeafInfo* li=oc.leafinfo;
	  const double h0 = oc.sides[0];
	  const double h1 = oc.sides[1];
	  const double h2 = oc.sides[2];
	  
          int buffcounter=0;
	  const double* src1=0;
	  int whichchild=-1;	    	  

	  // This mapping links f_xyz to the vertex with those coords on a 1x1x1 cube
	  GETHANGSAMPLE(oc, 0, f_000)
	  GETHANGSAMPLE(oc, 4, f_001)
	  GETHANGSAMPLE(oc, 5, f_101)
	  GETHANGSAMPLE(oc, 6, f_111)
	  GETHANGSAMPLE(oc, 2, f_110)
	  GETHANGSAMPLE(oc, 7, f_011)
	  GETHANGSAMPLE(oc, 3, f_010)
	  GETHANGSAMPLE(oc, 1, f_100)
/*	  
cout << "f_000: " << f_000[0] << ',' << f_000[1] << f_000[2] << endl;	  
cout << "f_001: " << f_001[0] << ',' << f_001[1] << f_001[2] << endl;
cout << "f_010: " << f_010[0] << ',' << f_010[1] << f_010[2] << endl;
cout << "f_011: " << f_011[0] << ',' << f_011[1] << f_011[2] << endl;
cout << "f_100: " << f_100[0] << ',' << f_100[1] << f_100[2] << endl;
cout << "f_101: " << f_101[0] << ',' << f_101[1] << f_101[2] << endl;
cout << "f_110: " << f_110[0] << ',' << f_110[1] << f_110[2] << endl;
cout << "f_111: " << f_111[0] << ',' << f_111[1] << f_111[2] << endl;*/
	  
	  double* o=out.getSampleDataRW(k);	// a single datapoint to represent the whole cell
	  
	  for (index_t i=0; i < numComp; ++i) {
	      const double V0=((f_100[i]-f_000[i])*C5 + (f_111[i]-f_011[i])*C0 + (f_101[i]+f_110[i]-f_001[i]-f_010[i])*C1) / h0;
	      const double V1=((f_110[i]-f_010[i])*C5 + (f_101[i]-f_001[i])*C0 + (f_100[i]+f_111[i]-f_000[i]-f_011[i])*C1) / h0;
	      const double V2=((f_101[i]-f_001[i])*C5 + (f_110[i]-f_010[i])*C0 + (f_100[i]+f_111[i]-f_000[i]-f_011[i])*C1) / h0;
	      const double V3=((f_111[i]-f_011[i])*C5 + (f_100[i]-f_000[i])*C0 + (f_101[i]+f_110[i]-f_001[i]-f_010[i])*C1) / h0;
	      const double V4=((f_010[i]-f_000[i])*C5 + (f_111[i]-f_101[i])*C0 + (f_011[i]+f_110[i]-f_001[i]-f_100[i])*C1) / h1;
	      const double V5=((f_110[i]-f_100[i])*C5 + (f_011[i]-f_001[i])*C0 + (f_010[i]+f_111[i]-f_000[i]-f_101[i])*C1) / h1;
	      const double V6=((f_011[i]-f_001[i])*C5 + (f_110[i]-f_100[i])*C0 + (f_010[i]+f_111[i]-f_000[i]-f_101[i])*C1) / h1;
	      const double V7=((f_111[i]-f_101[i])*C5 + (f_010[i]-f_000[i])*C0 + (f_011[i]+f_110[i]-f_001[i]-f_100[i])*C1) / h1;
	      const double V8=((f_001[i]-f_000[i])*C5 + (f_111[i]-f_110[i])*C0 + (f_011[i]+f_101[i]-f_010[i]-f_100[i])*C1) / h2;
	      const double V9=((f_101[i]-f_100[i])*C5 + (f_011[i]-f_010[i])*C0 + (f_001[i]+f_111[i]-f_000[i]-f_110[i])*C1) / h2;
	      const double V10=((f_011[i]-f_010[i])*C5 + (f_101[i]-f_100[i])*C0 + (f_001[i]+f_111[i]-f_000[i]-f_110[i])*C1) / h2;
	      const double V11=((f_111[i]-f_110[i])*C5 + (f_001[i]-f_000[i])*C0 + (f_011[i]+f_101[i]-f_010[i]-f_100[i])*C1) / h2;
	      o[INDEX3(i,0,0,numComp,3)] = V0;
	      o[INDEX3(i,1,0,numComp,3)] = V4;
	      o[INDEX3(i,2,0,numComp,3)] = V8;
	      o[INDEX3(i,0,1,numComp,3)] = V0;
	      o[INDEX3(i,1,1,numComp,3)] = V5;
	      o[INDEX3(i,2,1,numComp,3)] = V9;
	      o[INDEX3(i,0,2,numComp,3)] = V1;
	      o[INDEX3(i,1,2,numComp,3)] = V4;
	      o[INDEX3(i,2,2,numComp,3)] = V10;
	      o[INDEX3(i,0,3,numComp,3)] = V1;
	      o[INDEX3(i,1,3,numComp,3)] = V5;
	      o[INDEX3(i,2,3,numComp,3)] = V11;
	      o[INDEX3(i,0,4,numComp,3)] = V2;
	      o[INDEX3(i,1,4,numComp,3)] = V6;
	      o[INDEX3(i,2,4,numComp,3)] = V8;
	      o[INDEX3(i,0,5,numComp,3)] = V2;
	      o[INDEX3(i,1,5,numComp,3)] = V7;
	      o[INDEX3(i,2,5,numComp,3)] = V9;
	      o[INDEX3(i,0,6,numComp,3)] = V3;
	      o[INDEX3(i,1,6,numComp,3)] = V6;
	      o[INDEX3(i,2,6,numComp,3)] = V10;
	      o[INDEX3(i,0,7,numComp,3)] = V3;
	      o[INDEX3(i,1,7,numComp,3)] = V7;
	      o[INDEX3(i,2,7,numComp,3)] = V11;
	  } // end of component loop i	  
	  
      }      

    } else if (out.getFunctionSpace().getTypeCode() == red_discfn) {
      throw BuckleyException("We don't support gradient on that functionspace.");
      int k;
// #pragma omp parallel for
      for (k=0;k<ot.leafCount();++k)
      {

	  const OctCell& oc=*leaves[k];
	  const LeafInfo* li=oc.leafinfo;
	  const double h0 = oc.sides[0]/2;
	  const double h1 = oc.sides[1]/2;
	  const double h2 = oc.sides[2]/2;
	  
          int buffcounter=0;
	  const double* src1=0;
	  int whichchild=-1;	    	  
	  
	  GETHANGSAMPLE(oc, 0, f_000)
	  GETHANGSAMPLE(oc, 4, f_001)
	  GETHANGSAMPLE(oc, 5, f_101)
	  GETHANGSAMPLE(oc, 7, f_111)
	  GETHANGSAMPLE(oc, 3, f_110)
	  GETHANGSAMPLE(oc, 6, f_011)
	  GETHANGSAMPLE(oc, 2, f_010)
	  GETHANGSAMPLE(oc, 1, f_100)
	  double* o=out.getSampleDataRW(k);	// a single datapoint to represent the whole cell
	  for (index_t i=0; i < numComp; ++i) {
	      o[INDEX3(i,0,0,numComp,3)] = (f_100[i]+f_101[i]+f_110[i]+f_111[i]-f_000[i]-f_001[i]-f_010[i]-f_011[i])*C3 / h0;
	      o[INDEX3(i,1,0,numComp,3)] = (f_010[i]+f_011[i]+f_110[i]+f_111[i]-f_000[i]-f_001[i]-f_100[i]-f_101[i])*C3 / h1;
	      o[INDEX3(i,2,0,numComp,3)] = (f_001[i]+f_011[i]+f_101[i]+f_111[i]-f_000[i]-f_010[i]-f_100[i]-f_110[i])*C3 / h2;
	  } // end of component loop i
      }
    } else if (out.getFunctionSpace().getTypeCode() == disc_faces) {
// #pragma omp parallel
        {
	  
	  
	  
	    int partials[6];
	    partials[0]=0;
	    for (int j=1;j<6;++j)
	    {
		partials[j]=partials[j-1]+face_cells[j].size();
	    }
	  
            if (!face_cells[0].empty()) {
	        index_t limit=face_cells[0].size();
// #pragma omp for nowait
		for (index_t k=0; k<limit;++k) {
		    const OctCell& oc=*(face_cells[0][k]);
		    const LeafInfo* li=oc.leafinfo;
		    const double h0 = oc.sides[0];
		    const double h1 = oc.sides[1];
		    const double h2 = oc.sides[2];
		    
		    int buffcounter=0;
		    const double* src1=0;
		    int whichchild=-1;	    			    
		    
		    // This order is the same as specified in the original code
		    // I don't know if my ordering is the same as the one Lutz used to
		    // generate this
		    
		    GETHANGSAMPLE(oc, 0, f_000)
		    GETHANGSAMPLE(oc, 4, f_001)
		    GETHANGSAMPLE(oc, 5, f_101)
		    GETHANGSAMPLE(oc, 6, f_111)
		    GETHANGSAMPLE(oc, 2, f_110)
		    GETHANGSAMPLE(oc, 7, f_011)
		    GETHANGSAMPLE(oc, 3, f_010)
		    GETHANGSAMPLE(oc, 1, f_100)		    
		    
	  
cout << "f_000: " << f_000[0] << ',' << f_000[1] << f_000[2] << endl;	  
cout << "f_001: " << f_001[0] << ',' << f_001[1] << f_001[2] << endl;
cout << "f_010: " << f_010[0] << ',' << f_010[1] << f_010[2] << endl;
cout << "f_011: " << f_011[0] << ',' << f_011[1] << f_011[2] << endl;
cout << "f_100: " << f_100[0] << ',' << f_100[1] << f_100[2] << endl;
cout << "f_101: " << f_101[0] << ',' << f_101[1] << f_101[2] << endl;
cout << "f_110: " << f_110[0] << ',' << f_110[1] << f_110[2] << endl;
cout << "f_111: " << f_111[0] << ',' << f_111[1] << f_111[2] << endl;		    
		    
		    
		    
		    
		    double* o=out.getSampleDataRW(partials[0]+k);
		    for (index_t i=0; i < numComp; ++i) {
			const double V0=((f_010[i]-f_000[i])*C6 + (f_011[i]-f_001[i])*C2) / h1;
			const double V1=((f_010[i]-f_000[i])*C2 + (f_011[i]-f_001[i])*C6) / h1;
			const double V2=((f_001[i]-f_000[i])*C6 + (f_010[i]-f_011[i])*C2) / h2;
			const double V3=((f_001[i]-f_000[i])*C2 + (f_011[i]-f_010[i])*C6) / h2;
			o[INDEX3(i,0,0,numComp,3)] = ((f_100[i]-f_000[i])*C5 + (f_111[i]-f_011[i])*C0 + (f_101[i]+f_110[i]-f_001[i]-f_010[i])*C1) / h0;
			o[INDEX3(i,1,0,numComp,3)] = V0;
			o[INDEX3(i,2,0,numComp,3)] = V2;
			o[INDEX3(i,0,1,numComp,3)] = ((f_110[i]-f_010[i])*C5 + (f_101[i]-f_001[i])*C0 + (f_100[i]+f_111[i]-f_000[i]-f_011[i])*C1) / h0;
			o[INDEX3(i,1,1,numComp,3)] = V0;
			o[INDEX3(i,2,1,numComp,3)] = V3;
			o[INDEX3(i,0,2,numComp,3)] = ((f_101[i]-f_001[i])*C5 + (f_110[i]-f_010[i])*C0 + (f_100[i]+f_111[i]-f_000[i]-f_011[i])*C1) / h0;
			o[INDEX3(i,1,2,numComp,3)] = V1;
			o[INDEX3(i,2,2,numComp,3)] = V2;
			o[INDEX3(i,0,3,numComp,3)] = ((f_111[i]-f_011[i])*C5 + (f_100[i]-f_000[i])*C0 + (f_101[i]+f_110[i]-f_001[i]-f_010[i])*C1) / h0;
			o[INDEX3(i,1,3,numComp,3)] = V1;
			o[INDEX3(i,2,3,numComp,3)] = V3;
		    } // end of component loop i
                } // end of k loop
            } // end of face 0
            if (!face_cells[1].empty()) {
		// This is the bottom face
	        index_t limit=face_cells[1].size();		
#pragma omp for nowait
		for (index_t k=0; k<limit;++k) {
		    const OctCell& oc=*(face_cells[1][k]);
		    const LeafInfo* li=oc.leafinfo;
		    const double h0 = oc.sides[0];
		    const double h1 = oc.sides[1];
		    const double h2 = oc.sides[2];
		    
		    int buffcounter=0;
		    const double* src1=0;
		    int whichchild=-1;	    			    
		    
		    // The following node ordering does not take into account the face we are looking at
		    // and so is wrong
		    // I'm just doing this so I can get the code to the point of testing
		    GETHANGSAMPLE(oc, 0, f_000)
		    GETHANGSAMPLE(oc, 4, f_001)
		    GETHANGSAMPLE(oc, 5, f_101)
		    GETHANGSAMPLE(oc, 7, f_111)
		    GETHANGSAMPLE(oc, 3, f_110)
		    GETHANGSAMPLE(oc, 6, f_011)
		    GETHANGSAMPLE(oc, 2, f_010)
		    GETHANGSAMPLE(oc, 1, f_100)			    
		    

//                         const double* f_000 = in.getSampleDataRO(INDEX3(m_N0-2,k1,k2, m_N0,m_N1));
//                         const double* f_001 = in.getSampleDataRO(INDEX3(m_N0-2,k1,k2+1, m_N0,m_N1));
//                         const double* f_101 = in.getSampleDataRO(INDEX3(m_N0-1,k1,k2+1, m_N0,m_N1));
//                         const double* f_111 = in.getSampleDataRO(INDEX3(m_N0-1,k1+1,k2+1, m_N0,m_N1));
//                         const double* f_110 = in.getSampleDataRO(INDEX3(m_N0-1,k1+1,k2, m_N0,m_N1));
//                         const double* f_011 = in.getSampleDataRO(INDEX3(m_N0-2,k1+1,k2+1, m_N0,m_N1));
//                         const double* f_010 = in.getSampleDataRO(INDEX3(m_N0-2,k1+1,k2, m_N0,m_N1));
//                         const double* f_100 = in.getSampleDataRO(INDEX3(m_N0-1,k1,k2, m_N0,m_N1));
		    double* o=out.getSampleDataRW(partials[1]+k);
// 		    double* o = out.getSampleDataRW(m_faceOffset[1]+INDEX2(k1,k2,m_NE1));
		    for (index_t i=0; i < numComp; ++i) {
			const double V0=((f_110[i]-f_100[i])*C6 + (f_111[i]-f_101[i])*C2) / h1;
			const double V1=((f_110[i]-f_100[i])*C2 + (f_111[i]-f_101[i])*C6) / h1;
			const double V2=((f_101[i]-f_100[i])*C6 + (f_111[i]-f_110[i])*C2) / h2;
			const double V3=((f_101[i]-f_100[i])*C2 + (f_111[i]-f_110[i])*C6) / h2;
			o[INDEX3(i,0,0,numComp,3)] = ((f_100[i]-f_000[i])*C5 + (f_111[i]-f_011[i])*C0 + (f_101[i]+f_110[i]-f_001[i]-f_010[i])*C1) / h0;
			o[INDEX3(i,1,0,numComp,3)] = V0;
			o[INDEX3(i,2,0,numComp,3)] = V2;
			o[INDEX3(i,0,1,numComp,3)] = ((f_110[i]-f_010[i])*C5 + (f_101[i]-f_001[i])*C0 + (f_100[i]+f_111[i]-f_000[i]-f_011[i])*C1) / h0;
			o[INDEX3(i,1,1,numComp,3)] = V0;
			o[INDEX3(i,2,1,numComp,3)] = V3;
			o[INDEX3(i,0,2,numComp,3)] = ((f_101[i]-f_001[i])*C5 + (f_110[i]-f_010[i])*C0 + (f_100[i]+f_111[i]-f_000[i]-f_011[i])*C1) / h0;
			o[INDEX3(i,1,2,numComp,3)] = V1;
			o[INDEX3(i,2,2,numComp,3)] = V2;
			o[INDEX3(i,0,3,numComp,3)] = ((f_111[i]-f_011[i])*C5 + (f_100[i]-f_000[i])*C0 + (f_101[i]+f_110[i]-f_001[i]-f_010[i])*C1) / h0;
			o[INDEX3(i,1,3,numComp,3)] = V1;
			o[INDEX3(i,2,3,numComp,3)] = V3;
		    } // end of component loop i
		}
            } // end of face 1
            if (!face_cells[2].empty()) {
	        index_t limit=face_cells[2].size();	      
#pragma omp for nowait
		for (index_t k=0; k<limit;++k) {
		    const OctCell& oc=*(face_cells[2][k]);
		    const LeafInfo* li=oc.leafinfo;
		    const double h0 = oc.sides[0];
		    const double h1 = oc.sides[1];
		    const double h2 = oc.sides[2];
		    
		    int buffcounter=0;
		    const double* src1=0;
		    int whichchild=-1;	    			    
		    
		    // The following node ordering does not take into account the face we are looking at
		    // and so is wrong
		    // I'm just doing this so I can get the code to the point of testing
		    GETHANGSAMPLE(oc, 0, f_000)
		    GETHANGSAMPLE(oc, 4, f_001)
		    GETHANGSAMPLE(oc, 5, f_101)
		    GETHANGSAMPLE(oc, 7, f_111)
		    GETHANGSAMPLE(oc, 3, f_110)
		    GETHANGSAMPLE(oc, 6, f_011)
		    GETHANGSAMPLE(oc, 2, f_010)
		    GETHANGSAMPLE(oc, 1, f_100)
		    double* o=out.getSampleDataRW(partials[2]+k);

/*                        const double* f_000 = in.getSampleDataRO(INDEX3(k0,0,k2, m_N0,m_N1));
                        const double* f_001 = in.getSampleDataRO(INDEX3(k0,0,k2+1, m_N0,m_N1));
                        const double* f_101 = in.getSampleDataRO(INDEX3(k0+1,0,k2+1, m_N0,m_N1));
                        const double* f_100 = in.getSampleDataRO(INDEX3(k0+1,0,k2, m_N0,m_N1));
                        const double* f_111 = in.getSampleDataRO(INDEX3(k0+1,1,k2+1, m_N0,m_N1));
                        const double* f_110 = in.getSampleDataRO(INDEX3(k0+1,1,k2, m_N0,m_N1));
                        const double* f_011 = in.getSampleDataRO(INDEX3(k0,1,k2+1, m_N0,m_N1));
                        const double* f_010 = in.getSampleDataRO(INDEX3(k0,1,k2, m_N0,m_N1));
                        double* o = out.getSampleDataRW(m_faceOffset[2]+INDEX2(k0,k2,m_NE0));*/
                        for (index_t i=0; i < numComp; ++i) {
                            const double V0=((f_100[i]-f_000[i])*C6 + (f_101[i]-f_001[i])*C2) / h0;
                            const double V1=((f_001[i]-f_000[i])*C6 + (f_101[i]-f_100[i])*C2) / h2;
                            const double V2=((f_001[i]-f_000[i])*C2 + (f_101[i]-f_100[i])*C6) / h2;
                            o[INDEX3(i,0,0,numComp,3)] = V0;
                            o[INDEX3(i,1,0,numComp,3)] = ((f_010[i]-f_000[i])*C5 + (f_111[i]-f_101[i])*C0 + (f_011[i]+f_110[i]-f_001[i]-f_100[i])*C1) / h1;
                            o[INDEX3(i,2,0,numComp,3)] = V1;
                            o[INDEX3(i,0,1,numComp,3)] = V0;
                            o[INDEX3(i,1,1,numComp,3)] = ((f_110[i]-f_100[i])*C5 + (f_011[i]-f_001[i])*C0 + (f_010[i]+f_111[i]-f_000[i]-f_101[i])*C1) / h1;
                            o[INDEX3(i,2,1,numComp,3)] = V2;
                            o[INDEX3(i,0,2,numComp,3)] = V0;
                            o[INDEX3(i,1,2,numComp,3)] = ((f_011[i]-f_001[i])*C5 + (f_110[i]-f_100[i])*C0 + (f_010[i]+f_111[i]-f_000[i]-f_101[i])*C1) / h1;
                            o[INDEX3(i,2,2,numComp,3)] = V1;
                            o[INDEX3(i,0,3,numComp,3)] = V0;
                            o[INDEX3(i,1,3,numComp,3)] = ((f_111[i]-f_101[i])*C5 + (f_010[i]-f_000[i])*C0 + (f_011[i]+f_110[i]-f_001[i]-f_100[i])*C1) / h1;
                            o[INDEX3(i,2,3,numComp,3)] = V2;
                        } // end of component loop i
                    } // end of k0 loop
            } // end of face 2
            if (!face_cells[3].empty()) {
	      	index_t limit=face_cells[3].size();	      
#pragma omp for nowait
		for (index_t k=0; k<limit;++k) {
		    const OctCell& oc=*(face_cells[3][k]);
		    const LeafInfo* li=oc.leafinfo;
		    const double h0 = oc.sides[0];
		    const double h1 = oc.sides[1];
		    const double h2 = oc.sides[2];
		    
		    int buffcounter=0;
		    const double* src1=0;
		    int whichchild=-1;	    			    
		    
		    // The following node ordering does not take into account the face we are looking at
		    // and so is wrong
		    // I'm just doing this so I can get the code to the point of testing
		    GETHANGSAMPLE(oc, 0, f_000)
		    GETHANGSAMPLE(oc, 4, f_001)
		    GETHANGSAMPLE(oc, 5, f_101)
		    GETHANGSAMPLE(oc, 7, f_111)
		    GETHANGSAMPLE(oc, 3, f_110)
		    GETHANGSAMPLE(oc, 6, f_011)
		    GETHANGSAMPLE(oc, 2, f_010)
		    GETHANGSAMPLE(oc, 1, f_100)
		    double* o=out.getSampleDataRW(partials[3]+k);

//                         const double* f_011 = in.getSampleDataRO(INDEX3(k0,m_N1-1,k2+1, m_N0,m_N1));
//                         const double* f_110 = in.getSampleDataRO(INDEX3(k0+1,m_N1-1,k2, m_N0,m_N1));
//                         const double* f_010 = in.getSampleDataRO(INDEX3(k0,m_N1-1,k2, m_N0,m_N1));
//                         const double* f_111 = in.getSampleDataRO(INDEX3(k0+1,m_N1-1,k2+1, m_N0,m_N1));
//                         const double* f_000 = in.getSampleDataRO(INDEX3(k0,m_N1-2,k2, m_N0,m_N1));
//                         const double* f_001 = in.getSampleDataRO(INDEX3(k0,m_N1-2,k2+1, m_N0,m_N1));
//                         const double* f_101 = in.getSampleDataRO(INDEX3(k0+1,m_N1-2,k2+1, m_N0,m_N1));
//                         const double* f_100 = in.getSampleDataRO(INDEX3(k0+1,m_N1-2,k2, m_N0,m_N1));
//                         double* o = out.getSampleDataRW(m_faceOffset[3]+INDEX2(k0,k2,m_NE0));
                        for (index_t i=0; i < numComp; ++i) {
                            const double V0=((f_110[i]-f_010[i])*C6 + (f_111[i]-f_011[i])*C2) / h0;
                            const double V1=((f_110[i]-f_010[i])*C2 + (f_111[i]-f_011[i])*C6) / h0;
                            const double V2=((f_011[i]-f_010[i])*C6 + (f_111[i]-f_110[i])*C2) / h2;
                            const double V3=((f_011[i]-f_010[i])*C2 + (f_111[i]-f_110[i])*C6) / h2;
                            o[INDEX3(i,0,0,numComp,3)] = V0;
                            o[INDEX3(i,1,0,numComp,3)] = ((f_010[i]-f_000[i])*C5 + (f_111[i]-f_101[i])*C0 + (f_011[i]+f_110[i]-f_001[i]-f_100[i])*C1) / h1;
                            o[INDEX3(i,2,0,numComp,3)] = V2;
                            o[INDEX3(i,0,1,numComp,3)] = V0;
                            o[INDEX3(i,1,1,numComp,3)] = ((f_110[i]-f_100[i])*C5 + (f_011[i]-f_001[i])*C0 + (f_010[i]+f_111[i]-f_000[i]-f_101[i])*C1) / h1;
                            o[INDEX3(i,2,1,numComp,3)] = V3;
                            o[INDEX3(i,0,2,numComp,3)] = V1;
                            o[INDEX3(i,1,2,numComp,3)] = ((f_011[i]-f_001[i])*C5 + (f_110[i]-f_100[i])*C0 + (f_010[i]+f_111[i]-f_000[i]-f_101[i])*C1) / h1;
                            o[INDEX3(i,2,2,numComp,3)] = V2;
                            o[INDEX3(i,0,3,numComp,3)] = V1;
                            o[INDEX3(i,1,3,numComp,3)] = ((f_111[i]-f_101[i])*C5 + (f_010[i]-f_000[i])*C0 + (f_011[i]+f_110[i]-f_001[i]-f_100[i])*C1) / h1;
                            o[INDEX3(i,2,3,numComp,3)] = V3;
                        } // end of component loop i
		}
            } // end of face 3
            if (!face_cells[4].empty()) {
	      	index_t limit=face_cells[4].size();	      
#pragma omp for nowait
		for (index_t k=0; k<limit;++k) {
		    const OctCell& oc=*(face_cells[4][k]);
		    const LeafInfo* li=oc.leafinfo;
		    const double h0 = oc.sides[0];
		    const double h1 = oc.sides[1];
		    const double h2 = oc.sides[2];
		    
		    int buffcounter=0;
		    const double* src1=0;
		    int whichchild=-1;	    			    
		    
		    // The following node ordering does not take into account the face we are looking at
		    // and so is wrong
		    // I'm just doing this so I can get the code to the point of testing
		    GETHANGSAMPLE(oc, 0, f_000)
		    GETHANGSAMPLE(oc, 4, f_001)
		    GETHANGSAMPLE(oc, 5, f_101)
		    GETHANGSAMPLE(oc, 7, f_111)
		    GETHANGSAMPLE(oc, 3, f_110)
		    GETHANGSAMPLE(oc, 6, f_011)
		    GETHANGSAMPLE(oc, 2, f_010)
		    GETHANGSAMPLE(oc, 1, f_100)
		    double* o=out.getSampleDataRW(partials[4]+k);	      

/*                        const double* f_000 = in.getSampleDataRO(INDEX3(k0,k1,0, m_N0,m_N1));
                        const double* f_100 = in.getSampleDataRO(INDEX3(k0+1,k1,0, m_N0,m_N1));
                        const double* f_110 = in.getSampleDataRO(INDEX3(k0+1,k1+1,0, m_N0,m_N1));
                        const double* f_010 = in.getSampleDataRO(INDEX3(k0,k1+1,0, m_N0,m_N1));
                        const double* f_001 = in.getSampleDataRO(INDEX3(k0,k1,1, m_N0,m_N1));
                        const double* f_101 = in.getSampleDataRO(INDEX3(k0+1,k1,1, m_N0,m_N1));
                        const double* f_011 = in.getSampleDataRO(INDEX3(k0,k1+1,1, m_N0,m_N1));
                        const double* f_111 = in.getSampleDataRO(INDEX3(k0+1,k1+1,1, m_N0,m_N1));
                        double* o = out.getSampleDataRW(m_faceOffset[4]+INDEX2(k0,k1,m_NE0));*/
                        for (index_t i=0; i < numComp; ++i) {
                            const double V0=((f_100[i]-f_000[i])*C6 + (f_110[i]-f_010[i])*C2) / h0;
                            const double V1=((f_100[i]-f_000[i])*C2 + (f_110[i]-f_010[i])*C6) / h0;
                            const double V2=((f_010[i]-f_000[i])*C6 + (f_110[i]-f_100[i])*C2) / h1;
                            const double V3=((f_010[i]-f_000[i])*C2 + (f_110[i]-f_100[i])*C6) / h1;
                            o[INDEX3(i,0,0,numComp,3)] = V0;
                            o[INDEX3(i,1,0,numComp,3)] = V2;
                            o[INDEX3(i,2,0,numComp,3)] = ((f_001[i]-f_000[i])*C5 + (f_111[i]-f_110[i])*C0 + (f_011[i]+f_101[i]-f_010[i]-f_100[i])*C1) / h2;
                            o[INDEX3(i,0,1,numComp,3)] = V0;
                            o[INDEX3(i,1,1,numComp,3)] = V3;
                            o[INDEX3(i,2,1,numComp,3)] = ((f_101[i]-f_100[i])*C5 + (f_011[i]-f_010[i])*C0 + (f_001[i]+f_111[i]-f_000[i]-f_110[i])*C1) / h2;
                            o[INDEX3(i,0,2,numComp,3)] = V1;
                            o[INDEX3(i,1,2,numComp,3)] = V2;
                            o[INDEX3(i,2,2,numComp,3)] = ((f_011[i]-f_010[i])*C5 + (f_101[i]-f_100[i])*C0 + (f_001[i]+f_111[i]-f_000[i]-f_110[i])*C1) / h2;
                            o[INDEX3(i,0,3,numComp,3)] = V1;
                            o[INDEX3(i,1,3,numComp,3)] = V3;
                            o[INDEX3(i,2,3,numComp,3)] = ((f_111[i]-f_110[i])*C5 + (f_001[i]-f_000[i])*C0 + (f_011[i]+f_101[i]-f_010[i]-f_100[i])*C1) / h2;
                        } // end of component loop i
		}
            } // end of face 4
            if (!face_cells[5].empty()) {
	      	index_t limit=face_cells[5].size();	      
#pragma omp for nowait
		for (index_t k=0; k<limit;++k) {
		    const OctCell& oc=*(face_cells[5][k]);
		    const LeafInfo* li=oc.leafinfo;
		    const double h0 = oc.sides[0];
		    const double h1 = oc.sides[1];
		    const double h2 = oc.sides[2];
		    
		    int buffcounter=0;
		    const double* src1=0;
		    int whichchild=-1;	    			    
		    
		    // The following node ordering does not take into account the face we are looking at
		    // and so is wrong
		    // I'm just doing this so I can get the code to the point of testing
		    GETHANGSAMPLE(oc, 0, f_000)
		    GETHANGSAMPLE(oc, 4, f_001)
		    GETHANGSAMPLE(oc, 5, f_101)
		    GETHANGSAMPLE(oc, 7, f_111)
		    GETHANGSAMPLE(oc, 3, f_110)
		    GETHANGSAMPLE(oc, 6, f_011)
		    GETHANGSAMPLE(oc, 2, f_010)
		    GETHANGSAMPLE(oc, 1, f_100)
		    double* o=out.getSampleDataRW(partials[5]+k);	      

//                         const double* f_001 = in.getSampleDataRO(INDEX3(k0,k1,m_N2-1, m_N0,m_N1));
//                         const double* f_101 = in.getSampleDataRO(INDEX3(k0+1,k1,m_N2-1, m_N0,m_N1));
//                         const double* f_011 = in.getSampleDataRO(INDEX3(k0,k1+1,m_N2-1, m_N0,m_N1));
//                         const double* f_111 = in.getSampleDataRO(INDEX3(k0+1,k1+1,m_N2-1, m_N0,m_N1));
//                         const double* f_000 = in.getSampleDataRO(INDEX3(k0,k1,m_N2-2, m_N0,m_N1));
//                         const double* f_110 = in.getSampleDataRO(INDEX3(k0+1,k1+1,m_N2-2, m_N0,m_N1));
//                         const double* f_010 = in.getSampleDataRO(INDEX3(k0,k1+1,m_N2-2, m_N0,m_N1));
//                         const double* f_100 = in.getSampleDataRO(INDEX3(k0+1,k1,m_N2-2, m_N0,m_N1));
//                         double* o = out.getSampleDataRW(m_faceOffset[5]+INDEX2(k0,k1,m_NE0));
                        for (index_t i=0; i < numComp; ++i) {
                            const double V0=((f_101[i]-f_001[i])*C6 + (f_111[i]-f_011[i])*C2) / h0;
                            const double V1=((f_101[i]-f_001[i])*C2 + (f_111[i]-f_011[i])*C6) / h0;
                            const double V2=((f_011[i]-f_001[i])*C6 + (f_111[i]-f_101[i])*C2) / h1;
                            const double V3=((f_011[i]-f_001[i])*C2 + (f_111[i]-f_101[i])*C6) / h1;
                            o[INDEX3(i,0,0,numComp,3)] = V0;
                            o[INDEX3(i,1,0,numComp,3)] = V2;
                            o[INDEX3(i,2,0,numComp,3)] = ((f_001[i]-f_000[i])*C5 + (f_111[i]-f_110[i])*C0 + (f_011[i]+f_101[i]-f_010[i]-f_100[i])*C1) / h2;
                            o[INDEX3(i,0,1,numComp,3)] = V0;
                            o[INDEX3(i,1,1,numComp,3)] = V3;
                            o[INDEX3(i,2,1,numComp,3)] = ((f_011[i]-f_010[i])*C0 + (f_101[i]-f_100[i])*C5 + (f_001[i]+f_111[i]-f_000[i]-f_110[i])*C1) / h2;
                            o[INDEX3(i,0,2,numComp,3)] = V1;
                            o[INDEX3(i,1,2,numComp,3)] = V2;
                            o[INDEX3(i,2,2,numComp,3)] = ((f_011[i]-f_010[i])*C5 + (f_101[i]-f_100[i])*C0 + (f_001[i]+f_111[i]-f_000[i]-f_110[i])*C1) / h2;
                            o[INDEX3(i,0,3,numComp,3)] = V1;
                            o[INDEX3(i,1,3,numComp,3)] = V3;
                            o[INDEX3(i,2,3,numComp,3)] = ((f_001[i]-f_000[i])*C0 + (f_111[i]-f_110[i])*C5 + (f_011[i]+f_101[i]-f_010[i]-f_100[i])*C1) / h2;
                        } // end of component loop i
		}
            } // end of face 5
        } // end of parallel section
    } else if (out.getFunctionSpace().getTypeCode() == red_disc_faces) {
throw BuckleyException("Reduced face elements are not supported");
// // #pragma omp parallel
//          {
//             if (!face_cells[0].empty()) {
// #pragma omp for nowait
//                 for (index_t k2=0; k2 < m_NE2; ++k2) {
//                     for (index_t k1=0; k1 < m_NE1; ++k1) {
//                         const double* f_000 = in.getSampleDataRO(INDEX3(0,k1,k2, m_N0,m_N1));
//                         const double* f_001 = in.getSampleDataRO(INDEX3(0,k1,k2+1, m_N0,m_N1));
//                         const double* f_101 = in.getSampleDataRO(INDEX3(1,k1,k2+1, m_N0,m_N1));
//                         const double* f_011 = in.getSampleDataRO(INDEX3(0,k1+1,k2+1, m_N0,m_N1));
//                         const double* f_100 = in.getSampleDataRO(INDEX3(1,k1,k2, m_N0,m_N1));
//                         const double* f_110 = in.getSampleDataRO(INDEX3(1,k1+1,k2, m_N0,m_N1));
//                         const double* f_010 = in.getSampleDataRO(INDEX3(0,k1+1,k2, m_N0,m_N1));
//                         const double* f_111 = in.getSampleDataRO(INDEX3(1,k1+1,k2+1, m_N0,m_N1));
//                         double* o = out.getSampleDataRW(m_faceOffset[0]+INDEX2(k1,k2,m_NE1));
//                         for (index_t i=0; i < numComp; ++i) {
//                             o[INDEX3(i,0,0,numComp,3)] = (f_100[i]+f_101[i]+f_110[i]+f_111[i]-f_000[i]-f_001[i]-f_010[i]-f_011[i])*C3 / h0;
//                             o[INDEX3(i,1,0,numComp,3)] = (f_010[i]+f_011[i]-f_000[i]-f_001[i])*C4 / h1;
//                             o[INDEX3(i,2,0,numComp,3)] = (f_001[i]+f_011[i]-f_000[i]-f_010[i])*C4 / h2;
//                         } // end of component loop i
//                     } // end of k1 loop
//                 } // end of k2 loop
//             } // end of face 0
//             if (!face_cells[1].empty()) {
// #pragma omp for nowait
//                 for (index_t k2=0; k2 < m_NE2; ++k2) {
//                     for (index_t k1=0; k1 < m_NE1; ++k1) {
//                         const double* f_000 = in.getSampleDataRO(INDEX3(m_N0-2,k1,k2, m_N0,m_N1));
//                         const double* f_001 = in.getSampleDataRO(INDEX3(m_N0-2,k1,k2+1, m_N0,m_N1));
//                         const double* f_101 = in.getSampleDataRO(INDEX3(m_N0-1,k1,k2+1, m_N0,m_N1));
//                         const double* f_011 = in.getSampleDataRO(INDEX3(m_N0-2,k1+1,k2+1, m_N0,m_N1));
//                         const double* f_100 = in.getSampleDataRO(INDEX3(m_N0-1,k1,k2, m_N0,m_N1));
//                         const double* f_110 = in.getSampleDataRO(INDEX3(m_N0-1,k1+1,k2, m_N0,m_N1));
//                         const double* f_010 = in.getSampleDataRO(INDEX3(m_N0-2,k1+1,k2, m_N0,m_N1));
//                         const double* f_111 = in.getSampleDataRO(INDEX3(m_N0-1,k1+1,k2+1, m_N0,m_N1));
//                         double* o = out.getSampleDataRW(m_faceOffset[1]+INDEX2(k1,k2,m_NE1));
//                         for (index_t i=0; i < numComp; ++i) {
//                             o[INDEX3(i,0,0,numComp,3)] = (f_100[i]+f_101[i]+f_110[i]+f_111[i]-f_000[i]-f_001[i]-f_010[i]-f_011[i])*C3 / h0;
//                             o[INDEX3(i,1,0,numComp,3)] = (f_110[i]+f_111[i]-f_100[i]-f_101[i])*C4 / h1;
//                             o[INDEX3(i,2,0,numComp,3)] = (f_101[i]+f_111[i]-f_100[i]-f_110[i])*C4 / h2;
//                         } // end of component loop i
//                     } // end of k1 loop
//                 } // end of k2 loop
//             } // end of face 1
//             if (!face_cells[2].empty()) {
// #pragma omp for nowait
//                 for (index_t k2=0; k2 < m_NE2; ++k2) {
//                     for (index_t k0=0; k0 < m_NE0; ++k0) {
//                         const double* f_000 = in.getSampleDataRO(INDEX3(k0,0,k2, m_N0,m_N1));
//                         const double* f_001 = in.getSampleDataRO(INDEX3(k0,0,k2+1, m_N0,m_N1));
//                         const double* f_101 = in.getSampleDataRO(INDEX3(k0+1,0,k2+1, m_N0,m_N1));
//                         const double* f_100 = in.getSampleDataRO(INDEX3(k0+1,0,k2, m_N0,m_N1));
//                         const double* f_011 = in.getSampleDataRO(INDEX3(k0,1,k2+1, m_N0,m_N1));
//                         const double* f_110 = in.getSampleDataRO(INDEX3(k0+1,1,k2, m_N0,m_N1));
//                         const double* f_010 = in.getSampleDataRO(INDEX3(k0,1,k2, m_N0,m_N1));
//                         const double* f_111 = in.getSampleDataRO(INDEX3(k0+1,1,k2+1, m_N0,m_N1));
//                         double* o = out.getSampleDataRW(m_faceOffset[2]+INDEX2(k0,k2,m_NE0));
//                         for (index_t i=0; i < numComp; ++i) {
//                             o[INDEX3(i,0,0,numComp,3)] = (f_100[i]+f_101[i]-f_000[i]-f_001[i])*C4 / h0;
//                             o[INDEX3(i,1,0,numComp,3)] = (f_010[i]+f_011[i]+f_110[i]+f_111[i]-f_000[i]-f_001[i]-f_100[i]-f_101[i])*C3 / h1;
//                             o[INDEX3(i,2,0,numComp,3)] = (f_001[i]+f_101[i]-f_000[i]-f_100[i])*C4 / h2;
//                         } // end of component loop i
//                     } // end of k0 loop
//                 } // end of k2 loop
//             } // end of face 2
//             if (!face_cells[3].empty()) {
// #pragma omp for nowait
//                 for (index_t k2=0; k2 < m_NE2; ++k2) {
//                     for (index_t k0=0; k0 < m_NE0; ++k0) {
//                         const double* f_011 = in.getSampleDataRO(INDEX3(k0,m_N1-1,k2+1, m_N0,m_N1));
//                         const double* f_110 = in.getSampleDataRO(INDEX3(k0+1,m_N1-1,k2, m_N0,m_N1));
//                         const double* f_010 = in.getSampleDataRO(INDEX3(k0,m_N1-1,k2, m_N0,m_N1));
//                         const double* f_111 = in.getSampleDataRO(INDEX3(k0+1,m_N1-1,k2+1, m_N0,m_N1));
//                         const double* f_000 = in.getSampleDataRO(INDEX3(k0,m_N1-2,k2, m_N0,m_N1));
//                         const double* f_101 = in.getSampleDataRO(INDEX3(k0+1,m_N1-2,k2+1, m_N0,m_N1));
//                         const double* f_001 = in.getSampleDataRO(INDEX3(k0,m_N1-2,k2+1, m_N0,m_N1));
//                         const double* f_100 = in.getSampleDataRO(INDEX3(k0+1,m_N1-2,k2, m_N0,m_N1));
//                         double* o = out.getSampleDataRW(m_faceOffset[3]+INDEX2(k0,k2,m_NE0));
//                         for (index_t i=0; i < numComp; ++i) {
//                             o[INDEX3(i,0,0,numComp,3)] = (f_110[i]+f_111[i]-f_010[i]-f_011[i])*C4 / h0;
//                             o[INDEX3(i,1,0,numComp,3)] = (f_010[i]+f_011[i]+f_110[i]+f_111[i]-f_000[i]-f_001[i]-f_100[i]-f_101[i])*C3 / h1;
//                             o[INDEX3(i,2,0,numComp,3)] = (f_011[i]+f_111[i]-f_010[i]-f_110[i])*C4 / h2;
//                         } // end of component loop i
//                     } // end of k0 loop
//                 } // end of k2 loop
//             } // end of face 3
//             if (!face_cells[4].empty()) {
// #pragma omp for nowait
//                 for (index_t k1=0; k1 < m_NE1; ++k1) {
//                     for (index_t k0=0; k0 < m_NE0; ++k0) {
//                         const double* f_000 = in.getSampleDataRO(INDEX3(k0,k1,0, m_N0,m_N1));
//                         const double* f_100 = in.getSampleDataRO(INDEX3(k0+1,k1,0, m_N0,m_N1));
//                         const double* f_110 = in.getSampleDataRO(INDEX3(k0+1,k1+1,0, m_N0,m_N1));
//                         const double* f_010 = in.getSampleDataRO(INDEX3(k0,k1+1,0, m_N0,m_N1));
//                         const double* f_001 = in.getSampleDataRO(INDEX3(k0,k1,1, m_N0,m_N1));
//                         const double* f_101 = in.getSampleDataRO(INDEX3(k0+1,k1,1, m_N0,m_N1));
//                         const double* f_011 = in.getSampleDataRO(INDEX3(k0,k1+1,1, m_N0,m_N1));
//                         const double* f_111 = in.getSampleDataRO(INDEX3(k0+1,k1+1,1, m_N0,m_N1));
//                         double* o = out.getSampleDataRW(m_faceOffset[4]+INDEX2(k0,k1,m_NE0));
//                         for (index_t i=0; i < numComp; ++i) {
//                             o[INDEX3(i,0,0,numComp,3)] = (f_100[i]+f_110[i]-f_000[i]-f_010[i])*C4 / h0;
//                             o[INDEX3(i,1,0,numComp,3)] = (f_010[i]+f_110[i]-f_000[i]-f_100[i])*C4 / h1;
//                             o[INDEX3(i,2,0,numComp,3)] = (f_001[i]+f_011[i]+f_101[i]+f_111[i]-f_000[i]-f_010[i]-f_100[i]-f_110[i])*C4 / h2;
//                         } // end of component loop i
//                     } // end of k0 loop
//                 } // end of k1 loop
//             } // end of face 4
//             if (!face_cells[5].empty()) {
// #pragma omp for nowait
//                 for (index_t k1=0; k1 < m_NE1; ++k1) {
//                     for (index_t k0=0; k0 < m_NE0; ++k0) {
//                         const double* f_001 = in.getSampleDataRO(INDEX3(k0,k1,m_N2-1, m_N0,m_N1));
//                         const double* f_101 = in.getSampleDataRO(INDEX3(k0+1,k1,m_N2-1, m_N0,m_N1));
//                         const double* f_011 = in.getSampleDataRO(INDEX3(k0,k1+1,m_N2-1, m_N0,m_N1));
//                         const double* f_111 = in.getSampleDataRO(INDEX3(k0+1,k1+1,m_N2-1, m_N0,m_N1));
//                         const double* f_000 = in.getSampleDataRO(INDEX3(k0,k1,m_N2-2, m_N0,m_N1));
//                         const double* f_100 = in.getSampleDataRO(INDEX3(k0+1,k1,m_N2-2, m_N0,m_N1));
//                         const double* f_110 = in.getSampleDataRO(INDEX3(k0+1,k1+1,m_N2-2, m_N0,m_N1));
//                         const double* f_010 = in.getSampleDataRO(INDEX3(k0,k1+1,m_N2-2, m_N0,m_N1));
//                         double* o = out.getSampleDataRW(m_faceOffset[5]+INDEX2(k0,k1,m_NE0));
//                         for (index_t i=0; i < numComp; ++i) {
//                             o[INDEX3(i,0,0,numComp,3)] = (f_101[i]+f_111[i]-f_001[i]-f_011[i])*C4 / h0;
//                             o[INDEX3(i,1,0,numComp,3)] = (f_011[i]+f_111[i]-f_001[i]-f_101[i])*C4 / h1;
//                             o[INDEX3(i,2,0,numComp,3)] = (f_001[i]+f_011[i]+f_101[i]+f_111[i]-f_000[i]-f_010[i]-f_100[i]-f_110[i])*C3 / h2;
//                         } // end of component loop i
//                     } // end of k0 loop
//                 } // end of k1 loop
//             } // end of face 5
//         } // end of parallel section
    }
}

const int* BuckleyDomain::borrowListOfTagsInUse(int fsType) const
{
    static int bricktags[6]={1, 2, 10, 20, 100, 200};    //  left,right,front, back, bottom,top
    switch(fsType)
    {
        case disc_faces:  
	case red_disc_faces:  return bricktags;
	default: 
	{
            stringstream msg;
            msg << "borrowListOfTagsInUse: invalid function space type "
                << fsType;
            throw BuckleyException(msg.str());
	}
    }
}

int BuckleyDomain::getNumberOfTagsInUse(int functionSpaceCode) const
{
    switch(functionSpaceCode)
    {
        case disc_faces:  
	case red_disc_faces:  return 6;
	default: 
	    return 0;
    }
}

