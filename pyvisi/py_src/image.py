"""
@author: John Ngui
@author: Lutz Gross
"""

import vtk
from common import *

class Image(Common):
	"""
	Class that displays an image.
	"""

	def __init__(self, scene, format):
		"""
		@type scene: L{Scene <scene.Scene>} object
		@param scene: Scene in which components are to be added to
		@type format: String
		@param format: Format of the image (i.e. jpg)
		"""

		Common.__init__(self, scene)
		self.vtk_image_reader = self.determineReader(format) 
		self.vtk_texture = None
		self.vtk_plane = None

	def determineReader(self, format):
		"""
		Determines the image format and returns the corresponding image reader.

		@type format: String
		@param format: Format of the image (i.e. jpg)
		@rtype: vtkImageReader2 (i.e. vtkJPEGReader)
		@return: VTK image reader
		"""

		if(format == "jpg"):
			return vtk.vtkJPEGReader()	
		elif(format == "bmp"):
			return vtk.vtkBMPReader()

	def setFileName(self, file_name):
		"""
		Set the file name, and setup the mapper as well as the actor.	
		
		@type file_name: String
		@param file_name: Image file name
		"""

		self.vtk_image_reader.SetFileName(file_name)

		self.setTexture()
		self.setPlane()

		Common.setMapper(self, "self.vtk_plane.GetOutput()")
		Common.setActor(self)
		Common.setTexture(self, self.vtk_texture)
		Common.addActor(self)

	def setTexture(self):
		"""
		Set the texture map.	
		"""

		self.vtk_texture = vtk.vtkTexture()
		self.vtk_texture.SetInput(self.vtk_image_reader.GetOutput())	

	def setPlane(self):
		"""
		Set the texture coordinates (generated from the plane), which controls 
		the positioning of the texture on a surface.
		"""

		self.vtk_plane = vtk.vtkPlaneSource()
		


