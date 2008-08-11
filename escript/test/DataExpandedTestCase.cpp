
/* $Id$ */

/*******************************************************
 *
 *           Copyright 2003-2007 by ACceSS MNRF
 *       Copyright 2007 by University of Queensland
 *
 *                http://esscc.uq.edu.au
 *        Primary Business: Queensland, Australia
 *  Licensed under the Open Software License version 3.0
 *     http://www.opensource.org/licenses/osl-3.0.php
 *
 *******************************************************/

#include "escript/FunctionSpace.h"
#include "escript/DataExpanded.h"
#include "esysUtils/EsysException.h"
#include "DataExpandedTestCase.h"

#include <iostream>

using namespace CppUnitTest;
using namespace escript;
using namespace std;
using namespace esysUtils;

void DataExpandedTestCase::setUp() {
  //
  // This is called before each test is run
 
}

void DataExpandedTestCase::tearDown() {
  //
  // This is called after each test has been run
 
}

void DataExpandedTestCase::testAll() {

  cout << endl;

  //
  // Create a rank 1 pointData
  DataTypes::ShapeType shape;
  shape.push_back(3);
  DataTypes::ValueType data(DataArrayView::noValues(shape),0);
  DataArrayView pointData(data,shape);

  //
  // Assign an arbitrary value
  pointData(0)=0.0;
  pointData(1)=1.0;
  pointData(2)=2.0;

  //
  // Test constructor
  cout << "\tTest DataExpanded constructor." << endl;
  DataExpanded testData(pointData,FunctionSpace());

  cout << "\tTest getLength." << endl;
  assert(testData.getLength()==pointData.noValues());

  cout << "\tTest getPointDataView." << endl;
  for (int i=0;i<testData.getPointDataView().getShape()[0];i++) {
      assert(testData.getPointDataView()(i)==pointData(i));
  }

  cout << "\tVerify data point attributes." << endl;
  DataArrayView dataView=testData.getPointDataView();
  assert(dataView.getRank()==shape.size());
  assert(dataView.noValues()==shape[0]*1);
  assert(dataView.getShape()[0]==shape[0]);
  assert(testData.getNumDPPSample()==1);
  assert(testData.getNumSamples()==1);
  assert(testData.validSamplePointNo(testData.getNumDPPSample()-1));
  assert(testData.validSampleNo(testData.getNumSamples()-1));

  //
  // Test alternative constructor
  cout << "\tTest DataExpanded alternative constructor." << endl;
  data[0]=0.0;
  data[1]=1.0;
  data[2]=2.0;
  FunctionSpace tmp_fns;
  DataExpanded testData1(tmp_fns,shape,data);

  cout << "\tTest getLength." << endl;
  assert(testData1.getLength()==pointData.noValues());

  cout << "\tTest getPointDataView." << endl;
  for (int i=0;i<testData1.getPointDataView().getShape()[0];i++) {
      assert(testData1.getPointDataView()(i)==pointData(i));
  }

  cout << "\tVerify data point attributes." << endl;
  dataView=testData1.getPointDataView();
  assert(dataView.getRank()==shape.size());
  assert(dataView.noValues()==shape[0]*1);
  assert(dataView.getShape()[0]==shape[0]);
  assert(testData.getNumDPPSample()==1);
  assert(testData.getNumSamples()==1);
  assert(testData.validSamplePointNo(testData.getNumDPPSample()-1));
  assert(testData.validSampleNo(testData.getNumSamples()-1));

  //
  // Test copy constructor
  cout << "\tTest DataExpanded copy constructor." << endl;
  DataExpanded testData2(testData);

  cout << "\tTest getLength." << endl;
  assert(testData2.getLength()==pointData.noValues());

  cout << "\tTest getPointDataView." << endl;
  for (int i=0;i<testData2.getPointDataView().getShape()[0];i++) {
    assert(testData2.getPointDataView()(i)==pointData(i));
  }

  cout << "\tVerify data point attributes." << endl;
  dataView=testData2.getPointDataView();
  assert(dataView.getRank()==shape.size());
  assert(dataView.noValues()==shape[0]*1);
  assert(dataView.getShape()[0]==shape[0]);
  assert(testData2.getNumDPPSample()==1);
  assert(testData2.getNumSamples()==1);
  assert(testData2.validSamplePointNo(testData2.getNumDPPSample()-1));
  assert(testData2.validSampleNo(testData2.getNumSamples()-1));

}


void DataExpandedTestCase::testSlicing() {

  cout << endl;

  //
  // Create a rank 1 pointData
  DataTypes::ShapeType shape;
  shape.push_back(3);
  DataTypes::ValueType data(DataArrayView::noValues(shape),0);
  DataArrayView pointData(data,shape);

  //
  // Assign an arbitrary value
  pointData(0)=0.0;
  pointData(1)=1.0;
  pointData(2)=2.0;

  //
  // Create object to test
  cout << "\tCreate rank 1 DataExpanded object." << endl;
  DataExpanded testData(pointData,FunctionSpace());

  cout << "\tTest slicing (whole object)." << endl;
  DataTypes::RegionType region;
  region.push_back(DataTypes::RegionType::value_type(0,shape[0]));

  DataAbstract* testData2=testData.getSlice(region);

  //
  // Verify data values
  cout << "\tVerify data point values." << endl;
  for (int i=0;i<testData2->getPointDataView().getShape()[0];i++) {
    assert(testData2->getPointDataView()(i)==pointData(region[0].first+i));
  }

  cout << "\tVerify data point attributes." << endl;
  DataArrayView dataView=testData2->getPointDataView();
  assert(dataView.getRank()==shape.size());
  assert(dataView.noValues()==shape[0]*1);
  assert(dataView.getShape()[0]==shape[0]);

  delete testData2;
}

void DataExpandedTestCase::testSlicing2() {

  cout << endl;

  //
  // Create a rank 2 pointData
  DataTypes::ShapeType shape;
  shape.push_back(3);
  shape.push_back(3);
  DataTypes::ValueType data(DataArrayView::noValues(shape),0);
  DataArrayView pointData(data,shape);

  //
  // Assign an arbitrary value
  pointData(0,0)=0.0;
  pointData(1,0)=1.0;
  pointData(2,0)=2.0;
  pointData(0,1)=3.0;
  pointData(1,1)=4.0;
  pointData(2,1)=5.0;
  pointData(0,2)=6.0;
  pointData(1,2)=7.0;
  pointData(2,2)=8.0;

  //
  // Create object to test
  cout << "\tCreate rank 2 DataExpanded object." << endl;
  DataExpanded testData(pointData,FunctionSpace());

  cout << "\tTest slicing (part object)." << endl;
  DataTypes::RegionType region;
  region.push_back(DataTypes::RegionType::value_type(0,2));
  region.push_back(DataTypes::RegionType::value_type(0,2));
  DataAbstract* testData2=testData.getSlice(region);

  //
  // Verify data values
  cout << "\tVerify data point values." << endl;
  for (int j=0;j<testData2->getPointDataView().getShape()[1];j++) {
    for (int i=0;i<testData2->getPointDataView().getShape()[0];i++) {
      assert(testData2->getPointDataView()(i,j)==pointData(region[0].first+i,region[1].first+j));
    }
  }

  cout << "\tVerify data point attributes." << endl;
  DataArrayView dataView=testData2->getPointDataView();
  assert(dataView.getRank()==region.size());
  assert(dataView.noValues()==(region[0].second-region[0].first)*(region[1].second-region[1].first));
  assert(dataView.getShape()[0]==(region[0].second-region[0].first));
  assert(dataView.getShape()[1]==(region[1].second-region[1].first));

  cout << "\tTest slicing (part object)." << endl;
  DataTypes::RegionType region2;
  region2.push_back(DataTypes::RegionType::value_type(1,3));
  region2.push_back(DataTypes::RegionType::value_type(1,3));
  DataAbstract* testData3=testData.getSlice(region2);

  //
  // Verify data values
  cout << "\tVerify data point values." << endl;
  for (int j=0;j<testData3->getPointDataView().getShape()[1];j++) {
    for (int i=0;i<testData3->getPointDataView().getShape()[0];i++) {
      assert(testData3->getPointDataView()(i,j)==pointData(region2[0].first+i,region2[1].first+j));
    }
  }

  cout << "\tVerify data point attributes." << endl;
  dataView=testData3->getPointDataView();
  assert(dataView.getRank()==region2.size());
  assert(dataView.noValues()==(region2[0].second-region2[0].first)*(region2[1].second-region2[1].first));
  assert(dataView.getShape()[0]==(region2[0].second-region2[0].first));
  assert(dataView.getShape()[1]==(region2[1].second-region2[1].first));

  delete testData2;
  delete testData3;

}

void DataExpandedTestCase::testSlicing3() {

  cout << endl;

  //
  // Create a rank 3 pointData
  DataTypes::ShapeType shape;
  shape.push_back(3);
  shape.push_back(3);
  shape.push_back(3);
  DataTypes::ValueType data(DataArrayView::noValues(shape),0);
  DataArrayView pointData(data,shape);

  //
  // Assign an arbitrary value
  pointData(0,0,0)=0.0;
  pointData(1,0,0)=1.0;
  pointData(2,0,0)=2.0;
  pointData(0,1,0)=3.0;
  pointData(1,1,0)=4.0;
  pointData(2,1,0)=5.0;
  pointData(0,2,0)=6.0;
  pointData(1,2,0)=7.0;
  pointData(2,2,0)=8.0;

  pointData(0,0,1)=9.0;
  pointData(1,0,1)=10.0;
  pointData(2,0,1)=11.0;
  pointData(0,1,1)=12.0;
  pointData(1,1,1)=13.0;
  pointData(2,1,1)=14.0;
  pointData(0,2,1)=15.0;
  pointData(1,2,1)=16.0;
  pointData(2,2,1)=17.0;

  pointData(0,0,2)=18.0;
  pointData(1,0,2)=19.0;
  pointData(2,0,2)=20.0;
  pointData(0,1,2)=21.0;
  pointData(1,1,2)=22.0;
  pointData(2,1,2)=23.0;
  pointData(0,2,2)=24.0;
  pointData(1,2,2)=25.0;
  pointData(2,2,2)=26.0;

  //
  // Create object to test
  cout << "\tCreate rank 3 DataExpanded object." << endl;
  DataExpanded testData(pointData,FunctionSpace());

  cout << "\tTest slicing (part object)." << endl;
  DataTypes::RegionType region;
  region.push_back(DataTypes::RegionType::value_type(0,2));
  region.push_back(DataTypes::RegionType::value_type(0,2));
  region.push_back(DataTypes::RegionType::value_type(0,2));
  DataAbstract* testData2=testData.getSlice(region);

  //
  // Verify data values
  cout << "\tVerify data point values." << endl;
  for (int k=0;k<testData2->getPointDataView().getShape()[2];k++) {
    for (int j=0;j<testData2->getPointDataView().getShape()[1];j++) {
      for (int i=0;i<testData2->getPointDataView().getShape()[0];i++) {
        assert(testData2->getPointDataView()(i,j,k)==pointData(region[0].first+i,
                                                               region[1].first+j,
                                                               region[2].first+k));
      }
    }
  }

  cout << "\tVerify data point attributes." << endl;
  DataArrayView dataView=testData2->getPointDataView();
  assert(dataView.getRank()==region.size());
  assert(dataView.noValues()==(region[0].second-region[0].first)
                               *(region[1].second-region[1].first)
                               *(region[2].second-region[2].first));
  assert(dataView.getShape()[0]==(region[0].second-region[0].first));
  assert(dataView.getShape()[1]==(region[1].second-region[1].first));
  assert(dataView.getShape()[2]==(region[2].second-region[2].first));

  cout << "\tTest slicing (part object)." << endl;
  DataTypes::RegionType region2;
  region2.push_back(DataTypes::RegionType::value_type(1,3));
  region2.push_back(DataTypes::RegionType::value_type(1,3));
  region2.push_back(DataTypes::RegionType::value_type(1,3));
  DataAbstract* testData3=testData.getSlice(region2);

  //
  // Verify data values
  cout << "\tVerify data point values." << endl;
  for (int k=0;k<testData3->getPointDataView().getShape()[2];k++) {
    for (int j=0;j<testData3->getPointDataView().getShape()[1];j++) {
      for (int i=0;i<testData3->getPointDataView().getShape()[0];i++) {
        assert(testData3->getPointDataView()(i,j,k)==pointData(region2[0].first+i,
                                                               region2[1].first+j,
                                                               region2[2].first+k));
      }
    }
  }

  cout << "\tVerify data point attributes." << endl;
  dataView=testData2->getPointDataView();
  assert(dataView.getRank()==region.size());
  assert(dataView.noValues()==(region[0].second-region[0].first)
                               *(region[1].second-region[1].first)
                               *(region[2].second-region[2].first));
  assert(dataView.getShape()[0]==(region[0].second-region[0].first));
  assert(dataView.getShape()[1]==(region[1].second-region[1].first));
  assert(dataView.getShape()[2]==(region[2].second-region[2].first));

  delete testData2;
  delete testData3;

}


void DataExpandedTestCase::testSliceSetting() {

  cout << endl;

  //
  // Create a rank 2 pointData
  DataTypes::ShapeType shape;
  shape.push_back(2);
  shape.push_back(2);
  DataTypes::ValueType data(DataArrayView::noValues(shape),0);
  DataArrayView pointData(data,shape);

  //
  // Assign an arbitrary value
  pointData(0,0)=0.0;
  pointData(0,1)=1.0;
  pointData(1,0)=2.0;
  pointData(1,1)=3.0;

  //
  // Create object to test
  cout << "\tCreate rank 2 DataExpanded object." << endl;
  DataExpanded testData(pointData,FunctionSpace());

  //
  // Create another rank 2 pointData
  DataTypes::ShapeType shape2;
  shape2.push_back(3);
  shape2.push_back(3);
  DataTypes::ValueType data2(DataArrayView::noValues(shape2),0);
  DataArrayView pointData2(data2,shape2);

  //
  // Assign an arbitrary value
  pointData2(0,0)=0.1;
  pointData2(0,1)=1.1;
  pointData2(0,2)=2.1;
  pointData2(1,0)=3.1;
  pointData2(1,1)=4.1;
  pointData2(1,2)=5.1;
  pointData2(2,0)=6.1;
  pointData2(2,1)=7.1;
  pointData2(2,2)=8.1;

  //
  // Create object to test
  cout << "\tCreate second rank 2 DataExpanded object." << endl;
  DataExpanded testData2(pointData2,FunctionSpace());

  cout << "\tTest slice setting (1)." << endl;

  DataTypes::RegionType region;
  region.push_back(DataTypes::RegionType::value_type(0,2));
  region.push_back(DataTypes::RegionType::value_type(0,2));

  DataTypes::RegionType region2;
  region2.push_back(DataTypes::RegionType::value_type(1,3));
  region2.push_back(DataTypes::RegionType::value_type(1,3));

  DataAbstract* testData3=testData.getSlice(region);

  testData2.setSlice(testData3,region2);

  //
  // Verify data values
  cout << "\tVerify data point values." << endl;
  for (int j=region2[1].first;j<region2[1].second;j++) {
    for (int i=region2[0].first;i<region2[0].second;i++) {
      assert(testData2.getPointDataView()(i,j)==pointData(i-(region[0].second-1),j-(region[1].second-1)));
    }
   }

  delete testData3;

}

void DataExpandedTestCase::testSliceSetting2() {

  cout << endl;

  //
  // Create a rank 0 pointData
  DataTypes::ShapeType shape;
  DataTypes::ValueType data(DataArrayView::noValues(shape),0);
  DataArrayView pointData(data,shape);

  //
  // Assign an arbitrary value
  pointData()=0.0;

  //
  // Create object to test
  cout << "\tCreate rank 0 DataExpanded object." << endl;
  DataExpanded testData(pointData,FunctionSpace());

  //
  // Create a rank 2 pointData
  DataTypes::ShapeType shape2;
  shape2.push_back(3);
  shape2.push_back(3);
  DataTypes::ValueType data2(DataArrayView::noValues(shape2),0);
  DataArrayView pointData2(data2,shape2);

  //
  // Assign an arbitrary value
  pointData2(0,0)=0.1;
  pointData2(0,1)=1.1;
  pointData2(0,2)=2.1;
  pointData2(1,0)=3.1;
  pointData2(1,1)=4.1;
  pointData2(1,2)=5.1;
  pointData2(2,0)=6.1;
  pointData2(2,1)=7.1;
  pointData2(2,2)=8.1;

  //
  // Create object to test
  cout << "\tCreate rank 2 DataExpanded object." << endl;
  DataExpanded testData2(pointData2,FunctionSpace());

  cout << "\tTest slice setting (1)." << endl;

  DataTypes::RegionType region;

  DataTypes::RegionType region2;
  region2.push_back(DataTypes::RegionType::value_type(1,1));
  region2.push_back(DataTypes::RegionType::value_type(1,1));

  DataAbstract* testData3=testData.getSlice(region);

  testData2.setSlice(testData3,region2);

  //
  // Verify data values
  cout << "\tVerify data point values." << endl;
  for (int j=region2[1].first;j<region2[1].second;j++) {
    for (int i=region2[0].first;i<region2[0].second;i++) {
      assert(testData2.getPointDataView()(i,j)==pointData());
    }
   }

  delete testData3;

}


TestSuite* DataExpandedTestCase::suite ()
{
  //
  // Create the suite of tests to perform.
  TestSuite *testSuite = new TestSuite ("DataExpandedTestCase");
  testSuite->addTest (new TestCaller< DataExpandedTestCase>("testAll",&DataExpandedTestCase::testAll));
  testSuite->addTest (new TestCaller< DataExpandedTestCase>("testSlicing",&DataExpandedTestCase::testSlicing));
  testSuite->addTest (new TestCaller< DataExpandedTestCase>("testSlicing2",&DataExpandedTestCase::testSlicing2));
  testSuite->addTest (new TestCaller< DataExpandedTestCase>("testSlicing3",&DataExpandedTestCase::testSlicing3));
  testSuite->addTest (new TestCaller< DataExpandedTestCase>("testSliceSetting",&DataExpandedTestCase::testSliceSetting));
  testSuite->addTest (new TestCaller< DataExpandedTestCase>("testSliceSetting2",&DataExpandedTestCase::testSliceSetting2));
  return testSuite;
}
