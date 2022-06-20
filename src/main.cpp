/*
 * see
 * https://devblogs.nvidia.com/egl-eye-opengl-visualization-without-x-server/
 */

#define MESA_EGL_NO_X11_HEADERS
#define EGL_NO_X11
#define EGL_EGLEXT_PROTOTYPES
#define MY_EGL_CHECK()                                                         \
  do {                                                                         \
    auto error = eglGetError();                                                \
    if (error != EGL_SUCCESS) {                                                \
      std::cout << "EGL error  before " << __LINE__ << " of " << __FILE__      \
                << ":  " << get_egl_error_info(error) << std::endl;            \
    }                                                                          \
  } while (0)

#include <glad/glad.h>
#include <fstream>
#include <iostream>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <GL/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform2.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#define _USE_MATH_DEFINES

#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

using namespace std;
using glm::mat4;
using glm::vec3;
GLuint g_vao;
GLuint g_programHandle;
const int dim_data = 512;
const int dim_img = 384;
const GLuint g_winWidth = 600;
const GLuint g_winHeight = 600;
float theta, phi;
float weight_x, weight_y, weight_z;
GLuint g_frameBuffer;
// transfer function
GLuint g_tffTexObj;
GLuint g_bfTexObj;
GLuint g_volTexObj[3];
GLuint g_rcVertHandle;
GLuint g_rcFragHandle;
GLuint g_bfVertHandle;
GLuint g_bfFragHandle;
float g_stepSize = 0.001f;

char filename[1024];
char tf1d_filename[1024];
char tfid[1024];

constexpr int opengl_version[] = { 3, 3 };

constexpr EGLint config_attribs[] = { EGL_SURFACE_TYPE,
									 EGL_PBUFFER_BIT,
									 EGL_BLUE_SIZE,
									 8,
									 EGL_GREEN_SIZE,
									 8,
									 EGL_RED_SIZE,
									 8,
									 EGL_DEPTH_SIZE,
									 8,
									 EGL_RENDERABLE_TYPE,
									 EGL_OPENGL_BIT,
									 EGL_NONE };

constexpr EGLint pixel_buffer_attribs[] = {
	EGL_WIDTH, g_winWidth, EGL_HEIGHT, g_winHeight, EGL_NONE,
};

constexpr EGLint context_attris[] = { EGL_CONTEXT_MAJOR_VERSION,
									 opengl_version[0],
									 EGL_CONTEXT_MINOR_VERSION,
									 opengl_version[1],
									 EGL_CONTEXT_OPENGL_PROFILE_MASK,
									 EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
									 EGL_NONE };

EGLDisplay create_display_from_device();
void display(void);
void initVBO();
void initShader();
void initFrameBuffer(GLuint, GLuint, GLuint);
GLuint initTFF1DTex(const char* filename);
GLuint initFace2DTex(GLuint texWidth, GLuint texHeight);
GLuint initVol3DTex(const char* filename, GLuint width, GLuint height, GLuint depth, int idx);
void render(GLenum cullFace);
const char *get_egl_error_info(EGLint error);

int main(int argc, char *argv[]) {
	// 1. Initialize EGL
	auto egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (egl_display == EGL_NO_DISPLAY) {
		cout << "No default display, try to create a display "
			"from devices."
			<< endl;
		// try EXT_platform_device, see
		// https://www.khronos.org/registry/EGL/extensions/EXT/EGL_EXT_platform_device.txt
		egl_display = create_display_from_device();
	}

	EGLint major = 0, minor = 0;
	eglInitialize(egl_display, &major, &minor);
// 	cout << "EGL version: " << major << "." << minor << endl;
// 	cout << "EGL vendor string: " << eglQueryString(egl_display, EGL_VENDOR) << endl;
	MY_EGL_CHECK();

	// 2. Select an appropriate configuration
	EGLint num_configs = 0;
	EGLConfig egl_config = nullptr;
	eglChooseConfig(egl_display, config_attribs, &egl_config, 1, &num_configs);
	MY_EGL_CHECK();

	// 3. Create a surface
	auto egl_surface =
		eglCreatePbufferSurface(egl_display, egl_config, pixel_buffer_attribs);
	MY_EGL_CHECK();

	// 4. Bind the API
	eglBindAPI(EGL_OPENGL_API);
	MY_EGL_CHECK();

	// 5. Create a context and make it current
	auto egl_ctx =
		eglCreateContext(egl_display, egl_config, EGL_NO_CONTEXT, context_attris);
	MY_EGL_CHECK();

	eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_ctx);
	MY_EGL_CHECK();

	// from now on use your OpenGL context
	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(&eglGetProcAddress))) {
		cout << "Load OpenGL " << opengl_version[0] << "." << opengl_version[1]
			<< "failed" << endl;
		return -1;
	}

// 	cout << "OpenGL version: " << GLVersion.major << "." << GLVersion.minor << endl;

	// render and save
	initVBO();
	initShader();
	sprintf(tfid, "%s", argv[2]);
	sprintf(tf1d_filename, "../res/TF1D/nyx-%s.TF1D", tfid);
	g_tffTexObj = initTFF1DTex(tf1d_filename);
	g_bfTexObj = initFace2DTex(g_winWidth, g_winHeight);
	
	sprintf(filename, argv[1]);
	cout << filename << endl;
	
	char input_path_x[1024];
	sprintf(input_path_x, "/fs/project/PAS0027/nyx_vdl/512/vpx/pred/%s.bin", filename);
	g_volTexObj[0] = initVol3DTex(input_path_x, dim_data, dim_img, dim_img, 0);
	
	char input_path_y[1024];
	sprintf(input_path_y, "/fs/project/PAS0027/nyx_vdl/512/vpy/pred/%s.bin", filename);
	g_volTexObj[1] = initVol3DTex(input_path_y, dim_img, dim_data, dim_img, 1);
	
	char input_path_z[1024];
	sprintf(input_path_z, "/fs/project/PAS0027/nyx_vdl/512/vpz/pred/%s.bin", filename);
	g_volTexObj[2] = initVol3DTex(input_path_z, dim_img, dim_img, dim_data, 2);
	
	initFrameBuffer(g_bfTexObj, g_winWidth, g_winHeight);
	display();

	// 6. Terminate EGL when finished
	eglTerminate(egl_display);
	return 0;
}

EGLDisplay create_display_from_device() {
	auto eglGetPlatformDisplayEXT =
		reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
			eglGetProcAddress("eglGetPlatformDisplayEXT"));
	if (eglGetPlatformDisplayEXT == nullptr) {
		cerr << "eglGetPlatformDisplayEXT not found" << endl;
		return EGL_NO_DISPLAY;
	}

	auto eglQueryDevicesEXT = reinterpret_cast<PFNEGLQUERYDEVICESEXTPROC>(
		eglGetProcAddress("eglQueryDevicesEXT"));
	if (eglQueryDevicesEXT == nullptr) {
		cerr << "eglQueryDevicesEXT not found" << endl;
		return EGL_NO_DISPLAY;
	}

	EGLint max_devices = 0, num_devices = 0;
	eglQueryDevicesEXT(0, nullptr, &max_devices);

	auto devices = new EGLDeviceEXT[max_devices];
	eglQueryDevicesEXT(max_devices, devices, &num_devices);

	EGLint major = 0, minor = 0;
	cout << "Detected " << num_devices << " devices." << endl;
	for (int i = 0; i < num_devices; ++i) {
		auto display =
			eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, devices[i], nullptr);
		if (!eglInitialize(display, &major, &minor)) {
			cout << "  Failed to initialize EGL with device " << i << endl;
			continue;
		}

		cout << "  Device #" << i << " is used." << endl;
		return display;
	}

	return EGL_NO_DISPLAY;
}

// init the vertex buffer object
void initVBO() {
	GLfloat vertices[24] = {
		0.0, 0.0, 0.0,
		0.0, 0.0, 1.0,
		0.0, 1.0, 0.0,
		0.0, 1.0, 1.0,
		1.0, 0.0, 0.0,
		1.0, 0.0, 1.0,
		1.0, 1.0, 0.0,
		1.0, 1.0, 1.0
	};

	// draw the six faces of the boundbox by drawing triangles
	// draw it contra-clockwise
	// front: 1 5 7 3
	// back: 0 2 6 4
	// left: 0 1 3 2
	// right:7 5 4 6    
	// up: 2 3 7 6
	// down: 1 0 4 5
	GLuint indices[36] = {
		1,5,7,
		7,3,1,
		0,2,6,
		6,4,0,
		0,1,3,
		3,2,0,
		7,5,4,
		4,6,7,
		2,3,7,
		7,6,2,
		1,0,4,
		4,5,1
	};
	GLuint gbo[2];

	glGenBuffers(2, gbo);
	GLuint vertexdat = gbo[0];
	GLuint veridxdat = gbo[1];
	glBindBuffer(GL_ARRAY_BUFFER, vertexdat);
	glBufferData(GL_ARRAY_BUFFER, 24 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);
	// used in glDrawElement()
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, veridxdat);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 36 * sizeof(GLuint), indices, GL_STATIC_DRAW);

	GLuint vao;
	glGenVertexArrays(1, &vao);
	// vao like a closure binding 3 buffer object: verlocdat vercoldat and veridxdat
	glBindVertexArray(vao);
	glEnableVertexAttribArray(0); // for vertexloc
	glEnableVertexAttribArray(1); // for vertexcol

	// the vertex location is the same as the vertex color
	glBindBuffer(GL_ARRAY_BUFFER, vertexdat);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (GLfloat *)NULL);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (GLfloat *)NULL);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, veridxdat);
	// glBindVertexArray(0);
	g_vao = vao;
}

void drawBox(GLenum glFaces) {
	glEnable(GL_CULL_FACE);
	glCullFace(glFaces);
	glBindVertexArray(g_vao);
	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (GLuint *)NULL);
	glDisable(GL_CULL_FACE);
}
// check the compilation result
GLboolean compileCheck(GLuint shader){
	GLint err;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &err);
	if (GL_FALSE == err)
	{
		GLint logLen;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
		if (logLen > 0)
		{
			char* log = (char *)malloc(logLen);
			GLsizei written;
			glGetShaderInfoLog(shader, logLen, &written, log);
			cerr << "Shader log: " << log << endl;
			free(log);
		}
	}
	return err;
}
// init shader object
GLuint initShaderObj(const GLchar* srcfile, GLenum shaderType) {
	ifstream inFile(srcfile, ifstream::in);
	// use assert?
	if (!inFile)
	{
		cerr << "Error openning file: " << srcfile << endl;
		exit(EXIT_FAILURE);
	}

	const int MAX_CNT = 10000;
	GLchar *shaderCode = (GLchar *)calloc(MAX_CNT, sizeof(GLchar));
	inFile.read(shaderCode, MAX_CNT);
	if (inFile.eof())
	{
		size_t bytecnt = inFile.gcount();
		*(shaderCode + bytecnt) = '\0';
	}
	else if (inFile.fail())
	{
		cout << srcfile << "read failed " << endl;
	}
	else
	{
		cout << srcfile << "is too large" << endl;
	}
	// create the shader Object
	GLuint shader = glCreateShader(shaderType);
	if (0 == shader)
	{
		cerr << "Error creating vertex shader." << endl;
	}
	// cout << shaderCode << endl;
	// cout << endl;
	const GLchar* codeArray[] = { shaderCode };
	glShaderSource(shader, 1, codeArray, NULL);
	free(shaderCode);

	// compile the shader
	glCompileShader(shader);
	if (GL_FALSE == compileCheck(shader))
	{
		cerr << "shader compilation failed" << endl;
	}
	return shader;
}
GLint checkShaderLinkStatus(GLuint pgmHandle){
	GLint status;
	glGetProgramiv(pgmHandle, GL_LINK_STATUS, &status);
	if (GL_FALSE == status)
	{
		GLint logLen;
		glGetProgramiv(pgmHandle, GL_INFO_LOG_LENGTH, &logLen);
		if (logLen > 0)
		{
			GLchar * log = (GLchar *)malloc(logLen);
			GLsizei written;
			glGetProgramInfoLog(pgmHandle, logLen, &written, log);
			cerr << "Program log: " << log << endl;
		}
	}
	return status;
}
// link shader program
GLuint createShaderPgm() {
	// Create the shader program
	GLuint programHandle = glCreateProgram();
	if (0 == programHandle) {
		cerr << "Error create shader program" << endl;
		exit(EXIT_FAILURE);
	}
	return programHandle;
}

// init the 1 dimentional texture for transfer function
GLuint initTFF1DTex(const char* filename) {
	FILE *fp = fopen(filename, "r");
	if (!(fp = fopen(filename, "rb"))) {
		cout << "Error: opening .tf1d file failed" << endl;
		exit(EXIT_FAILURE);
	}

	int keyNum;
	float thresholdL, thresholdU;
	fscanf(fp, "%d %f %f\n", &keyNum, &thresholdL, &thresholdU);

	float *intensity = new float[keyNum],
		*rl = new float[keyNum],
		*gl = new float[keyNum],
		*bl = new float[keyNum],
		*al = new float[keyNum],
		*rr = new float[keyNum],
		*gr = new float[keyNum],
		*br = new float[keyNum],
		*ar = new float[keyNum];
	for (int i = 0; i < keyNum; i++) {
		fscanf(fp, "%f %f %f %f %f %f %f %f %f", &intensity[i],
			&rl[i], &gl[i], &bl[i], &al[i], &rr[i], &gr[i], &br[i], &ar[i]);
	}

	int dimension = 256;
	float *transferFunction = new float[dimension * 4];

	int frontEnd = (int)floor(thresholdL * dimension + 0.5);
	int backStart = (int)floor(thresholdU * dimension + 0.5);
	//all values before front_end and after back_start are set to zero
	//all other values remain the same
	for (int x = 0; x < frontEnd; x++) {
		transferFunction[x * 4 + 0] = 0;
		transferFunction[x * 4 + 1] = 0;
		transferFunction[x * 4 + 2] = 0;
		transferFunction[x * 4 + 3] = 0;
	}

	float r, g, b, a;
	int keyIterator = 0;
	for (int x = frontEnd; x < backStart; x++) {
		float value = (float)x / (dimension - 1);
		while (keyIterator < keyNum && value > intensity[keyIterator])
			keyIterator++;
		if (keyIterator == 0) {
			r = rl[keyIterator];
			g = gl[keyIterator];
			b = bl[keyIterator];
			a = al[keyIterator];
		}
		else if (keyIterator == keyNum) {
			r = rr[keyIterator - 1];
			g = gr[keyIterator - 1];
			b = br[keyIterator - 1];
			a = ar[keyIterator - 1];
		}
		else {
			float fraction = (value - intensity[keyIterator - 1]) / (intensity[keyIterator] - intensity[keyIterator - 1]);
			r = rr[keyIterator - 1] + (rl[keyIterator] - rr[keyIterator - 1]) * fraction;
			g = gr[keyIterator - 1] + (gl[keyIterator] - gr[keyIterator - 1]) * fraction;
			b = br[keyIterator - 1] + (bl[keyIterator] - br[keyIterator - 1]) * fraction;
			a = ar[keyIterator - 1] + (al[keyIterator] - ar[keyIterator - 1]) * fraction;
		}

		transferFunction[x * 4 + 0] = r;
		transferFunction[x * 4 + 1] = g;
		transferFunction[x * 4 + 2] = b;
		transferFunction[x * 4 + 3] = a;
	}

	for (int x = backStart; x < dimension; x++) {
		transferFunction[x * 4 + 0] = 0;
		transferFunction[x * 4 + 1] = 0;
		transferFunction[x * 4 + 2] = 0;
		transferFunction[x * 4 + 3] = 0;
	}

	GLuint tff1DTex;
	glGenTextures(1, &tff1DTex);
	glBindTexture(GL_TEXTURE_1D, tff1DTex);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA16, dimension, 0, GL_RGBA, GL_FLOAT, transferFunction);

	delete[]intensity;
	delete[]rl;
	delete[]gl;
	delete[]bl;
	delete[]al;
	delete[]rr;
	delete[]gr;
	delete[]br;
	delete[]ar;
	delete[]transferFunction;
	return tff1DTex;
}

// init the 2D texture for render backface 'bf' stands for backface
GLuint initFace2DTex(GLuint bfTexWidth, GLuint bfTexHeight) {
	GLuint backFace2DTex;
	glGenTextures(1, &backFace2DTex);
	glBindTexture(GL_TEXTURE_2D, backFace2DTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, bfTexWidth, bfTexHeight, 0, GL_RGBA, GL_FLOAT, NULL);
	return backFace2DTex;
}

// init 3D texture to store the volume data used fo ray casting
GLuint initVol3DTex(const char* filename, GLuint w, GLuint h, GLuint d, int idx) {
	FILE *fp;
	long long int size = w * h;
	size *= d;
	float *data = new float[size];
	if (!(fp = fopen(filename, "rb"))) {
		cout << "Error: opening .raw file failed" << endl;
		exit(EXIT_FAILURE);
	}
	if (fread(data, sizeof(float), size, fp) != size) {
		cout << "Error: read .raw file failed" << endl;
		exit(1);
	}
	fclose(fp);

	float data_min = 1e14f;
	float data_max = 0.0f;
	for (int i = 0; i < size; i++) {
		if (data[i] < data_min)
			data_min = data[i];
		if (data[i] > data_max)
			data_max = data[i];
		data[i] /= 3162277660168.3794f;     // 10^12.5 
	}
	cout << "data_min: " << data_min << " data_max: " << data_max << endl;
	
	glGenTextures(1, &(g_volTexObj[idx]));
    // bind 3D texture target
    glBindTexture(GL_TEXTURE_3D, g_volTexObj[idx]);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    // pixel transfer happens here from client to OpenGL server
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // 	cout << "start creating volume texture" << endl;
    glTexImage3D(GL_TEXTURE_3D, 0, GL_COMPRESSED_RED, w, h, d, 0, GL_RED, GL_FLOAT, data);
	
	delete[]data;
// 	cout << "volume texture created" << endl;
	return g_volTexObj[idx];
}

void checkFramebufferStatus() {
	GLenum complete = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (complete != GL_FRAMEBUFFER_COMPLETE)
	{
		cout << "framebuffer is not complete" << endl;
		exit(EXIT_FAILURE);
	}
}
// init the framebuffer, the only framebuffer used in this program
void initFrameBuffer(GLuint texObj, GLuint texWidth, GLuint texHeight) {
	// create a depth buffer for our framebuffer
	GLuint depthBuffer;
	glGenRenderbuffers(1, &depthBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, texWidth, texHeight);

	// attach the texture and the depth buffer to the framebuffer
	glGenFramebuffers(1, &g_frameBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, g_frameBuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texObj, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
	checkFramebufferStatus();
	glEnable(GL_DEPTH_TEST);
}

void rcSetUinforms() {
	// setting uniforms such as
	// ScreenSize 
	// StepSize
	// TransferFunc
	// ExitPoints i.e. the backface, the backface hold the ExitPoints of ray casting
	// VolumeTex the texture that hold the volume data i.e. head256.raw
	GLint screenSizeLoc = glGetUniformLocation(g_programHandle, "ScreenSize");
	if (screenSizeLoc >= 0) {
		glUniform2f(screenSizeLoc, (float)g_winWidth, (float)g_winHeight);
	}
	else {
		cout << "ScreenSize "
			<< "is not bind to the uniform"
			<< endl;
	}
	GLint stepSizeLoc = glGetUniformLocation(g_programHandle, "StepSize");
	if (stepSizeLoc >= 0) {
		glUniform1f(stepSizeLoc, g_stepSize);
	}
	else {
		cout << "StepSize"
			<< "is not bind to the uniform"
			<< endl;
	}
	GLint transferFuncLoc = glGetUniformLocation(g_programHandle, "TransferFunc");
	if (transferFuncLoc >= 0) {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_1D, g_tffTexObj);
		glUniform1i(transferFuncLoc, 0);
	}
	else {
		cout << "TransferFunc"
			<< "is not bind to the uniform"
			<< endl;
	}
	GLint backFaceLoc = glGetUniformLocation(g_programHandle, "ExitPoints");
	if (backFaceLoc >= 0) {
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, g_bfTexObj);
		glUniform1i(backFaceLoc, 1);
	}
	else {
		cout << "ExitPoints"
			<< "is not bind to the uniform"
			<< endl;
	}
	GLint volumeLocX = glGetUniformLocation(g_programHandle, "VolumeTexX");
	if (volumeLocX >= 0) {
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_3D, g_volTexObj[0]);
		glUniform1i(volumeLocX, 2);
	}
	else {
		cout << "VolumeTexX "
			<< "is not bind to the uniform"
			<< endl;
	}
	GLint VolumeLocY = glGetUniformLocation(g_programHandle, "VolumeTexY");
	if (volumeLocX >= 0) {
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_3D, g_volTexObj[1]);
		glUniform1i(VolumeLocY, 3);
	}
	else {
		cout << "VolumeTexY "
			<< "is not bind to the uniform"
			<< endl;
	}
	GLint volumeLocZ = glGetUniformLocation(g_programHandle, "VolumeTexZ");
	if (volumeLocZ >= 0) {
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_3D, g_volTexObj[2]);
		glUniform1i(volumeLocZ, 4);
	}
	else {
		cout << "VolumeTexZ "
			<< "is not bind to the uniform"
			<< endl;
	}
	GLint weightLoc = glGetUniformLocation(g_programHandle, "weight");
	if (weightLoc >= 0) {
		glUniform3f(weightLoc, weight_x, weight_y, weight_z);
	}
	else {
		cout << "weight "
			<< "is not bind to the uniform"
			<< endl;
	}
}

// init the shader object and shader program
void initShader() {
	// vertex shader object for first pass
	g_bfVertHandle = initShaderObj("../res/shaders/backface.vs", GL_VERTEX_SHADER);
	// fragment shader object for first pass
	g_bfFragHandle = initShaderObj("../res/shaders/backface.fs", GL_FRAGMENT_SHADER);
	// vertex shader object for second pass
	g_rcVertHandle = initShaderObj("../res/shaders/raycasting.vs", GL_VERTEX_SHADER);
	// fragment shader object for second pass
	g_rcFragHandle = initShaderObj("../res/shaders/raycasting.fs", GL_FRAGMENT_SHADER);
	// create the shader program , use it in an appropriate time
	g_programHandle = createShaderPgm();
}

// link the shader objects using the shader program
void linkShader(GLuint shaderPgm, GLuint newVertHandle, GLuint newFragHandle) {
	const GLsizei maxCount = 2;
	GLsizei count;
	GLuint shaders[maxCount];
	glGetAttachedShaders(shaderPgm, maxCount, &count, shaders);
	// cout << "get VertHandle: " << shaders[0] << endl;
	// cout << "get FragHandle: " << shaders[1] << endl;
	for (int i = 0; i < count; i++) {
		glDetachShader(shaderPgm, shaders[i]);
	}
	// Bind index 0 to the shader input variable "VerPos"
	glBindAttribLocation(shaderPgm, 0, "VerPos");
	// Bind index 1 to the shader input variable "VerClr"
	glBindAttribLocation(shaderPgm, 1, "VerClr");
	glAttachShader(shaderPgm, newVertHandle);
	glAttachShader(shaderPgm, newFragHandle);
	glLinkProgram(shaderPgm);
	if (GL_FALSE == checkShaderLinkStatus(shaderPgm)) {
		cerr << "Failed to relink shader program!" << endl;
		exit(EXIT_FAILURE);
	}
}

// the color of the vertex in the back face is also the location
// of the vertex
// save the back face to the user defined framebuffer bound
// with a 2D texture named `g_bfTexObj`
// draw the front face of the box
// in the rendering process, i.e. the ray marching process
// loading the volume `g_volTexObj` as well as the `g_bfTexObj`
// after vertex shader processing we got the color as well as the location of
// the vertex (in the object coordinates, before transformation).
// and the vertex assemblied into primitives before entering
// fragment shader processing stage.
// in fragment shader processing stage. we got `g_bfTexObj`
// (correspond to 'VolumeTex' in glsl)and `g_volTexObj`(correspond to 'ExitPoints')
// as well as the location of primitives.
// the most important is that we got the GLSL to exec the logic. Here we go!
// draw the back face of the box
void display() {
	glEnable(GL_DEPTH_TEST);
	
	FILE* fp;
	char vp_path[1024];
	sprintf(vp_path, "/fs/project/PAS0027/nyx_vdl/viewpoints_weight.txt", filename);
// 	sprintf(vp_path, "/fs/project/PAS0027/nyx_vdl/train/%s/viewpoints.txt", filename);
	if (!(fp = fopen(vp_path, "r"))) {
		cout << "Error: opening viewpoint file failed" << endl;
		exit(EXIT_FAILURE);
	}
	int vpNum;
	fscanf(fp, "%d", &vpNum);
	for (int idx = 0; idx < vpNum; idx++){
		fscanf(fp, "%f%f%f%f%f", &phi, &theta, &weight_x, &weight_y, &weight_z);
// 		cout << phi << " " << theta << " ";
		phi = phi * M_PI / 180.0f;
		theta = theta * M_PI / 180.0f;
		
		// render to texture
    	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_frameBuffer);
    	glViewport(0, 0, g_winWidth, g_winHeight);
    	linkShader(g_programHandle, g_bfVertHandle, g_bfFragHandle);
    	glUseProgram(g_programHandle);
	    // cull front face
    	render(GL_FRONT);
    	glUseProgram(0);
    	glBindFramebuffer(GL_FRAMEBUFFER, 0);
    	glViewport(0, 0, g_winWidth, g_winHeight);
    	linkShader(g_programHandle, g_rcVertHandle, g_rcFragHandle);
    	glUseProgram(g_programHandle);
    	rcSetUinforms();
    	// glUseProgram(g_programHandle);
    	// cull back face
    	render(GL_BACK);
    	glUseProgram(0);
    	glFlush();
    
    	stbi_flip_vertically_on_write(1);
    	char imagepath[1024];
    	sprintf(imagepath, "/fs/project/PAS0027/nyx_vdl/512/img/tf%s/fused/%s/%d.png", tfid, filename, idx);
    // 	cout << "output " << idx << ".png" << endl; 
    	float* pBuffer = new float[g_winWidth * g_winHeight * 4];
    	unsigned char* pImage = new unsigned char[g_winWidth * g_winHeight * 3];
    	glReadBuffer(GL_BACK);
    	glReadPixels(0, 0, g_winWidth, g_winHeight, GL_RGBA, GL_FLOAT, pBuffer);
    	for (unsigned int j = 0; j < g_winHeight; j++) {
    		for (unsigned int k = 0; k < g_winWidth; k++) {
    			int index = j * g_winWidth + k;
    			pImage[index * 3 + 0] = GLubyte(min(pBuffer[index * 4 + 0] * 255, 255.0f));
    			pImage[index * 3 + 1] = GLubyte(min(pBuffer[index * 4 + 1] * 255, 255.0f));
    			pImage[index * 3 + 2] = GLubyte(min(pBuffer[index * 4 + 2] * 255, 255.0f));
    		}
    	}
    	stbi_write_png(imagepath, g_winWidth, g_winHeight, 3, pImage, g_winWidth * 3);
    	delete pBuffer;
    	delete pImage;
	}
	fclose(fp);
}
// both of the two pass use the "render() function"
// the first pass render the backface of the boundbox
// the second pass render the frontface of the boundbox
// together with the frontface, use the backface as a 2D texture in the second pass
// to calculate the entry point and the exit point of the ray in and out the box.
void render(GLenum cullFace) {
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// create the projection matrix 
	float fov_r = 30.0f * M_PI / 180.0f;

	bool perspective_projection = true;
	glm::mat4 pMatrix;
	if (perspective_projection) {
		// Resulting perspective matrix, FOV in radians, aspect ratio, near, and far clipping plane.
		pMatrix = glm::perspective(fov_r, (float)g_winWidth / (float)g_winHeight, 0.1f, 400.f);
	}
	else {
		// The goal is to have the object be about the same size in the window
		// during orthographic project as it is during perspective projection.

		float a = (float)g_winWidth / (float)g_winHeight;
		float h = 3.5f * tan(fov_r / 2); // Window aspect ratio.
		float w = h * a; // Knowing the new window height size, get the new window width size based on the aspect ratio.

		// The canvas' origin is the upper left corner. To the right is the positive x-axis. 
		// Going down is the positive y-axis.

		// Any object at the world origin would appear at the upper left hand corner.
		// Shift the origin to the middle of the screen.
		
		// Also, invert the y-axis as WebgL's positive y-axis points up while the canvas' positive
		// y-axis points down the screen.

		//           (0,O)------------------------(w,0)
		//               |                        |
		//               |                        |
		//               |                        |
		//           (0,h)------------------------(w,h)
		//
		//  (-(w/2),(h/2))------------------------((w/2),(h/2))
		//               |                        |
		//               |         (0,0)          |gbo
		//               |                        |
		// (-(w/2),-(h/2))------------------------((w/2),-(h/2))

		// Resulting perspective matrix, left, right, bottom, top, near, and far clipping plane.
		pMatrix = glm::ortho(-(w / 2),
			(w / 2),
			-(h / 2),
			(h / 2),
		    0.1f, 400.f);
	}
	
	// transform
	float dist = 2.0f;
	glm::vec3 direction = glm::vec3(sin(theta) * cos(phi) * dist, sin(theta) * sin(phi) * dist, cos(theta) * dist);
	glm::vec3 up = glm::vec3(sin(theta - M_PI / 2) * cos(phi), sin(theta - M_PI / 2) * sin(phi), cos(theta - M_PI / 2));
	glm::vec3 center = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 eye = center + direction;

	glm::mat4 view = glm::lookAt(eye, center, up);

	glm::mat4 model = glm::mat4(1.0f);

	// to make the "head256.raw" i.e. the volume data stand up.
	model *= glm::scale(glm::vec3(0.6f, 0.6f, 0.6f));
	// from local space to world space [0, 1]^3 -> [-0.5, 0.5]^3
	model *= glm::translate(glm::vec3(-0.5f, -0.5f, -0.5f));

	glm::mat4 mvMatrix = view * model;
	glm::mat4 mvp = pMatrix * mvMatrix;

	GLuint mvpIdx = glGetUniformLocation(g_programHandle, "MVP");
	if (mvpIdx >= 0) {
		glUniformMatrix4fv(mvpIdx, 1, GL_FALSE, &mvp[0][0]);
	}
	else {
		cerr << "can't get the MVP" << endl;
	}
	drawBox(cullFace);
}

const char *get_egl_error_info(EGLint error) {
	switch (error) {
	case EGL_NOT_INITIALIZED:
		return "EGL_NOT_INITIALIZED";
	case EGL_BAD_ACCESS:
		return "EGL_BAD_ACCESS";
	case EGL_BAD_ALLOC:
		return "EGL_BAD_ALLOC";
	case EGL_BAD_ATTRIBUTE:
		return "EGL_BAD_ATTRIBUTE";
	case EGL_BAD_CONTEXT:
		return "EGL_BAD_CONTEXT";
	case EGL_BAD_CONFIG:
		return "EGL_BAD_CONFIG";
	case EGL_BAD_CURRENT_SURFACE:
		return "EGL_BAD_CURRENT_SURFACE";
	case EGL_BAD_DISPLAY:
		return "EGL_BAD_DISPLAY";
	case EGL_BAD_SURFACE:
		return "EGL_BAD_SURFACE";
	case EGL_BAD_MATCH:
		return "EGL_BAD_MATCH";
	case EGL_BAD_PARAMETER:
		return "EGL_BAD_PARAMETER";
	case EGL_BAD_NATIVE_PIXMAP:
		return "EGL_BAD_NATIVE_PIXMAP";
	case EGL_BAD_NATIVE_WINDOW:
		return "EGL_BAD_NATIVE_WINDOW";
	case EGL_CONTEXT_LOST:
		return "EGL_CONTEXT_LOST";
	case EGL_SUCCESS:
		return "NO ERROR";
	default:
		return "UNKNOWN ERROR";
	}
}
