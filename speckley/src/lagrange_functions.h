
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

namespace speckley {

inline double lagrange_degree2_0(double xi) {
    return 0.5*xi*(xi - 1.0);
}

inline double lagrange_degree2_1(double xi) {
    return -1.0*pow(xi, 2) + 1.0;
}

inline double lagrange_degree2_2(double xi) {
    return 0.5*xi*(xi + 1.0);
}

inline double lagrange_degree3_0(double xi) {
    return (-0.625*xi + 0.625)*(xi - 0.447213595499958)*(xi + 0.447213595499958);
}

inline double lagrange_degree3_1(double xi) {
    return (xi - 0.447213595499958)*(xi + 1.0)*(1.39754248593737*xi - 1.39754248593737);
}

inline double lagrange_degree3_2(double xi) {
    return (-1.39754248593737*xi + 1.39754248593737)*(xi + 0.447213595499958)*(xi + 1.0);
}

inline double lagrange_degree3_3(double xi) {
    return (0.625*xi - 0.279508497187474)*(xi + 0.447213595499958)*(xi + 1.0);
}

inline double lagrange_degree4_0(double xi) {
    return 0.874999999999998*xi*(xi - 1.0)*(xi - 0.654653670707977)*(xi + 0.654653670707977);
}

inline double lagrange_degree4_1(double xi) {
    return -2.04166666666667*xi*(xi - 1.0)*(xi - 0.654653670707977)*(xi + 1.0);
}

inline double lagrange_degree4_2(double xi) {
    return (xi - 0.654653670707977)*(xi + 0.654653670707977)*(xi + 1.0)*(2.33333333333333*xi - 2.33333333333333);
}

inline double lagrange_degree4_3(double xi) {
    return -2.04166666666667*xi*(xi - 1.0)*(xi + 0.654653670707977)*(xi + 1.0);
}

inline double lagrange_degree4_4(double xi) {
    return 0.875*xi*(xi - 0.654653670707977)*(xi + 0.654653670707977)*(xi + 1.0);
}

inline double lagrange_degree5_0(double xi) {
    return (-1.31250000000001*xi + 1.31250000000001)*(xi - 0.765055323929464)*(xi - 0.285231516480645)*(xi + 0.285231516480645)*(xi + 0.765055323929466);
}

inline double lagrange_degree5_1(double xi) {
    return (xi - 0.765055323929464)*(xi - 0.285231516480645)*(xi + 0.285231516480645)*(xi + 1.0)*(3.12725658269744*xi - 3.12725658269744);
}

inline double lagrange_degree5_2(double xi) {
    return (-3.78648303389511*xi + 3.78648303389511)*(xi - 0.765055323929464)*(xi - 0.285231516480645)*(xi + 0.765055323929466)*(xi + 1.0);
}

inline double lagrange_degree5_3(double xi) {
    return (xi - 0.765055323929464)*(xi + 0.285231516480645)*(xi + 0.765055323929466)*(xi + 1.0)*(3.78648303389512*xi - 3.78648303389512);
}

inline double lagrange_degree5_4(double xi) {
    return (-3.12725658269744*xi + 3.12725658269744)*(xi - 0.285231516480645)*(xi + 0.285231516480645)*(xi + 0.765055323929466)*(xi + 1.0);
}

inline double lagrange_degree5_5(double xi) {
    return (xi - 0.285231516480645)*(xi + 0.285231516480645)*(xi + 0.765055323929466)*(xi + 1.0)*(1.3125*xi - 1.00413511265742);
}

inline double lagrange_degree6_0(double xi) {
    return 2.06249999999999*xi*(xi - 1.0)*(xi - 0.830223896278565)*(xi - 0.468848793470715)*(xi + 0.468848793470714)*(xi + 0.830223896278566);
}

inline double lagrange_degree6_1(double xi) {
    return -4.97286970608696*xi*(xi - 1.0)*(xi - 0.830223896278565)*(xi - 0.468848793470715)*(xi + 0.468848793470714)*(xi + 1.0);
}

inline double lagrange_degree6_2(double xi) {
    return 6.21036970608698*xi*(xi - 1.0)*(xi - 0.830223896278565)*(xi - 0.468848793470715)*(xi + 0.830223896278566)*(xi + 1.0);
}

inline double lagrange_degree6_3(double xi) {
    return -6.6*pow(xi, 6) + 12.6*pow(xi, 4) - 7.0*pow(xi, 2) + 1.46549439250521e-15*xi + 1.0;
}

inline double lagrange_degree6_4(double xi) {
    return 6.21036970608699*xi*(xi - 1.0)*(xi - 0.830223896278565)*(xi + 0.468848793470714)*(xi + 0.830223896278566)*(xi + 1.0);
}

inline double lagrange_degree6_5(double xi) {
    return -4.97286970608697*xi*(xi - 1.0)*(xi - 0.468848793470715)*(xi + 0.468848793470714)*(xi + 0.830223896278566)*(xi + 1.0);
}

inline double lagrange_degree6_6(double xi) {
    return 2.06249999999999*xi*(xi - 0.830223896278565)*(xi - 0.468848793470715)*(xi + 0.468848793470714)*(xi + 0.830223896278566)*(xi + 1.0);
}

inline double lagrange_degree7_0(double xi) {
    return (-3.35156249999998*xi + 3.35156249999997)*(xi - 0.871740148509608)*(xi - 0.591700181433143)*(xi - 0.209299217902479)*(xi + 0.209299217902479)*(xi + 0.591700181433142)*(xi + 0.871740148509606);
}

inline double lagrange_degree7_1(double xi) {
    return (xi - 0.871740148509608)*(xi - 0.591700181433143)*(xi - 0.209299217902479)*(xi + 0.209299217902479)*(xi + 0.591700181433142)*(xi + 1.0)*(8.14072271825386*xi - 8.14072271825384);
}

inline double lagrange_degree7_2(double xi) {
    return (-10.3581368289505*xi + 10.3581368289505)*(xi - 0.871740148509608)*(xi - 0.591700181433143)*(xi - 0.209299217902479)*(xi + 0.209299217902479)*(xi + 0.871740148509606)*(xi + 1.0);
}

inline double lagrange_degree7_3(double xi) {
    return (xi - 0.871740148509608)*(xi - 0.591700181433143)*(xi - 0.209299217902479)*(xi + 0.591700181433142)*(xi + 0.871740148509606)*(xi + 1.0)*(11.3898137484866*xi - 11.3898137484866);
}

inline double lagrange_degree7_4(double xi) {
    return (-11.3898137484866*xi + 11.3898137484866)*(xi - 0.871740148509608)*(xi - 0.591700181433143)*(xi + 0.209299217902479)*(xi + 0.591700181433142)*(xi + 0.871740148509606)*(xi + 1.0);
}

inline double lagrange_degree7_5(double xi) {
    return (xi - 0.871740148509608)*(xi - 0.209299217902479)*(xi + 0.209299217902479)*(xi + 0.591700181433142)*(xi + 0.871740148509606)*(xi + 1.0)*(10.3581368289505*xi - 10.3581368289505);
}

inline double lagrange_degree7_6(double xi) {
    return (-8.140722718254*xi + 8.14072271825398)*(xi - 0.591700181433143)*(xi - 0.209299217902479)*(xi + 0.209299217902479)*(xi + 0.591700181433142)*(xi + 0.871740148509606)*(xi + 1.0);
}

inline double lagrange_degree7_7(double xi) {
    return (xi - 0.591700181433143)*(xi - 0.209299217902479)*(xi + 0.209299217902479)*(xi + 0.591700181433142)*(xi + 0.871740148509606)*(xi + 1.0)*(3.35156250000011*xi - 2.92169159148933);
}

inline double lagrange_degree8_0(double xi) {
    return 5.58593750000054*xi*(xi - 1.0)*(xi - 0.899757995411458)*(xi - 0.677186279510736)*(xi - 0.363117463826179)*(xi + 0.363117463826178)*(xi + 0.677186279510735)*(xi + 0.899757995411471);
}

inline double lagrange_degree8_1(double xi) {
    return -13.63453200049*xi*(xi - 1.0)*(xi - 0.899757995411458)*(xi - 0.677186279510736)*(xi - 0.363117463826179)*(xi + 0.363117463826178)*(xi + 0.677186279510735)*(xi + 1.0);
}

inline double lagrange_degree8_2(double xi) {
    return 17.5609949844529*xi*(xi - 1.0)*(xi - 0.899757995411458)*(xi - 0.677186279510736)*(xi - 0.363117463826179)*(xi + 0.363117463826178)*(xi + 0.899757995411471)*(xi + 1.0);
}

inline double lagrange_degree8_3(double xi) {
    return -19.7266861982491*xi*(xi - 1.0)*(xi - 0.899757995411458)*(xi - 0.677186279510736)*(xi - 0.363117463826179)*(xi + 0.677186279510735)*(xi + 0.899757995411471)*(xi + 1.0);
}

inline double lagrange_degree8_4(double xi) {
    return (xi - 0.899757995411458)*(xi - 0.677186279510736)*(xi - 0.363117463826179)*(xi + 0.363117463826178)*(xi + 0.677186279510735)*(xi + 0.899757995411471)*(xi + 1.0)*(20.4285714285713*xi - 20.4285714285713);
}

inline double lagrange_degree8_5(double xi) {
    return -19.7266861982493*xi*(xi - 1.0)*(xi - 0.899757995411458)*(xi - 0.677186279510736)*(xi + 0.363117463826178)*(xi + 0.677186279510735)*(xi + 0.899757995411471)*(xi + 1.0);
}

inline double lagrange_degree8_6(double xi) {
    return 17.5609949844536*xi*(xi - 1.0)*(xi - 0.899757995411458)*(xi - 0.363117463826179)*(xi + 0.363117463826178)*(xi + 0.677186279510735)*(xi + 0.899757995411471)*(xi + 1.0);
}

inline double lagrange_degree8_7(double xi) {
    return -13.6345320004894*xi*(xi - 1.0)*(xi - 0.677186279510736)*(xi - 0.363117463826179)*(xi + 0.363117463826178)*(xi + 0.677186279510735)*(xi + 0.899757995411471)*(xi + 1.0);
}

inline double lagrange_degree8_8(double xi) {
    return 5.58593749999947*xi*(xi - 0.899757995411458)*(xi - 0.677186279510736)*(xi - 0.363117463826179)*(xi + 0.363117463826178)*(xi + 0.677186279510735)*(xi + 0.899757995411471)*(xi + 1.0);
}

inline double lagrange_degree9_0(double xi) {
    return (-9.49609374999916*xi + 9.49609374999925)*(xi - 0.919533908166451)*(xi - 0.738773865105506)*(xi - 0.477924949810445)*(xi - 0.165278957666387)*(xi + 0.165278957666387)*(xi + 0.477924949810445)*(xi + 0.738773865105507)*(xi + 0.919533908166451);
}

inline double lagrange_degree9_1(double xi) {
    return (xi - 0.919533908166451)*(xi - 0.738773865105506)*(xi - 0.477924949810445)*(xi - 0.165278957666387)*(xi + 0.165278957666387)*(xi + 0.477924949810445)*(xi + 0.738773865105507)*(xi + 1.0)*(23.2581991069278*xi - 23.258199106928);
}

inline double lagrange_degree9_2(double xi) {
    return (-30.2089539641755*xi + 30.2089539641758)*(xi - 0.919533908166451)*(xi - 0.738773865105506)*(xi - 0.477924949810445)*(xi - 0.165278957666387)*(xi + 0.165278957666387)*(xi + 0.477924949810445)*(xi + 0.919533908166451)*(xi + 1.0);
}

inline double lagrange_degree9_3(double xi) {
    return (xi - 0.919533908166451)*(xi - 0.738773865105506)*(xi - 0.477924949810445)*(xi - 0.165278957666387)*(xi + 0.165278957666387)*(xi + 0.738773865105507)*(xi + 0.919533908166451)*(xi + 1.0)*(34.4250370034967*xi - 34.425037003497);
}

inline double lagrange_degree9_4(double xi) {
    return (-36.4571961125312*xi + 36.4571961125316)*(xi - 0.919533908166451)*(xi - 0.738773865105506)*(xi - 0.477924949810445)*(xi - 0.165278957666387)*(xi + 0.477924949810445)*(xi + 0.738773865105507)*(xi + 0.919533908166451)*(xi + 1.0);
}

inline double lagrange_degree9_5(double xi) {
    return (xi - 0.919533908166451)*(xi - 0.738773865105506)*(xi - 0.477924949810445)*(xi + 0.165278957666387)*(xi + 0.477924949810445)*(xi + 0.738773865105507)*(xi + 0.919533908166451)*(xi + 1.0)*(36.4571961125312*xi - 36.4571961125316);
}

inline double lagrange_degree9_6(double xi) {
    return (-34.4250370034965*xi + 34.4250370034968)*(xi - 0.919533908166451)*(xi - 0.738773865105506)*(xi - 0.165278957666387)*(xi + 0.165278957666387)*(xi + 0.477924949810445)*(xi + 0.738773865105507)*(xi + 0.919533908166451)*(xi + 1.0);
}

inline double lagrange_degree9_7(double xi) {
    return (xi - 0.919533908166451)*(xi - 0.477924949810445)*(xi - 0.165278957666387)*(xi + 0.165278957666387)*(xi + 0.477924949810445)*(xi + 0.738773865105507)*(xi + 0.919533908166451)*(xi + 1.0)*(30.2089539641749*xi - 30.2089539641752);
}

inline double lagrange_degree9_8(double xi) {
    return (-23.2581991069258*xi + 23.258199106926)*(xi - 0.738773865105506)*(xi - 0.477924949810445)*(xi - 0.165278957666387)*(xi + 0.165278957666387)*(xi + 0.477924949810445)*(xi + 0.738773865105507)*(xi + 0.919533908166451)*(xi + 1.0);
}

inline double lagrange_degree9_9(double xi) {
    return (xi - 0.738773865105506)*(xi - 0.477924949810445)*(xi - 0.165278957666387)*(xi + 0.165278957666387)*(xi + 0.477924949810445)*(xi + 0.738773865105507)*(xi + 0.919533908166451)*(xi + 1.0)*(9.49609374999766*xi - 8.73198019825036);
}

inline double lagrange_degree10_0(double xi) {
    return 16.4023437499897*xi*(xi - 1.00000000000002)*(xi - 0.934001430408021)*(xi - 0.784483473663165)*(xi - 0.565235326996199)*(xi - 0.29575813558694)*(xi + 0.295758135586939)*(xi + 0.565235326996199)*(xi + 0.784483473663166)*(xi + 0.934001430408012);
}

inline double lagrange_degree10_1(double xi) {
    return -40.2732656174519*xi*(xi - 1.00000000000002)*(xi - 0.934001430408021)*(xi - 0.784483473663165)*(xi - 0.565235326996199)*(xi - 0.29575813558694)*(xi + 0.295758135586939)*(xi + 0.565235326996199)*(xi + 0.784483473663166)*(xi + 1.0);
}

inline double lagrange_degree10_2(double xi) {
    return 52.62659082067*xi*(xi - 1.00000000000002)*(xi - 0.934001430408021)*(xi - 0.784483473663165)*(xi - 0.565235326996199)*(xi - 0.29575813558694)*(xi + 0.295758135586939)*(xi + 0.565235326996199)*(xi + 0.934001430408012)*(xi + 1.0);
}

inline double lagrange_degree10_3(double xi) {
    return -60.5836186612294*xi*(xi - 1.00000000000002)*(xi - 0.934001430408021)*(xi - 0.784483473663165)*(xi - 0.565235326996199)*(xi - 0.29575813558694)*(xi + 0.295758135586939)*(xi + 0.784483473663166)*(xi + 0.934001430408012)*(xi + 1.0);
}

inline double lagrange_degree10_4(double xi) {
    return 65.1533465334198*xi*(xi - 1.00000000000002)*(xi - 0.934001430408021)*(xi - 0.784483473663165)*(xi - 0.565235326996199)*(xi - 0.29575813558694)*(xi + 0.565235326996199)*(xi + 0.784483473663166)*(xi + 0.934001430408012)*(xi + 1.0);
}

inline double lagrange_degree10_5(double xi) {
    return (-66.650793650796*xi + 66.6507936507973)*(xi - 0.934001430408021)*(xi - 0.784483473663165)*(xi - 0.565235326996199)*(xi - 0.29575813558694)*(xi + 0.295758135586939)*(xi + 0.565235326996199)*(xi + 0.784483473663166)*(xi + 0.934001430408012)*(xi + 1.0);
}

inline double lagrange_degree10_6(double xi) {
    return 65.1533465334186*xi*(xi - 1.00000000000002)*(xi - 0.934001430408021)*(xi - 0.784483473663165)*(xi - 0.565235326996199)*(xi + 0.295758135586939)*(xi + 0.565235326996199)*(xi + 0.784483473663166)*(xi + 0.934001430408012)*(xi + 1.0);
}

inline double lagrange_degree10_7(double xi) {
    return -60.5836186612267*xi*(xi - 1.00000000000002)*(xi - 0.934001430408021)*(xi - 0.784483473663165)*(xi - 0.29575813558694)*(xi + 0.295758135586939)*(xi + 0.565235326996199)*(xi + 0.784483473663166)*(xi + 0.934001430408012)*(xi + 1.0);
}

inline double lagrange_degree10_8(double xi) {
    return 52.6265908206628*xi*(xi - 1.00000000000002)*(xi - 0.934001430408021)*(xi - 0.565235326996199)*(xi - 0.29575813558694)*(xi + 0.295758135586939)*(xi + 0.565235326996199)*(xi + 0.784483473663166)*(xi + 0.934001430408012)*(xi + 1.0);
}

inline double lagrange_degree10_9(double xi) {
    return -40.2732656174398*xi*(xi - 1.00000000000002)*(xi - 0.784483473663165)*(xi - 0.565235326996199)*(xi - 0.29575813558694)*(xi + 0.295758135586939)*(xi + 0.565235326996199)*(xi + 0.784483473663166)*(xi + 0.934001430408012)*(xi + 1.0);
}

inline double lagrange_degree10_10(double xi) {
    return 16.402343749983*xi*(xi - 0.934001430408021)*(xi - 0.784483473663165)*(xi - 0.565235326996199)*(xi - 0.29575813558694)*(xi + 0.295758135586939)*(xi + 0.565235326996199)*(xi + 0.784483473663166)*(xi + 0.934001430408012)*(xi + 1.0);
}

} //namespace speckley
