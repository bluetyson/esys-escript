
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
    
    
    
    const unsigned int NUM_QUAD=8;	// number of quadrature points required


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
}

bool BuckleyDomain::isValidFunctionSpaceType(int functionSpaceType) const
{
    return (functionSpaceType==ctsfn || functionSpaceType==discfn);
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


void BuckleyDomain::processMods() const
{
      if (leaves!=0)
      {
	  delete [] leaves;
	  delete [] samplerefids;
      }
      leaves=ot.process(numpts);
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
    return invalidspace;  
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
    throw BuckleyException("Not Implemented");
}

void BuckleyDomain::addPDEToSystem(
                     escript::AbstractSystemMatrix& mat, escript::Data& rhs,
                     const escript::Data& A, const escript::Data& B, const escript::Data& C, 
                     const escript::Data& D, const escript::Data& X, const escript::Data& Y,
                     const escript::Data& d, const escript::Data& y,
                     const escript::Data& d_contact, const escript::Data& y_contact, 
                     const escript::Data& d_dirac, const escript::Data& y_dirac) const
{
    throw BuckleyException("Not Implemented");
}

void BuckleyDomain::addPDEToRHS(escript::Data& rhs,
                     const escript::Data& X, const escript::Data& Y,
                     const escript::Data& y, const escript::Data& y_contact, const escript::Data& y_dirac) const
{
    throw BuckleyException("Not Implemented");
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
    throw BuckleyException("Not Implemented");
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
   PatternBuilder* pb=makePB(numpts/numranks,26);
   
   // again, dummy values for a sole rank
   Paso_SystemMatrixPattern* psystemMatrixPattern=pb->generatePattern(0, numpts); 
   
 
//    Paso_SystemMatrixPattern* fsystemMatrixPattern=Finley_getPattern(getFinley_Mesh(),reduceRowOrder,reduceColOrder);
//    checkFinleyError();
//    Paso_SystemMatrix* fsystemMatrix;
//    int trilinos = 0;
//    if (trilinos) {
// #ifdef TRILINOS
//       /* Allocation Epetra_VrbMatrix here */
// #endif
//    }
//    else {
//       fsystemMatrix=Paso_SystemMatrix_alloc(type,fsystemMatrixPattern,row_blocksize,column_blocksize,FALSE);
//    }
//    checkPasoError();
//    Paso_SystemMatrixPattern_free(fsystemMatrixPattern);
//    SystemMatrixAdapter* sma=new SystemMatrixAdapter(fsystemMatrix, row_blocksize, row_functionspace, column_blocksize, column_functionspace);
//    return ASM_ptr(sma);
   
  
  
    throw BuckleyException("Not Implemented ::newSystemMatrix");
}

escript::ATP_ptr BuckleyDomain::newTransportProblem(
                      const bool useBackwardEuler,
                      const int blocksize,
                      const escript::FunctionSpace& functionspace,
                      const int type) const
{
    throw BuckleyException("Not Implemented");
}

int BuckleyDomain::getNumDataPointsGlobal() const
{
    throw BuckleyException("Not Implemented");
}


  BUCKLEY_DLL_API
std::pair<int,int> BuckleyDomain::getDataShape(int functionSpaceCode) const
{
   switch (functionSpaceCode)
   {
     case ctsfn: return std::pair<int,int>(1,numpts);  
     case discfn: return std::pair<int, int>(NUM_QUAD,ot.leafCount());
   }
   throw BuckleyException("Not Implemented");  
  
}

BUCKLEY_DLL_API
void BuckleyDomain::setNewX(const escript::Data& arg)
{
    throw BuckleyException("This domain does not support changing coordinates");  
}


BUCKLEY_DLL_API
void BuckleyDomain::Print_Mesh_Info(const bool full) const
{
    if (modified)
    {
        processMods();
    }
    std::cout << "Buckley tree with " << ot.leafCount();
    std::cout << ((ot.leafCount()>1)?" leaves ":" leaf ") << "and " << numpts << " unknowns\n";
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
    default: return "Invalid";  
    };
}


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
       
}


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