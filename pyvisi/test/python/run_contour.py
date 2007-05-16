from esys.pyvisi import DataCollector, Scene, Contour
from esys.pyvisi.constant import *
import unittest, os
from stat import ST_SIZE

<<<<<<< .mine
try:
	PYVISI_WORKDIR=os.environ['PYVISI_WORKDIR']
except KeyError:
	PYVISI_WORKDIR='.'
try:
	PYVISI_TEST_DATA_ROOT=os.environ['PYVISI_TEST_DATA_ROOT']
except KeyError:
	PYVISI_TEST_DATA_ROOT='.'

PYVISI_TEST_MESHES_PATH = os.path.join(PYVISI_TEST_DATA_ROOT, "data_meshes")
PYVISI_TEST_CONTOUR_REFERENCE_IMAGES_PATH = os.path.join(PYVISI_TEST_DATA_ROOT,\
		"data_reference_images", "contour")
PYVISI_TEST_CONTOUR_IMAGES_PATH = os.path.join(PYVISI_WORKDIR, \
		"data_sample_images", "contour")

=======
try:
     PYVISI_WORKDIR=os.environ['PYVISI_WORKDIR']
except KeyError:
     PYVISI_WORKDIR='.'

try:
     PYVISI_TEST_DATA_ROOT=os.environ['PYVISI_TEST_DATA_ROOT']
except KeyError:
     PYVISI_TEST_DATA_ROOT='.'
 

     
PYVISI_TEST_MESHES_PATH = os.path.join(PYVISI_TEST_DATA_ROOT,"data_meshes")
PYVISI_TEST_CONTOUR_IMAGES_PATH = os.path.join(PYVISI_TEST_DATA_ROOT, "data_sample_images","contour")
>>>>>>> .r1142
MIN_IMAGE_SIZE = 100
FILE_2D = "interior_2D.xml"
X_SIZE = 400
Y_SIZE = 400
JPG_RENDERER = Renderer.OFFLINE_JPG

class TestContour(unittest.TestCase):
	def setUp(self):
		self.scene = \
				Scene(renderer = JPG_RENDERER, num_viewport = 1,
						x_size = X_SIZE, y_size = Y_SIZE)

		self.data_collector = DataCollector(source = Source.XML)
		self.data_collector.setFileName(file_name = \
<<<<<<< .mine
				os.path.join(PYVISI_TEST_MESHES_PATH, FILE_2D))
=======
				os.path.join(PYVISI_TEST_MESHES_PATH,FILE_2D))
>>>>>>> .r1142

		self.contour = Contour(scene = self.scene,
				data_collector = self.data_collector,
				viewport = Viewport.SOUTH_WEST, lut = Lut.COLOR,
				cell_to_point = False, outline = True)

	def tearDown(self):
		self.scene
		self.data_collector
		self.contour

	def render(self, file):
		self.scene.render(image_name = \
<<<<<<< .mine
				os.path.join(PYVISI_TEST_CONTOUR_IMAGES_PATH, file))
=======
		os.path.join(PYVISI_TEST_CONTOUR_IMAGES_PATH ,file))
>>>>>>> .r1142

<<<<<<< .mine
		self.failUnless(os.stat(os.path.join(PYVISI_TEST_CONTOUR_IMAGES_PATH, \
				file))[ST_SIZE] > MIN_IMAGE_SIZE)
=======
		self.failUnless(os.stat(os.path.join(PYVISI_TEST_CONTOUR_IMAGES_PATH ,file))[ST_SIZE] > MIN_IMAGE_SIZE)
>>>>>>> .r1142

	def testGenerateContours(self):
		self.contour.generateContours(5)
		self.render("TestContour_testGenerateContours.jpg")


##############################################################################


if __name__ == '__main__':
	suite = unittest.TestSuite()
	suite.addTest(unittest.TestLoader().loadTestsFromTestCase(TestContour))
	unittest.TextTestRunner(verbosity=2).run(suite)

