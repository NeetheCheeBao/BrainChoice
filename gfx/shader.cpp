#include "gfx/shader.hpp"
static GLuint compileOne(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[2048];
        glGetShaderInfoLog(s, 2048, nullptr, buf);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

bool Shader::load(const char* vertexSrc, const char* fragmentSrc) {
    GLuint vs = compileOne(GL_VERTEX_SHADER, vertexSrc);
    GLuint fs = compileOne(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }

    id = glCreateProgram();
    glAttachShader(id, vs);
    glAttachShader(id, fs);
    glLinkProgram(id);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(id, GL_LINK_STATUS, &ok);
    if (!ok) {
        char buf[2048];
        glGetProgramInfoLog(id, 2048, nullptr, buf);
        glDeleteProgram(id);
        id = 0;
        return false;
    }
    return true;
}

void Shader::use() const {
    glUseProgram(id);
}

void Shader::destroy() {
    if (id) {
        glDeleteProgram(id);
        id = 0;
    }
}

void Shader::setMat4(const char* name, const Mat4& m) const {
    glUniformMatrix4fv(glGetUniformLocation(id, name), 1, GL_FALSE, m.m);
}

void Shader::setVec3(const char* name, const Vec3& v) const {
    glUniform3f(glGetUniformLocation(id, name), v.x, v.y, v.z);
}

void Shader::setVec4(const char* name, const Vec4& v) const {
    glUniform4f(glGetUniformLocation(id, name), v.x, v.y, v.z, v.w);
}

void Shader::setFloat(const char* name, float v) const {
    glUniform1f(glGetUniformLocation(id, name), v);
}

void Shader::setInt(const char* name, int v) const {
    glUniform1i(glGetUniformLocation(id, name), v);
}
