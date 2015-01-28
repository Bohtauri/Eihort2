/* Copyright (c) 2012, Jason Lloyd-Price
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met: 

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer. 
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include <cstring>
#include <cstdlib>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include "glshader.h"
using namespace std;

#ifdef _WINDOWS
# define snprintf _snprintf
#endif

void loadShader (GLShader &shdr, bool &success, const char *root) {
	char fname[64];
	snprintf (fname, 64, "%s.txt", root);
	ofstream outf (fname);

	if (!GLEW_VERSION_2_0) {
		outf << "GLSL Shaders are not supported.";
		success = false;
		return;
	}

	GLShaderObject vs, fs;
	char logs[512];

	snprintf (fname, 64, "%s.vs", root);
	bool vsc = vs.makeVertexShaderFile (fname, logs, 512);
	outf << fname << " compile: " << (vsc ? "success" : "FAILED!")<< endl;
	outf << logs << endl << endl;
	outf.flush();

	snprintf (fname, 64, "%s.fs", root);
	bool fsc = fs.makeFragmentShaderFile (fname, logs, 512);
	outf << fname << " compile: " << (fsc ? "success" : "FAILED!") << endl;
	outf << logs << endl << endl;
	outf.flush();

	if (vsc && fsc) {
		shdr.attach (&vs);
		shdr.attach (&fs);
		success = shdr.link (logs, 512);
		outf << "link: " << (success ? "success" : "FAILED!") << endl;
		outf << logs << endl << endl;
		outf.flush();
		if (success) {
			outf << "validation:"  << (shdr.validate (logs, 512) ? "success" : "FAILED!") << endl;
			outf << logs << endl << endl;
			outf.flush();
		}
	}
}

// ---------------------------------------------------------------------------
GLShader::GLShader() {
	id = 0;
	lunk = false;
}

// ---------------------------------------------------------------------------
GLShader::~GLShader() {
	glDeleteShader (id);
}

// ---------------------------------------------------------------------------
void GLShader::attach (GLShaderObject *o) {
	ensureObjectExists();
	if (o->id) {
		glAttachShader (id, o->id);
		lunk = false;
	}
}

// ---------------------------------------------------------------------------
void GLShader::detach (GLShaderObject *o) {
	if (!id) return;
	if (o->id) {
		glDetachShader (id, o->id);
		lunk = false;
	}
}

// ---------------------------------------------------------------------------
void GLShader::bindVertexAttribute (const char *attr, GLuint loc) {
	ensureObjectExists();
	glBindAttribLocation (id, loc, attr);
}

// ---------------------------------------------------------------------------
bool GLShader::validate (char *logstr, size_t maxlen) {
	if (!lunk) {
		if (logstr) snprintf (logstr, maxlen, "Shader is not linked.");
		return false;
	}

	// Validate the program
	glValidateProgram (id);

	// Grab the logstr
	if (logstr) {
		glGetProgramInfoLog (id, (GLsizei)maxlen, NULL, logstr);
	}

	// Check for success
	int success;
	glGetProgramiv (id, GL_VALIDATE_STATUS, &success);
	return success != GL_FALSE;
}

// ---------------------------------------------------------------------------
bool GLShader::link (char *logstr, size_t maxlen) {
	lunk = false;

	if (!id) {
		if (logstr) snprintf (logstr, maxlen, "GLShader::link called before the shader was set up.");
		return false;
	}

	// Link the program
	glLinkProgram (id);

	// Grab the logstr
	if (logstr) {
		glGetProgramInfoLog (id, (GLsizei)maxlen, NULL, logstr);
	}

	// Check for success
	int success;
	glGetProgramiv (id, GL_LINK_STATUS, &success);
	return lunk = (success != GL_FALSE);
}

// ---------------------------------------------------------------------------
bool GLShader::linked() {
	return lunk;
}

// ---------------------------------------------------------------------------
GLint GLShader::uniform (const char *name) {
	if (lunk) {
		return glGetUniformLocation (id, name);
	}
	return -1;
}

// ---------------------------------------------------------------------------
void GLShader::bind() {
	if (lunk) {
		glUseProgram (id);
	} else {
		unbind();
	}
}

// ---------------------------------------------------------------------------
void GLShader::unbind() {
	glUseProgram (0);
}

// ---------------------------------------------------------------------------
void GLShader::ensureObjectExists() {
	if (!id) {
		id = glCreateProgram();
	}
}

// ---------------------------------------------------------------------------
GLShaderObject::GLShaderObject() {
	id = 0;
}

// ---------------------------------------------------------------------------
GLShaderObject::~GLShaderObject() {
	release();
}

// ---------------------------------------------------------------------------
void GLShaderObject::release() {
	if (id) {
		glDeleteShader (id);
		id = 0;
	}
}

// ---------------------------------------------------------------------------
bool GLShaderObject::makeVertexShader (const char *data, char *logstr, size_t maxlen) {
	return makeShader (data, GL_VERTEX_SHADER, strlen(data), logstr, maxlen);
}

// ---------------------------------------------------------------------------
bool GLShaderObject::makeVertexShaderFile (const char *filename, char *logstr, size_t maxlen) {
	return makeShaderFile (filename, GL_VERTEX_SHADER, logstr, maxlen);
}

// ---------------------------------------------------------------------------
bool GLShaderObject::makeFragmentShader (const char *data, char *logstr, size_t maxlen) {
	return makeShader (data, GL_FRAGMENT_SHADER, strlen(data), logstr, maxlen);
}

// ---------------------------------------------------------------------------
bool GLShaderObject::makeFragmentShaderFile (const char *filename, char *logstr, size_t maxlen) {
	return makeShaderFile (filename, GL_FRAGMENT_SHADER, logstr, maxlen);
}

// ---------------------------------------------------------------------------
bool GLShaderObject::makeShader (const char *data, GLenum type, size_t len, char *logstr, size_t maxlen) {
	// Create and load the shader
	GLuint id = glCreateShader (type);
	glShaderSource (id, 1, &data, (GLint*)&len);

	// Compile the shader
	glCompileShader (id);

	// Get the log
	if (logstr) {
		glGetShaderInfoLog (id, (GLsizei)maxlen, NULL, logstr);
	}

	// Check for success
	int success;
	glGetShaderiv (id, GL_COMPILE_STATUS, &success);
	if (success) {
		// woot
		release();
		this->id = id;
		return true;
	}

	glDeleteShader (id);

	return false;
}

// ---------------------------------------------------------------------------
bool GLShaderObject::makeShaderFile (const char *filename, GLenum type, char *logstr, size_t maxlen) {
	ifstream file (filename);
	if (!file) {
		if (logstr) {
			snprintf (logstr, maxlen, "Failed to open \'%s\'", filename);
		}
		return false;
	}

	// Get the file length
    file.seekg(0,ios::end);
    size_t len = (size_t)file.tellg();
    file.seekg(ios::beg);

	// Read in the string
	int ch;
	char *data = (char*)malloc (len), *s = data;
	while ((ch = file.get()) >= 0) *(s++) = (char)ch;

	// Read it
	bool success = makeShader (data, type, s - data, logstr, maxlen);

	// Done
	free (data);

	return success;
}
