from esys.pyvisi import Scene
from esys.pyvisi.constant import *
import unittest, os
from stat import ST_SIZE

try:
	PYVISI_WORKDIR=os.environ['PYVISI_WORKDIR']
except KeyError:
	PYVISI_WORKDIR='.'
try:
	PYVISI_TEST_DATA_ROOT=os.environ['PYVISI_TEST_DATA_ROOT']
except KeyError:
	PYVISI_TEST_DATA_ROOT='.'

PYVISI_TEST_SCENE_REFERENCE_IMAGES_PATH = os.path.join(PYVISI_TEST_DATA_ROOT, \
		"data_reference_images", "scene")
PYVISI_TEST_SCENE_IMAGES_PATH = os.path.join(PYVISI_WORKDIR, \
		"data_sample_images", "scene")

MIN_IMAGE_SIZE = 100
X_SIZE = 400 
Y_SIZE = 400
JPG_RENDERER = Renderer.OFFLINE_JPG

class TestScene:
	def render(self, file):
		self.scene.render(image_name = \
				os.path.join(PYVISI_TEST_SCENE_IMAGES_PATH, file))

		self.failUnless(os.stat(os.path.join(PYVISI_TEST_SCENE_IMAGES_PATH, \
				file))[ST_SIZE] > MIN_IMAGE_SIZE)
	
	def setBackground(self, c):
		self.scene.setBackground(color = c)

class TestSceneOneViewport(unittest.TestCase, TestScene):
	def setUp(self): 
		self.scene = \
				Scene(renderer = JPG_RENDERER, num_viewport = 1, 
				x_size = X_SIZE, y_size = Y_SIZE)

	def tearDown(self):
		del self.scene
		
	def testRender(self):
		file = "TestSceneOneViewport_testRender.jpg"
		self.render(file)
	
	def testSetBackground(self):
		self.setBackground(Color.GREEN)
		file = "TestSceneOneViewport_testSetBackground.jpg"
		self.render(file)

class TestSceneFourViewports(unittest.TestCase, TestScene):
	def setUp(self):
		self.scene = \
				Scene(renderer = JPG_RENDERER, num_viewport = 4, x_size = 800, 
				y_size = 800) 

	def tearDown(self):
		del self.scene

	def testRender(self):
		file = "TestSceneFourViewport_testRender.jpg"
		self.render(file)
	

###############################################################################


if __name__ == '__main__':
	suite = unittest.TestSuite()
	suite.addTest(unittest.TestLoader().loadTestsFromTestCase(TestSceneOneViewport))
	suite.addTest(unittest.TestLoader().loadTestsFromTestCase(TestSceneFourViewports))
	unittest.TextTestRunner(verbosity=2).run(suite)
