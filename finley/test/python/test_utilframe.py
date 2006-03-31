# $Id:$
"""
frame to ran a single test out of the Test_util suite
"""

__copyright__="""  Copyright (c) 2006 by ACcESS MNRF
                    http://www.access.edu.au
                Primary Business: Queensland, Australia"""
__license__="""Licensed under the Open Software License version 3.0
             http://www.opensource.org/licenses/osl-3.0.php"""
import unittest
from esys.escript import *
from esys.finley import Rectangle
import numarray

class Test_util2(unittest.TestCase):
   RES_TOL=1.e-7
   def setUp(self):
       self.__dom =Rectangle(10,10,2)
       self.functionspace = FunctionOnBoundary(self.__dom) # due to a bug in escript python needs to hold a reference to the domain

class Test_util_binary_still_failing(Test_util2):
   """
   these binary opereations still fail! (see Mantis 0000053)
   """
   #+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   def test_power_float_rank0_constData_rank0(self):
      arg0=4.35251522982
      arg1=Data(2.94566651719,self.functionspace)
      res=power(arg0,arg1)
      ref=Data(76.1230031099,self.functionspace)
      self.failUnless(isinstance(res,Data),"wrong type of result.")
      self.failUnlessEqual(res.getShape(),(),"wrong shape of result.")
      self.failUnless(Lsup(res-ref)<=self.RES_TOL*Lsup(ref),"wrong result")
   #+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   def test_power_float_rank0_expandedData_rank0(self):
      arg0=3.41819405792
      msk_arg1=whereNegative(self.functionspace.getX()[0]-0.5)
      arg1=msk_arg1*(3.18794922769)+(1.-msk_arg1)*(0.35955973515)
      res=power(arg0,arg1)
      msk_ref=whereNegative(self.functionspace.getX()[0]-0.5)
      ref=msk_ref*(50.3172415941)+(1.-msk_ref)*(1.55572132713)
      self.failUnless(isinstance(res,Data),"wrong type of result.")
      self.failUnlessEqual(res.getShape(),(),"wrong shape of result.")
      self.failUnless(Lsup(res-ref)<=self.RES_TOL*Lsup(ref),"wrong result")
   #+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   def test_power_float_rank0_taggedData_rank0(self):
      arg0=3.76704928748
      arg1=Data(1.30536801555,self.functionspace)
      arg1.setTaggedValue(1,3.46723750757)
      res=power(arg0,arg1)
      ref=Data(5.64798685367,self.functionspace)
      ref.setTaggedValue(1,99.3420963479)
      self.failUnless(isinstance(res,Data),"wrong type of result.")
      self.failUnlessEqual(res.getShape(),(),"wrong shape of result.")
      self.failUnless(Lsup(res-ref)<=self.RES_TOL*Lsup(ref),"wrong result")

class Test_util_overloaded_binary_still_failing(Test_util2):
   """
   these overloaded operations still fail!

        - as float**Data is not implemented  (Manits 0000053)
        - wrong return value of Data binaries (Mantis 0000054) 
   """
   #+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   def test_pow_overloaded_float_rank0_constData_rank1(self):
      arg0=1.56847796034
      arg1=Data(numarray.array([1.668960941491038, 1.5438441811985528]),self.functionspace)
      res=arg0**arg1
      ref=Data(numarray.array([2.1195606529779085, 2.003494811618745]),self.functionspace)
      self.failUnless(isinstance(res,Data),"wrong type of result.")
      self.failUnlessEqual(res.getShape(),(2,),"wrong shape of result.")
      self.failUnless(Lsup(res-ref)<=self.RES_TOL*Lsup(ref),"wrong result")
   #+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   def test_pow_overloaded_float_rank0_constData_rank2(self):
      arg0=0.853668063781
      arg1=Data(numarray.array([[3.2486997556413488, 0.86400711652329565, 3.2884578380088287, 1.2334289335947253, 2.5487776044652506], [3.7635594458327879, 2.0892653973449962, 3.1786608578416762, 0.14432777693961937, 2.6022783227644193], [0.10738723325449236, 1.9315854964052719, 3.1596089936211107, 0.45131939265454701, 2.5221015795578148], [3.296601365878248, 0.11581499226281025, 3.8612489083924948, 4.5664152349145377, 1.2728985965946598]]),self.functionspace)
      res=arg0**arg1
      ref=Data(numarray.array([[0.59810674931764074, 0.87223445231801477, 0.59435632263408789, 0.82271593116798969, 0.66814547267568969], [0.55131803150166692, 0.71852942411582488, 0.60477125876633264, 0.97742422610323876, 0.66251382293516647], [0.98315347711569301, 0.73668002747870431, 0.60659694046203705, 0.93108516866399793, 0.67097133288368638], [0.59359104068625757, 0.98184343378610262, 0.54286252143570424, 0.48555362457985307, 0.81759440057278132]]),self.functionspace)
      self.failUnless(isinstance(res,Data),"wrong type of result.")
      self.failUnlessEqual(res.getShape(),(4, 5),"wrong shape of result.")
      self.failUnless(Lsup(res-ref)<=self.RES_TOL*Lsup(ref),"wrong result")
   #+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   def test_pow_overloaded_float_rank0_constData_rank3(self):
      arg0=0.357074915448
      arg1=Data(numarray.array([[[0.38518924057053733, 0.24038528636715123], [1.6360866883952314, 0.43029605106106911]], [[3.423417953608805, 4.2988721626759077], [1.2524682631274544, 1.1468077998964645]], [[0.58295721049108573, 3.824568580799006], [3.0208703291598176, 1.3499811271261073]], [[3.0953057377971902, 1.3094835373781], [3.6681512369867795, 4.6149094849966819]], [[2.7674281276596542, 1.1256247920546796], [1.8871491353554231, 1.9524702935330096]], [[1.7534659804811799, 0.081583735919993849], [0.44546937195258135, 0.72867650813344209]]]),self.functionspace)
      res=arg0**arg1
      ref=Data(numarray.array([[[0.67255485268654747, 0.78071033102668452], [0.18547078622777072, 0.6420282101483078]], [[0.029438000064868096, 0.01195004623087142], [0.27532476037344766, 0.30697352764374525]], [[0.54862782939462229, 0.019475895313555789], [0.044559875252802042, 0.24901961219972699]], [[0.041271801415451524, 0.25962452868686592], [0.022879873935515389, 0.0086302791912317954]], [[0.057848702603705324, 0.31374356524419156], [0.14321555126374733, 0.13389855839543346]], [[0.16435341072630344, 0.91941680244570745], [0.63207408549094379, 0.4721785335572487]]]),self.functionspace)
      self.failUnless(isinstance(res,Data),"wrong type of result.")
      self.failUnlessEqual(res.getShape(),(6, 2, 2),"wrong shape of result.")
      self.failUnless(Lsup(res-ref)<=self.RES_TOL*Lsup(ref),"wrong result")
   #+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   def test_pow_overloaded_float_rank0_constData_rank4(self):
      arg0=2.54914513421
      arg1=Data(numarray.array([[[[1.270326259520153, 2.703299648777842, 4.8868346033905601, 2.3051326073096781], [3.2104019478564068, 4.817944979076735, 3.3247417263409202, 0.98796021017567104], [2.8880422572281463, 3.785353186109202, 1.1117179336379168, 4.9472947776437248]], [[3.5602918771225234, 3.5282818484304541, 2.8402575218661106, 1.6543365821874987], [0.16084877586377758, 0.89809582383337261, 1.3182980703805016, 3.3178012989111392], [3.2028874945099108, 2.5648139049109338, 4.8955284805714774, 3.5432022149852789]]], [[[1.1618978717086723, 2.7972414378300354, 3.8402714340316493, 1.6001498069270363], [1.087268582711364, 1.737265734430502, 3.6813919135947497, 4.3853914265325047], [1.88609441539519, 0.76524872642235886, 3.6761434732611624, 4.3321470021505437]], [[3.8235160205264509, 1.4456401748290546, 4.9173755965598991, 0.81975566563269575], [3.2061466542685157, 4.1636780468474139, 4.6306556567028174, 2.5296462390778065], [4.845909777306229, 0.57810028049528195, 1.1669649534267978, 4.7820404580882832]]], [[[2.1014714421841116, 2.4894332649036168, 2.3637808594117877, 0.5984928521048446], [3.4559984949550633, 2.686579205418913, 2.2483997830542748, 0.020742963721536667], [4.9435995803226715, 3.1161994022593595, 1.4907525564149176, 2.7151195320092083]], [[4.9609626993990288, 3.2550482319119549, 2.756546888804754, 4.1177301026436313], [3.2969135888482954, 1.1694652654802464, 3.5049458620366489, 2.086655354958836], [1.305388807910512, 4.1439092644148641, 4.1618527860945553, 0.27563591561125417]]]]),self.functionspace)
      res=arg0**arg1
      ref=Data(numarray.array([[[[3.2828699942440989, 12.548917902286258, 96.823997850208443, 8.6455645449807932], [20.169256775933832, 90.779260772485557, 22.446930364732179, 2.5205867988213329], [14.917109221466619, 34.541958014986271, 2.8300631290510951, 102.45984550866767]], [[27.982290424490554, 27.156547644185704, 14.264784430300786, 4.7023331158783899], [1.162433367658529, 2.3172946523503977, 3.433595788127132, 22.301619783037562], [20.027929872633244, 11.023675606380118, 97.614909291888679, 27.538362597921434]]], [[[2.9661217756338676, 13.701992022280693, 36.363479213061616, 4.469842090112488], [2.7660500872410752, 5.0817740136946528, 31.339903588247452, 60.561863519575425], [5.8411520949429976, 2.0464112817180369, 31.186362199170709, 57.618372322207513]], [[35.797782115214986, 3.8681232751319539, 99.631049282802593, 2.1534968710662095], [20.089103989632303, 49.214844611560132, 76.185667975322431, 10.666807966518682], [93.186153150784094, 1.7176584513241597, 2.9802192231638229, 87.779931945141115]]], [[[7.1454003570562259, 10.272878285306508, 9.1332992663966657, 1.7507504263789035], [25.380431205406289, 12.354101945587495, 8.1985574857144279, 1.0196000020407845], [102.10617058325805, 18.467431327194475, 4.0349088932001909, 12.688486277704584]], [[103.77870947420652, 21.029739940364855, 13.190026463287207, 47.143641282881781], [21.869948257238978, 2.9872001672429778, 26.569962421375287, 7.0470181501669806], [3.3923676531460898, 48.312798275660654, 49.130857281409916, 1.2942463156201212]]]]),self.functionspace)
      self.failUnless(isinstance(res,Data),"wrong type of result.")
      self.failUnlessEqual(res.getShape(),(3, 2, 3, 4),"wrong shape of result.")
      self.failUnless(Lsup(res-ref)<=self.RES_TOL*Lsup(ref),"wrong result")
   #+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   def test_pow_overloaded_float_rank0_expandedData_rank0(self):
      arg0=0.672041586744
      msk_arg1=whereNegative(self.functionspace.getX()[0]-0.5)
      arg1=msk_arg1*(2.50085945333)+(1.-msk_arg1)*(0.714228495159)
      res=arg0**arg1
      msk_ref=whereNegative(self.functionspace.getX()[0]-0.5)
      ref=msk_ref*(0.370119550813)+(1.-msk_ref)*(0.752872459895)
      self.failUnless(isinstance(res,Data),"wrong type of result.")
      self.failUnlessEqual(res.getShape(),(),"wrong shape of result.")
      self.failUnless(Lsup(res-ref)<=self.RES_TOL*Lsup(ref),"wrong result")
   #+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   def test_pow_overloaded_float_rank0_expandedData_rank1(self):
      arg0=3.83643456658
      msk_arg1=whereNegative(self.functionspace.getX()[0]-0.5)
      arg1=msk_arg1*numarray.array([2.4649528119460604, 4.4015773579432063])+(1.-msk_arg1)*numarray.array([2.5050056042267443, 3.1185274699598557])
      res=arg0**arg1
      msk_ref=whereNegative(self.functionspace.getX()[0]-0.5)
      ref=msk_ref*numarray.array([27.501378365433037, 371.70924493690819])+(1.-msk_ref)*numarray.array([29.02300655036413, 66.220868195790914])
      self.failUnless(isinstance(res,Data),"wrong type of result.")
      self.failUnlessEqual(res.getShape(),(2,),"wrong shape of result.")
      self.failUnless(Lsup(res-ref)<=self.RES_TOL*Lsup(ref),"wrong result")
   #+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   def test_pow_overloaded_float_rank0_expandedData_rank2(self):
      arg0=1.13471850643
      msk_arg1=whereNegative(self.functionspace.getX()[0]-0.5)
      arg1=msk_arg1*numarray.array([[0.58912677090382137, 0.93983668514090013, 2.0122074986633924, 4.4727320622173794, 1.2940493913793865], [0.41905245533976337, 1.7214742795801785, 3.3690628746243316, 4.3264805098978441, 2.2821866717410533], [3.5873589494704259, 4.5154183135527708, 0.67571874016825018, 0.49739384193149055, 2.297552452346038], [2.4711308013943767, 0.035889491188690691, 0.56766133369174721, 2.9251085245873232, 4.0512786424163183]])+(1.-msk_arg1)*numarray.array([[4.3857003148743008, 4.7692907184605806, 2.1049865113290753, 4.7253126157148948, 4.6473956540399657], [4.7007738365129486, 3.4364353832802648, 1.8949658645375465, 4.6712667420053657, 3.7857807635261378], [3.2496714869217667, 1.8954007017372452, 1.414346327611854, 1.8982366357369476, 4.2242475882140065], [2.9895955236808205, 4.0056998573414901, 4.0987731208544638, 2.6341113165055616, 0.83739250662962583]])
      res=arg0**arg1
      msk_ref=whereNegative(self.functionspace.getX()[0]-0.5)
      ref=msk_ref*numarray.array([[1.0772985404874016, 1.1261231478052052, 1.2895741613114386, 1.7599483980551891, 1.1776817946355922], [1.0543893459992322, 1.24304977052763, 1.5308110476569741, 1.7277164132177647, 1.3343353041969708], [1.5736329944297045, 1.7694687751144236, 1.0891531288355194, 1.0648808615781149, 1.3369290986616456], [1.3665822272329975, 1.0045461819546835, 1.0743798971224092, 1.4472839975883616, 1.6686572548284615]])+(1.-msk_ref)*numarray.array([[1.7406959700520956, 1.8271638189402759, 1.3047845032528298, 1.8170363298534116, 1.7992308696652106], [1.8114098407754482, 1.5439013229774907, 1.2706067196110626, 1.8046672342912449, 1.6135946927617708], [1.5078856800121894, 1.2706765499191768, 1.1957236972774039, 1.2711320654123079, 1.7055367599969291], [1.459127798590762, 1.6590726589695033, 1.6787036103748865, 1.3950233126781093, 1.1116368001118335]])
      self.failUnless(isinstance(res,Data),"wrong type of result.")
      self.failUnlessEqual(res.getShape(),(4, 5),"wrong shape of result.")
      self.failUnless(Lsup(res-ref)<=self.RES_TOL*Lsup(ref),"wrong result")
   #+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   def test_pow_overloaded_float_rank0_expandedData_rank3(self):
      arg0=4.69671679008
      msk_arg1=whereNegative(self.functionspace.getX()[0]-0.5)
      arg1=msk_arg1*numarray.array([[[4.1109492881321765, 1.5337005632312695], [4.3407884519530366, 4.6530992322911136]], [[2.5646089934744452, 2.519089903454621], [1.4887838873084611, 0.34112905345492628]], [[3.5362317357059716, 3.8706128693651469], [1.1870955077990113, 4.4747271870138459]], [[1.188825141228715, 0.62354589843593555], [2.2744424942987802, 2.6498967536945086]], [[1.900681769034668, 2.9685327729104474], [4.9963790460373829, 1.7061294551675452]], [[2.5090791675722377, 2.1621870068681837], [3.4768663195477485, 4.2757225442582376]]])+(1.-msk_arg1)*numarray.array([[[0.55947048241267894, 3.892675918272622], [2.0928980801633474, 3.4373418158231508]], [[3.1001717026044253, 0.2732937451339511], [1.3529965739226828, 2.942372355807342]], [[0.19566839701147096, 3.5887158621765871], [2.74664233625374, 0.52321381674739664]], [[4.8284101588082473, 3.8297905260356648], [0.37716052032457881, 1.5885464889650818]], [[1.1563227169890045, 2.9607643976148381], [3.2225447832954872, 1.3146888053450296]], [[3.6572582485321434, 3.2335045378392566], [0.55266266203484782, 1.6820417984729208]]])
      res=arg0**arg1
      msk_ref=whereNegative(self.functionspace.getX()[0]-0.5)
      ref=msk_ref*numarray.array([[[577.71361183417048, 10.723372898515619], [824.36065818127418, 1336.3674293440472]], [[52.831128957427595, 49.239135886293603], [10.00360738339231, 1.6949956135969837]], [[237.4764687630614, 398.34185963465751], [6.2731270382215989, 1014.1368084784533]], [[6.2899333072924852, 2.6235818996943157], [33.725443528800987, 60.281732951333787]], [[18.917655727234941, 98.683288531378238], [2272.6854395211553, 14.001293771330797]], [[48.482529090228326, 28.349477896809432], [216.64029075415539, 745.42910675836185]]])+(1.-msk_ref)*numarray.array([[[2.3760146936426265, 412.1713290347119], [25.468135625976604, 203.7918870061259]], [[120.97006271287876, 1.5261499268949541], [8.1084059812258715, 94.76963162907046]], [[1.3534709167425627, 257.56041919364674], [70.013028937280922, 2.2464259022347384]], [[1752.664443764852, 373.9656614833155], [1.7921498298672001, 11.672841617856733]], [[5.9815136051294902, 97.504545603100766], [146.17983011754166, 7.6418857758763306]], [[286.36867675378357, 148.67917899777751], [2.351124714064428, 13.489200678865306]]])
      self.failUnless(isinstance(res,Data),"wrong type of result.")
      self.failUnlessEqual(res.getShape(),(6, 2, 2),"wrong shape of result.")
      self.failUnless(Lsup(res-ref)<=self.RES_TOL*Lsup(ref),"wrong result")
   #+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   def test_pow_overloaded_float_rank0_expandedData_rank4(self):
      arg0=3.30902145167
      msk_arg1=whereNegative(self.functionspace.getX()[0]-0.5)
      arg1=msk_arg1*numarray.array([[[[3.1725349657503203, 3.0845748500431833, 1.3251300629206579, 2.6569079873920867], [4.401133367358729, 0.2813959929768825, 1.7519650763785801, 2.6178805778238692], [1.8864041086569574, 3.7791551835731969, 1.3422457102105219, 0.2081604723265463]], [[2.1902428534420713, 3.0841633696278761, 2.9965222635048661, 2.2869374663249884], [2.7280695311253944, 3.9590303115195788, 3.1301491431538744, 4.182436518053791], [1.2260234819740716, 1.8242153795636069, 3.9401624249260436, 3.4349251048319336]]], [[[0.48861823801104354, 2.3131499879646946, 1.2086122619459398, 2.2409237938676974], [3.3117711127665297, 0.87018754086000127, 1.6709579147934865, 1.2893183225200897], [1.3390161693818554, 1.0108007072272069, 3.9414993282493, 1.4462150778696792]], [[3.7967170311866441, 2.0212015799940115, 3.8412381336762307, 2.4057808908249125], [0.26260712889943277, 1.5647590608685933, 2.3707533968847603, 2.6465667159258581], [4.4731993538103074, 3.7651741459195378, 3.1338509227608538, 3.5035590250769135]]], [[[3.3050973895841906, 4.0367399640154655, 2.4941086434787398, 0.39145638395278809], [1.0614822508511919, 4.5756493953902719, 0.475720968447504, 2.1319535282121014], [1.7670440121784567, 4.7448652509649039, 1.8204034323285305, 4.7703236186277493]], [[2.1011053202368353, 2.3665545270862758, 1.6073533542163603, 1.2600308196018239], [0.29247411729720818, 2.3684742025187231, 1.7771399367205181, 4.9668082864579608], [4.595426606005498, 1.6371273654479281, 1.5037601874471596, 3.78912031777574]]]])+(1.-msk_arg1)*numarray.array([[[[4.8327836291046573, 1.1638816644184602, 2.1517890444121353, 3.3263630206047123], [3.7647650305885212, 1.1608624440787043, 0.61940200250059252, 3.3853649131580092], [2.9385268975715064, 0.52662330688397618, 4.4799956940372914, 2.9996151641521429]], [[0.70044410456478112, 4.3945400808511978, 3.1252348972736086, 1.2688805783984476], [0.15599343791305745, 0.53599762707945264, 1.061576396298286, 2.5648049865803384], [0.19676100009146294, 0.32071368159627772, 4.7168617485783004, 0.11755525142074101]]], [[[3.7377217347115015, 2.9327497612552462, 1.8145282053559222, 0.56740904011144078], [1.4416030660600863, 4.1189600680246015, 2.1110973910715658, 3.5384856009199517], [0.95118141400261069, 2.4572939344524936, 0.54929526659505457, 2.9652939525377708]], [[2.0885234488307876, 0.25836797235157094, 2.6699858158100973, 3.3139553157584252], [2.6559413882762239, 1.2879435669486743, 3.3675769889236591, 4.237965236929135], [0.79625310217359302, 3.3583510561037029, 1.2582374986295783, 2.1398901148681815]]], [[[4.7482284899129752, 2.2293361019743232, 1.1930794286761943, 1.0124469031550984], [0.38197241933053727, 4.1981998835993846, 0.79098246624754098, 4.4325191244897875], [4.135761934042792, 2.5772295341335645, 2.2968196972889792, 0.4273068074746656]], [[1.1976128220324929, 1.7459294668094707, 0.96078177214101046, 4.0854494840822007], [3.2731005299433793, 0.82489742017210455, 4.9787451612568212, 1.3471667907126523], [4.343723010658568, 0.57644019147251058, 0.71342917305041831, 2.0211306038878543]]]])
      res=arg0**arg1
      msk_ref=whereNegative(self.functionspace.getX()[0]-0.5)
      ref=msk_ref*numarray.array([[[[44.541526569463137, 40.091495998956134, 4.8828067986685939, 24.032224484143175], [193.76135996199952, 1.4003654269209849, 8.1375846434933727, 22.935670648779361], [9.5579208088126997, 92.050334604168881, 4.9838450947922492, 1.2828648641369595]], [[13.748910875268686, 40.071759842941873, 36.082063760330911, 15.435490923052084], [26.168365475343812, 114.15801285560873, 42.338669214857362, 149.14562304631761], [4.3367450539559762, 8.8724583914528239, 111.60939851359214, 60.971817813198015]]], [[[1.7944637697607297, 15.927333847311955, 4.2473229866626534, 14.608551648073879], [52.616997841336541, 2.8329316847606481, 7.3857759810555583, 4.6779782048798051], [4.9646215069645256, 3.3520671162853026, 111.78809510460806, 5.644134732164181]], [[94.005281960103943, 11.230979059402637, 99.149346198163528, 17.794399015213255], [1.3692312903098052, 6.5043609053916684, 17.063949250753016, 23.736660466381913], [211.21260699785594, 90.523102907549571, 42.52663468713218, 66.190881211600896]]], [[[52.198465234023658, 125.28297981521477, 19.77822080158063, 1.597495779133167], [3.5616552748767889, 238.76083054241485, 1.7669814438499811, 12.822578271065522], [8.2857540079134431, 292.35093722860603, 8.8320781648818247, 301.39441025042674]], [[12.357868168826375, 16.978424976621678, 6.844486437856224, 4.5168689568684455], [1.4190531977552476, 17.01747238246455, 8.3864639284588733, 381.28365954211506], [244.47884719156841, 7.0927460162084532, 6.0464928235844093, 93.154587613024191]]]])+(1.-msk_ref)*numarray.array([[[[324.78475431172814, 4.0259540631320077, 13.130578736246635, 53.54383496616213], [90.478796459093957, 4.0114347064939215, 2.0984709533536985, 57.460940797659013], [33.662874823011009, 1.8779581626455863, 212.93737318668644, 36.215855509993467]], [[2.3121739758861097, 192.23862246855467, 42.090421189774155, 4.5649571677125911], [1.2052294205209508, 1.8991433322517872, 3.5620565513756124, 21.524252643769405], [1.2654838573469389, 1.4678266430201032, 282.71645200527132, 1.1510479483688476]]], [[[87.597643951351131, 33.430958374780801, 8.7702010825145305, 1.9718879873813866], [5.6130706904588212, 138.23621312261142, 12.50651858980023, 69.015953092721603], [3.1212502046915214, 18.925816809951922, 1.9296054663911439, 34.758580290537438]], [[12.173200204345505, 1.3623030451242739, 24.411277858111475, 52.754704457848213], [24.004442882739546, 4.6702887676119742, 56.25075494797089, 159.3928238308925], [2.5930590384710972, 55.633148887433734, 4.5071862303782986, 12.944938739392638]]], [[[293.5299119704776, 14.407381017935226, 4.1691054609354943, 3.3586769436892241], [1.5794682740857684, 151.98569970294611, 2.5767557720475129, 201.17700525164358], [141.04371902898262, 21.846663187868508, 15.619108363619203, 1.6675204530848913]], [[4.1917838851875393, 8.0790224539183537, 3.157314804255805, 132.80254310369224], [50.237611053976565, 2.6834829790067647, 386.76911099066751, 5.0132806634938518], [180.89686297334865, 1.9933140453396749, 2.3483825436453394, 11.230025210903007]]]])
      self.failUnless(isinstance(res,Data),"wrong type of result.")
      self.failUnlessEqual(res.getShape(),(3, 2, 3, 4),"wrong shape of result.")
      self.failUnless(Lsup(res-ref)<=self.RES_TOL*Lsup(ref),"wrong result")
   #+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   def test_pow_overloaded_float_rank0_taggedData_rank0(self):
      arg0=0.341812064896
      arg1=Data(4.30670501129,self.functionspace)
      arg1.setTaggedValue(1,3.64191179292)
      res=arg0**arg1
      ref=Data(0.00982109262741,self.functionspace)
      ref.setTaggedValue(1,0.0200490957504)
      self.failUnless(isinstance(res,Data),"wrong type of result.")
      self.failUnlessEqual(res.getShape(),(),"wrong shape of result.")
      self.failUnless(Lsup(res-ref)<=self.RES_TOL*Lsup(ref),"wrong result")
   #+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   def test_pow_overloaded_float_rank0_taggedData_rank1(self):
      arg0=2.71918984915
      arg1=Data(numarray.array([3.6696137015868118, 0.10205613053632596]),self.functionspace)
      arg1.setTaggedValue(1,numarray.array([2.0744420420962988, 0.15604693286805166]))
      res=arg0**arg1
      ref=Data(numarray.array([39.284863912783393, 1.1074833798224062]),self.functionspace)
      ref.setTaggedValue(1,numarray.array([7.9656207691933547, 1.1689419815449242]))
      self.failUnless(isinstance(res,Data),"wrong type of result.")
      self.failUnlessEqual(res.getShape(),(2,),"wrong shape of result.")
      self.failUnless(Lsup(res-ref)<=self.RES_TOL*Lsup(ref),"wrong result")
   #+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   def test_pow_overloaded_float_rank0_taggedData_rank2(self):
      arg0=4.4483395692
      arg1=Data(numarray.array([[2.4529759744254518, 0.7580664327558474, 3.8975000203490939, 3.4541350498163998, 2.9544320655798217], [0.51290472026154688, 4.8961671628638266, 3.2135251243961367, 2.2462684812562461, 0.40911836554684933], [0.49170040047395686, 1.6842515195955219, 1.3170700476689892, 4.1425294249616993, 3.5304507633256805], [3.947444976634118, 0.11124097097693472, 3.9923991533874283, 1.7047022614872385, 2.8975607347873065]]),self.functionspace)
      arg1.setTaggedValue(1,numarray.array([[1.367948443330212, 4.7927959789646231, 2.3429313409787804, 4.9593367032554436, 4.2265558126195799], [3.4900286322554974, 2.8595582096739616, 1.8509445481474043, 0.5564155380820085, 4.3906066414576292], [4.6766848736409896, 3.2257976245481674, 4.7778474579462165, 2.0847486832037205, 1.7898532863269634], [1.4356217054626659, 0.92982462549186062, 2.7611923306294219, 1.3646256492703579, 3.9619562449225665]]))
      res=arg0**arg1
      ref=Data(numarray.array([[38.905764567917963, 3.1001099299365245, 336.00942079583973, 173.36570774432349, 82.235011176537952], [2.150125347731461, 1491.7135365638999, 121.05990834187568, 28.577654722949994, 1.8415761361989709], [2.0831434757725034, 12.351746001040247, 7.1403924396906904, 484.37197478038973, 194.28124800097655], [362.01425445908728, 1.1806092113877813, 387.13717427618201, 12.734575745284882, 75.542765004391114]]),self.functionspace)
      ref.setTaggedValue(1,numarray.array([[7.7037353055247513, 1278.4402727482807, 33.012867631466662, 1639.1994369435079, 549.09152187901964], [182.90658566677064, 71.377234712123865, 15.840848073557623, 2.294390845812015, 701.42646220920267], [1075.0222789059023, 123.29780636599452, 1250.2326681663551, 22.455864839588571, 14.460363130951452], [8.5225002163523413, 4.0059949692425398, 61.630992150805056, 7.6656241919499521, 369.9404712831535]]))
      self.failUnless(isinstance(res,Data),"wrong type of result.")
      self.failUnlessEqual(res.getShape(),(4, 5),"wrong shape of result.")
      self.failUnless(Lsup(res-ref)<=self.RES_TOL*Lsup(ref),"wrong result")
   #+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   def test_pow_overloaded_float_rank0_taggedData_rank3(self):
      arg0=3.9897936579
      arg1=Data(numarray.array([[[2.889727106362701, 1.3623716131192851], [2.9916597036534092, 3.4367308950490072]], [[3.3335872229779913, 1.5322214297481302], [4.7494147088593879, 3.6775797357334379]], [[4.685707621980133, 4.2506337725961352], [0.15321167564939667, 2.5577348615722117]], [[3.6361368676688279, 4.2834060370927194], [0.37638972870613419, 0.39030513216716933]], [[3.4140922088424324, 3.6871164394652509], [3.9581489155415834, 3.3148416225122186]], [[0.35997218987828361, 2.6122533672231647], [1.5937950637694218, 1.5725008654108865]]]),self.functionspace)
      arg1.setTaggedValue(1,numarray.array([[[0.68058173710671155, 3.6604896813370336], [0.013910592130982523, 4.1640866365439138]], [[1.2628720805564884, 1.4181377509080724], [3.068393656986165, 2.3871991576613487]], [[2.6698285170199094, 1.4193369447656969], [0.46164979577366916, 2.1969148005263128]], [[1.2834781040798817, 3.0914843479670262], [3.6005580804044293, 4.3792710341621239]], [[3.7534608128402835, 2.9947985761366493], [0.53622360363926969, 2.6873369507418534]], [[1.1972181696962809, 3.2164783088599913], [3.0875364909104346, 1.0164281755144038]]]))
      res=arg0**arg1
      ref=Data(numarray.array([[[54.523379096101252, 6.5874573196153481], [62.782586289507627, 116.22643811597655]], [[100.76754870055993, 8.3327650318245841], [714.76427612640032, 162.19712216078111]], [[654.45235790561503, 358.44309402018331], [1.2361541272919527, 34.440692824698289]], [[153.15741747367267, 375.07206890646461], [1.6834164683828434, 1.7161452365879322]], [[112.64196040084123, 164.35171133165582], [239.13949905472657, 98.187345080054001]], [[1.6456044167800061, 37.139392797469], [9.0738550613751467, 8.8103889216644493]]]),self.functionspace)
      ref.setTaggedValue(1,numarray.array([[[2.564459775636728, 158.40645271869869], [1.0194350853765908, 317.98722907748692]], [[5.7401537744364708, 7.1159108619175973], [69.815604851474347, 27.201219423427464]], [[40.219309519822779, 7.127728606939832], [1.8942122717654939, 20.90436147123215]], [[5.9061806413967508, 72.082338184950487], [145.79980629661623, 428.27708277753862]], [[180.15400056508486, 63.05586809159955], [2.1001185365211592, 41.205606541304057]], [[5.2416591987144212, 85.692742263889059], [71.689639962920467, 4.0815296418305298]]]))
      self.failUnless(isinstance(res,Data),"wrong type of result.")
      self.failUnlessEqual(res.getShape(),(6, 2, 2),"wrong shape of result.")
      self.failUnless(Lsup(res-ref)<=self.RES_TOL*Lsup(ref),"wrong result")
   #+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   def test_pow_overloaded_float_rank0_taggedData_rank4(self):
      arg0=2.00273786518
      arg1=Data(numarray.array([[[[4.058085214998175, 1.6160604376121306, 3.392298211100885, 1.0452007249155919], [2.5273517683292699, 3.0494177512997704, 0.33690014082657282, 0.12528978101802304], [4.5197516324925351, 0.23226248241823033, 3.5664208665570021, 4.7732489962057079]], [[4.4549507097535654, 0.55109845956885817, 2.7253278884059697, 4.496324732480609], [1.586341752881361, 2.4991507441211032, 3.2436198792465789, 1.8861809587343012], [3.3856069070921238, 2.8132833163000455, 1.8557615185276446, 1.2515630764821439]]], [[[3.9029634096168433, 3.4238838330903647, 0.168526403580252, 2.6183355793979], [4.012617066171174, 3.1192068434369804, 1.5566641170696784, 4.2941982176726397], [1.4311511821530991, 3.0032853336479115, 1.5699302744706125, 3.3894002471002085]], [[2.0541298653067361, 0.041957272399514252, 3.9926465817541326, 2.9475866810662157], [3.6248456625161114, 2.5516057015658311, 0.10278482539076957, 4.5989676951872394], [1.3614783963249981, 3.4546685791640237, 1.0699048554869663, 2.0764514576328352]]], [[[2.2381377204691408, 3.8176327781348736, 0.90064790493230951, 4.6989234806143587], [2.1736357404880438, 3.1637074985297562, 3.1423445423973613, 3.5095763892793981], [2.5804729333454222, 0.89043093205556245, 0.3803504224422426, 1.8258865124301544]], [[4.8909917949317609, 4.6840264802036744, 4.5827397185651009, 2.325986597602165], [4.6542743462709959, 4.3602860031505459, 1.9718692294021449, 1.6727430742597078], [4.0944735363242009, 3.4603064237310965, 3.0807314616342185, 1.2081156932037695]]]]),self.functionspace)
      arg1.setTaggedValue(1,numarray.array([[[[2.7870810246480366, 2.0753323950124378, 1.6206774020250057, 1.6693164535030807], [0.22495732068957769, 1.4826277903660066, 1.6654536412954513, 3.7011690087174687], [2.8688237888109613, 4.131833355592474, 0.58743394654963033, 3.1170748037096057]], [[2.2316988612632387, 4.8338802120706443, 1.1830725418117809, 2.2558696691891198], [0.1828771012820089, 1.6175444987052734, 2.0276687081781493, 2.1722295435556092], [1.7526525704637648, 2.5413810371959822, 1.2335871661409938, 3.1822782500328279]]], [[[4.8612724338078301, 0.7675517686181661, 2.175206221217064, 0.64847845944287819], [2.7137589100492434, 3.4524463464048054, 1.6167573912489441, 0.16158562025267317], [4.0907368103502781, 1.468174509078422, 3.8588398986692334, 4.8128173918988981]], [[0.15544289923049187, 3.7268756946718211, 2.9147996475422486, 3.6294512035901425], [4.29461385610918, 4.4680526078126217, 0.060265276226098695, 2.316238253151544], [3.1881249924409127, 0.73957031720346222, 0.085295929508293178, 2.3155430777698207]]], [[[1.5940476159630346, 2.2712827258710178, 0.24608800148241564, 3.9476270044554274], [0.31300686021970531, 1.1763364711836799, 0.76136985552643244, 3.8726250916336724], [2.6397833251035379, 3.9748466473005739, 3.8288284623159541, 2.0374679133608966]], [[0.47517472829931101, 4.0767363652225068, 0.52520993483385048, 1.5660106079134684], [1.6486334521439767, 0.28245826998834422, 4.2578225614767797, 4.047421985445852], [3.2269014632928079, 3.0865258235349415, 4.5301478170774354, 3.4909708031440467]]]]))
      res=arg0**arg1
      ref=Data(numarray.array([[[[16.750058845591397, 3.0721526379826374, 10.548699609572795, 2.0666062446181979], [5.7850914043791031, 8.3133853562698778, 1.2636220765441493, 1.0909137572305785], [23.081607888717194, 1.1750489650261489, 11.904701658561461, 27.524977675184346]], [[22.065843738793575, 1.4663058992285734, 6.6378066215549509, 22.709100960148394], [3.0093930782835683, 5.6728867856068153, 9.5137986534737795, 3.7061044164408381], [10.499791295855353, 7.055926912267954, 3.628627724725646, 2.385072570137162]]], [[[15.039308247944584, 10.782659979803462, 1.1241690550540289, 6.1624461340705841], [16.229384415515096, 8.7262567079767237, 2.9479996476896653, 19.734844610529727], [2.7019026677952165, 8.0512490841275621, 2.9752866951568713, 10.527489804478877]], [[4.1646172182532135, 1.0295686853650003, 16.005839869392002, 7.7457451954453456], [12.397692008655882, 5.8833650491927445, 1.0739953013827201, 24.387064856692479], [2.5742736313898518, 11.015680522547886, 2.1023697483406334, 4.2296830097635185]]], [[[4.7323438821950701, 14.173923521480578, 1.8692058081325049, 26.140182058429897], [4.5250245885470779, 9.0001646657529406, 8.8676158851323628, 11.443868303875972], [6.0025094057468991, 1.8559891647626132, 1.3023354347347804, 3.5541143150273866]], [[29.870400260851106, 25.871124951438134, 24.113751679060641, 5.0300654379866412], [25.342027194461846, 20.661763233119618, 3.9333563988776534, 3.1955060146113112], [17.178764738937865, 11.058897730257282, 8.4961639056616765, 2.3141783138707765]]]]),self.functionspace)
      ref.setTaggedValue(1,numarray.array([[[[6.9286852125785368, 4.2263969519979208, 3.0820194655202178, 3.1879102631117733], [1.1691023982577264, 2.8002466491192726, 3.1793692577325836, 13.072594935061897], [7.3334176438266114, 17.6303345843294, 1.503779779154625, 8.7133450051819885]], [[4.7112286301679207, 28.708784932966971, 2.2742761723956582, 4.7909835415904132], [1.1354294192926007, 3.0753207471873583, 4.0887802238112432, 4.5206074931782005], [3.3778648551221635, 5.8417341771948292, 2.3554811322786962, 9.1169976252798239]]], [[[29.260178173166832, 1.7041668707386901, 4.529962828226199, 1.5689053267278315], [6.5846866575461958, 10.998692316424849, 3.0736400576927507, 1.1187630615338042], [17.134239987654585, 2.7722783164873048, 14.585426938009, 28.291876751816698]], [[1.1140003466430173, 13.308084833978867, 7.5713591317368989, 12.437410980682518], [19.740542235230951, 22.267547463257269, 1.042743425368402, 4.9961250421125465], [9.1540938403147862, 1.6713685966647158, 1.0610291332840969, 4.9937134459319727]]], [[[3.0255420398866604, 4.8425445904748727, 1.1863861667386619, 15.513131639293059], [1.2428262437334108, 2.2636612679395824, 1.6968658298480934, 14.725738793712299], [6.254927688836438, 15.809188519363234, 14.284563479653341, 4.1167021282117702]], [[1.3909904683391461, 16.968442210027412, 1.4401773573943464, 2.9671981822001441], [3.1424443542515723, 1.2167356249730308, 19.242519541811749, 16.626469883428879], [9.4039709445950059, 8.530423671641044, 23.248867311915969, 11.296943532689685]]]]))
      self.failUnless(isinstance(res,Data),"wrong type of result.")
      self.failUnlessEqual(res.getShape(),(3, 2, 3, 4),"wrong shape of result.")
      self.failUnless(Lsup(res-ref)<=self.RES_TOL*Lsup(ref),"wrong result")

if __name__ == '__main__':
   suite = unittest.TestSuite()
   suite.addTest(unittest.makeSuite(Test_util2))
   suite.addTest(unittest.makeSuite(Test_util_binary_still_failing))
   suite.addTest(unittest.makeSuite(Test_util_overloaded_binary_still_failing))
   s=unittest.TextTestRunner(verbosity=2).run(suite)


