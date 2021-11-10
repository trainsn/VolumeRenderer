#pragma comment(lib,"glew32.lib")
#include <fstream>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glut.h>
#include <GL/glm/glm.hpp>
#include <GL/glm/gtc/matrix_transform.hpp>
#include <GL/glm/gtx/transform2.hpp>
#include <GL/glm/gtc/type_ptr.hpp>
#define _USE_MATH_DEFINES

#define STBI_MSC_SECURE_CRT
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define GL_ERROR() checkForOpenGLError(__FILE__, __LINE__)
using namespace std;
using glm::mat4;
using glm::vec3;
GLuint g_vao;
GLuint g_programHandle;
const GLuint g_winWidth = 2048;
const GLuint g_winHeight = 2048;
GLfloat g_angle = 0;
GLuint g_frameBuffer;
// transfer function
GLuint g_tffTexObj;
GLuint g_bfTexObj;
GLuint g_volTexObj;
GLuint g_rcVertHandle;
GLuint g_rcFragHandle;
GLuint g_bfVertHandle;
GLuint g_bfFragHandle;
float g_stepSize = 0.001f;


int checkForOpenGLError(const char* file, int line){
    // return 1 if an OpenGL error occured, 0 otherwise.
    GLenum glErr;
    int retCode = 0;

    glErr = glGetError();
    while(glErr != GL_NO_ERROR){
		cout << "glError in file " << file
			 << "@line " << line << gluErrorString(glErr) << endl;
		retCode = 1;
		exit(EXIT_FAILURE);
    }
    return retCode;
}
void keyboard(unsigned char key, int x, int y);
void display(void);
void initVBO();
void initShader();
void initFrameBuffer(GLuint, GLuint, GLuint);
GLuint initTFF1DTex(const char* filename);
GLuint initFace2DTex(GLuint texWidth, GLuint texHeight);
GLuint initVol3DTex(const char* filename, GLuint width, GLuint height, GLuint depth);
void render(GLenum cullFace);
void init(){
    initVBO();
    initShader();
    g_tffTexObj = initTFF1DTex("../TF1D/nyx-1.TF1D");
    g_bfTexObj = initFace2DTex(g_winWidth, g_winHeight);
    g_volTexObj = initVol3DTex("D:\\OSU\\Grade3\\Nyx\\00150density_upsample.raw", 256, 256, 256);
    GL_ERROR();
    initFrameBuffer(g_bfTexObj, g_winWidth, g_winHeight);
    GL_ERROR();
}
// init the vertex buffer object
void initVBO(){
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

// draw the six faces of the boundbox by drawwing triangles
// draw it contra-clockwise
// front: 1 5 7 3
// back: 0 2 6 4
// left：0 1 3 2
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
void drawBox(GLenum glFaces)
{
    glEnable(GL_CULL_FACE);
    glCullFace(glFaces);
    glBindVertexArray(g_vao);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (GLuint *)NULL);
    glDisable(GL_CULL_FACE);
}
// check the compilation result
GLboolean compileCheck(GLuint shader)
{
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
GLuint initShaderObj(const GLchar* srcfile, GLenum shaderType)
{
    ifstream inFile(srcfile, ifstream::in);
    // use assert?
    if (!inFile)
    {
	cerr << "Error openning file: " << srcfile << endl;
	exit(EXIT_FAILURE);
    }
    
    const int MAX_CNT = 10000;
    GLchar *shaderCode = (GLchar *) calloc(MAX_CNT, sizeof(GLchar));
    inFile.read(shaderCode, MAX_CNT);
    if (inFile.eof())
    {
	size_t bytecnt = inFile.gcount();
	*(shaderCode + bytecnt) = '\0';
    }
    else if(inFile.fail())
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
    const GLchar* codeArray[] = {shaderCode};
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
GLint checkShaderLinkStatus(GLuint pgmHandle)
{
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
GLuint createShaderPgm()
{
    // Create the shader program
    GLuint programHandle = glCreateProgram();
    if (0 == programHandle)
    {
	cerr << "Error create shader program" << endl;
	exit(EXIT_FAILURE);
    }
    return programHandle;
}


// init the 1 dimentional texture for transfer function
GLuint initTFF1DTex(const char* filename)
{
	FILE *fp = fopen(filename, "r");
	if (!(fp = fopen(filename, "rb"))) {
		cout << "Error: opening .tf1d file failed" << endl;
		exit(EXIT_FAILURE);
	}
	else {
		cout << "OK: open .tf1d file successed" << endl;
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
GLuint initFace2DTex(GLuint bfTexWidth, GLuint bfTexHeight){
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
GLuint initVol3DTex(const char* filename, GLuint w, GLuint h, GLuint d){
    FILE *fp;
    size_t size = w * h * d;
    float *data = new float[size];
    if (!(fp = fopen(filename, "rb"))){
        cout << "Error: opening .raw file failed" << endl;
        exit(EXIT_FAILURE);
    }
	else{
        cout << "OK: open .raw file successed" << endl;
    }
    if ( fread(data, sizeof(float), size, fp)!= size) {
        cout << "Error: read .raw file failed" << endl;
        exit(1);
    }
    else{
        cout << "OK: read .raw file successed" << endl;
    }
    fclose(fp);

	float data_min = 13.0f;
	float data_max = 0.0f;
	for (int i = 0; i < size; i++) {
		if (data[i] < data_min)
			data_min = data[i];
		if (data[i] > data_max)
			data_max = data[i];
		data[i] /= 12.0f;
	}

    glGenTextures(1, &g_volTexObj);
    // bind 3D texture target
    glBindTexture(GL_TEXTURE_3D, g_volTexObj);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);	
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
    // pixel transfer happens here from client to OpenGL server
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RED, w, h, d, 0, GL_RED, GL_FLOAT, data);

    delete []data;
    cout << "volume texture created" << endl;
    return g_volTexObj;
}

void checkFramebufferStatus(){
    GLenum complete = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (complete != GL_FRAMEBUFFER_COMPLETE)
    {
	cout << "framebuffer is not complete" << endl;
	exit(EXIT_FAILURE);
    }
}
// init the framebuffer, the only framebuffer used in this program
void initFrameBuffer(GLuint texObj, GLuint texWidth, GLuint texHeight){
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

void rcSetUinforms(){
    // setting uniforms such as
    // ScreenSize 
    // StepSize
    // TransferFunc
    // ExitPoints i.e. the backface, the backface hold the ExitPoints of ray casting
    // VolumeTex the texture that hold the volume data i.e. head256.raw
    GLint screenSizeLoc = glGetUniformLocation(g_programHandle, "ScreenSize");
    if (screenSizeLoc >= 0){
		glUniform2f(screenSizeLoc, (float)g_winWidth, (float)g_winHeight);
    }
    else{
		cout << "ScreenSize "
			 << "is not bind to the uniform"
			 << endl;
    }
    GLint stepSizeLoc = glGetUniformLocation(g_programHandle, "StepSize");
    GL_ERROR();
    if (stepSizeLoc >= 0){
		glUniform1f(stepSizeLoc, g_stepSize);
    }
    else{
		cout << "StepSize"
			 << "is not bind to the uniform"
			 << endl;
    }
    GL_ERROR();
    GLint transferFuncLoc = glGetUniformLocation(g_programHandle, "TransferFunc");
    if (transferFuncLoc >= 0){
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_1D, g_tffTexObj);
		glUniform1i(transferFuncLoc, 0);
    }
    else{
		cout << "TransferFunc"
			 << "is not bind to the uniform"
			 << endl;
    }
    GL_ERROR();    
    GLint backFaceLoc = glGetUniformLocation(g_programHandle, "ExitPoints");
    if (backFaceLoc >= 0){
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, g_bfTexObj);
		glUniform1i(backFaceLoc, 1);
    }
    else{
		cout << "ExitPoints"
			 << "is not bind to the uniform"
			 << endl;
    }
    GL_ERROR();    
    GLint volumeLoc = glGetUniformLocation(g_programHandle, "VolumeTex");
    if (volumeLoc >= 0){
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_3D, g_volTexObj);
		glUniform1i(volumeLoc, 2);
    }
    else{
		cout << "VolumeTex"
			 << "is not bind to the uniform"
			 << endl;
    }
    
}
// init the shader object and shader program
void initShader(){
// vertex shader object for first pass
    g_bfVertHandle = initShaderObj("shader/backface.vs", GL_VERTEX_SHADER);
// fragment shader object for first pass
    g_bfFragHandle = initShaderObj("shader/backface.fs", GL_FRAGMENT_SHADER);
// vertex shader object for second pass
    g_rcVertHandle = initShaderObj("shader/raycasting.vs", GL_VERTEX_SHADER);
// fragment shader object for second pass
    g_rcFragHandle = initShaderObj("shader/raycasting.fs", GL_FRAGMENT_SHADER);
// create the shader program , use it in an appropriate time
    g_programHandle = createShaderPgm();
// 获得由着色器编译器分配的索引(可选)
}

// link the shader objects using the shader program
void linkShader(GLuint shaderPgm, GLuint newVertHandle, GLuint newFragHandle){
    const GLsizei maxCount = 2;
    GLsizei count;
    GLuint shaders[maxCount];
    glGetAttachedShaders(shaderPgm, maxCount, &count, shaders);
    // cout << "get VertHandle: " << shaders[0] << endl;
    // cout << "get FragHandle: " << shaders[1] << endl;
    GL_ERROR();
    for (int i = 0; i < count; i++) {
		glDetachShader(shaderPgm, shaders[i]);
    }
    // Bind index 0 to the shader input variable "VerPos"
    glBindAttribLocation(shaderPgm, 0, "VerPos");
    // Bind index 1 to the shader input variable "VerClr"
    glBindAttribLocation(shaderPgm, 1, "VerClr");
    GL_ERROR();
    glAttachShader(shaderPgm,newVertHandle);
    glAttachShader(shaderPgm,newFragHandle);
    GL_ERROR();
    glLinkProgram(shaderPgm);
    if (GL_FALSE == checkShaderLinkStatus(shaderPgm)){
		cerr << "Failed to relink shader program!" << endl;
		exit(EXIT_FAILURE);
    }
    GL_ERROR();
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
void display(){
    glEnable(GL_DEPTH_TEST);
    // test the gl_error
    GL_ERROR();
    // render to texture
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_frameBuffer);
    glViewport(0, 0, g_winWidth, g_winHeight);
    linkShader(g_programHandle, g_bfVertHandle, g_bfFragHandle);
    glUseProgram(g_programHandle);
    // cull front face
    render(GL_FRONT);
    glUseProgram(0);
    GL_ERROR();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g_winWidth, g_winHeight);
    linkShader(g_programHandle, g_rcVertHandle, g_rcFragHandle);
    GL_ERROR();
    glUseProgram(g_programHandle);
    rcSetUinforms();
    GL_ERROR();
    // glUseProgram(g_programHandle);
    // cull back face
    render(GL_BACK);
    glUseProgram(0);
    GL_ERROR(); 

	//stbi_flip_vertically_on_write(1);
	//char imagepath[1024];
	//sprintf(imagepath, "../res/0.png");
	//float* pBuffer = new float[g_winWidth * g_winHeight * 4];
	//unsigned char* pImage = new unsigned char[g_winWidth * g_winHeight * 3];
	//glReadBuffer(GL_BACK);
	//glReadPixels(0, 0, g_winWidth, g_winHeight, GL_RGBA, GL_FLOAT, pBuffer);
	//for (unsigned int j = 0; j < g_winHeight; j++) {
	//	for (unsigned int k = 0; k < g_winWidth; k++) {
	//		int index = j * g_winWidth + k;
	//		pImage[index * 3 + 0] = GLubyte(min(pBuffer[index * 4 + 0] * 255, 255.0f));
	//		pImage[index * 3 + 1] = GLubyte(min(pBuffer[index * 4 + 1] * 255, 255.0f));
	//		pImage[index * 3 + 2] = GLubyte(min(pBuffer[index * 4 + 2] * 255, 255.0f));
	//	}
	//}
	//stbi_write_png(imagepath, g_winWidth, g_winHeight, 3, pImage, g_winWidth * 3);
	//delete pBuffer;
	//delete pImage;
    
    glutSwapBuffers();
}
// both of the two pass use the "render() function"
// the first pass render the backface of the boundbox
// the second pass render the frontface of the boundbox
// together with the frontface, use the backface as a 2D texture in the second pass
// to calculate the entry point and the exit point of the ray in and out the box.
void render(GLenum cullFace){
    GL_ERROR();
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// create the projection matrix 
	float near = 0.1;
	float far = 5.0;
	float fov_r = 60.0f;

	// Resulting perspective matrix, FOV in radians, aspect ratio, near, and far clipping plane.
	glm::mat4 pMatrix = glm::perspective(fov_r, (float)g_winWidth / (float)g_winHeight, near, far);

	// transform
	float theta = M_PI / 2;
	float phi = M_PI / 4 * 3;
	float dist = 2.0f;
	glm::vec3 direction = glm::vec3(sin(theta) * cos(phi) * dist, sin(theta) * sin(phi) * dist, cos(theta) * dist);
	glm::vec3 up = glm::vec3(sin(theta - M_PI / 2) * cos(phi), sin(theta - M_PI / 2) * sin(phi), cos(theta - M_PI / 2));
	glm::vec3 center = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 eye = center + direction;

	glm::mat4 view = glm::lookAt(eye, center, up);

	glm::mat4 model = glm::mat4(1.0f);
	model *= glm::rotate(g_angle, glm::vec3(0.0f, 1.0f, 0.0f));

	// to make the "head256.raw" i.e. the volume data stand up.
	model *= glm::scale(glm::vec3(1.2f, 1.2f, 1.2f));
	// from local space to world space [0, 1]^3 -> [-0.5, 0.5]^3
	model *= glm::translate(glm::vec3(-0.5f, -0.5f, -0.5f));

	glm::mat4 mvMatrix = view * model;
	glm::mat4 mvp = pMatrix * mvMatrix;

    GLuint mvpIdx = glGetUniformLocation(g_programHandle, "MVP");
    if (mvpIdx >= 0){
    	glUniformMatrix4fv(mvpIdx, 1, GL_FALSE, &mvp[0][0]);
    }
    else{
    	cerr << "can't get the MVP" << endl;
    }
    GL_ERROR();
    drawBox(cullFace);
    GL_ERROR();
}
void rotateDisplay(){
    g_angle = (g_angle + 0.2f);
	if (g_angle >= 360.0f)
		g_angle = 0.0f;
    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y){
    switch (key){
		case '\x1B':
		exit(EXIT_SUCCESS);
		break;
    }
}

int main(int argc, char** argv){
	glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(g_winWidth, g_winHeight);
    glutCreateWindow("GLUT Test");
    GLenum err = glewInit();
    if (GLEW_OK != err){
		/* Problem: glewInit failed, something is seriously wrong. */
		fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
    }
  
    glutKeyboardFunc(&keyboard);
    glutDisplayFunc(&display);
    glutIdleFunc(&rotateDisplay);
    init();
    glutMainLoop();
    return EXIT_SUCCESS;
}

