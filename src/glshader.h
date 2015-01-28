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


// glshader.h/cpp provide helper classes that make handling GLSL shaders
// much easier.

#ifndef _GLSHADER_H
#define _GLSHADER_H

#include <GL/glew.h>
#include <vector>
using namespace std;

class GLShaderObject;
class GLShader {
public:
	// NOTE: The GLShader will not create the GLSL program object until
	// necessary - that is, when attach or bindVertexAttribute is called.
	// Until then the shader is an extremely lightweight class allowing
	// unused GLShaders to sit idle in other classes without consuming
	// unnecessary resources
	GLShader();
	~GLShader();

	// Attach a shader object - program will not be
	// re-linked automatically
	void attach (GLShaderObject *o);
	// Detach a shader object - program will not be
	// re-linked automatically
	void detach (GLShaderObject *o);

	// Bind a vertex attribute to a specific location
	// The new location will take effect the next time the
	// program is linked
	void bindVertexAttribute (const char *attr, GLuint loc);

	// Link the program
	// If the program is already in use and linking succeeds,
	// the newly linked program will be automatically updated
	// in the GPU
	bool link (char *logstr = NULL, size_t maxlen = 0);
	// Validate the program - if validation fails,
	// running the shader is not recommended
	bool validate (char *logstr = NULL, size_t maxlen = 0);
	// Check if the program has been linked
	// if it has been linked and validated, it
	// can be safely bound
	bool linked();

	// Use the GLSL program - it must already be linked to have any effect
	void bind();
	// Get the index of a uniform
	GLint uniform (const char *name);
	// Restore the fixed function pipeline
	static void unbind();

private:
	void ensureObjectExists();

	GLuint id;
	bool lunk;
};

class GLShaderObject {
	friend class GLShader;

public:
	GLShaderObject();
	~GLShaderObject();

	// Delete the shader
	void release();
	// Returns true if the shader represents a valid, usable shader
	bool isValid() { return id > 0; }

	// Load and compile a vertex shader
	bool makeVertexShader (const char *data, char *logstr = NULL, size_t maxlen = 0);
	bool makeVertexShaderFile (const char *filename, char *logstr = NULL, size_t maxlen = 0);

	// Load and compile a fragment shader
	bool makeFragmentShader (const char *data, char *logstr = NULL, size_t maxlen = 0);
	bool makeFragmentShaderFile (const char *filename, char *logstr = NULL, size_t maxlen = 0);

private:
	bool makeShader (const char *data, GLenum type, size_t len, char *logstr = NULL, size_t maxlen = 0);
	bool makeShaderFile (const char *data, GLenum type, char *logstr = NULL, size_t maxlen = 0);

	GLuint id;
};

void loadShader (GLShader &shdr, bool &success, const char *root);

#endif
