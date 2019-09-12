/* Copyright (C) 2015 Hans-Kristian Arntzen <maister@archlinux.us>
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "glfft_gl_interface.hpp"
#ifdef GLFFT_GL_DEBUG
#include "glfft_validate.hpp"
#endif
#include <cstdarg>
#include <cstring>
#include <vector>

using namespace GLFFT;
using namespace std;

GLCommandBuffer GLContext::static_command_buffer;

void GLCommandBuffer::bind_program(Program *program)
{
    glUseProgram(program ? static_cast<GLProgram*>(program)->name : 0);
}

void GLCommandBuffer::bind_storage_texture(unsigned binding, Texture *texture, Format format)
{
    glBindImageTexture(binding, static_cast<GLTexture*>(texture)->name,
            0, GL_FALSE, 0, GL_WRITE_ONLY, convert(format));
}

void GLCommandBuffer::bind_texture(unsigned binding, Texture *texture)
{
    glActiveTexture(GL_TEXTURE0 + binding);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLTexture*>(texture)->name);
}

void GLCommandBuffer::bind_sampler(unsigned binding, Sampler *sampler)
{
    glBindSampler(binding, sampler ? static_cast<GLSampler*>(sampler)->name : 0);
}

void GLCommandBuffer::bind_storage_buffer(unsigned binding, Buffer *buffer)
{
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, static_cast<GLBuffer*>(buffer)->name);
}

void GLCommandBuffer::bind_storage_buffer_range(unsigned binding, size_t offset, size_t size, Buffer *buffer)
{
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, binding, static_cast<GLBuffer*>(buffer)->name, offset, size);
}

void GLCommandBuffer::dispatch(unsigned x, unsigned y, unsigned z)
{
    glDispatchCompute(x, y, z);
}

void GLCommandBuffer::barrier(Buffer*)
{
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void GLCommandBuffer::barrier(Texture*)
{
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
}

void GLCommandBuffer::barrier()
{
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

void GLCommandBuffer::push_constant_data(unsigned binding, const void *data, size_t size)
{
    glBindBufferBase(GL_UNIFORM_BUFFER, binding, ubos[ubo_index]);
    void *ptr = glMapBufferRange(GL_UNIFORM_BUFFER,
            0, CommandBuffer::MaxConstantDataSize,
            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    if (ptr)
    {
        std::memcpy(ptr, data, size);
        glUnmapBuffer(GL_UNIFORM_BUFFER);
    }

    if (++ubo_index >= ubo_count)
        ubo_index = 0;
}

CommandBuffer* GLContext::request_command_buffer()
{
    if (!initialized_ubos)
    {
        glGenBuffers(MaxBuffersRing, ubos);
        for (auto &ubo : ubos)
        {
            glBindBuffer(GL_UNIFORM_BUFFER, ubo);
            glBufferData(GL_UNIFORM_BUFFER, CommandBuffer::MaxConstantDataSize, nullptr, GL_STREAM_DRAW);
        }
        static_command_buffer.set_constant_data_buffers(ubos, MaxBuffersRing);
        initialized_ubos = true;
    }
    return &static_command_buffer;
}

void GLContext::submit_command_buffer(CommandBuffer*)
{}

void GLContext::wait_idle()
{
    glFinish();
}

unique_ptr<Texture> GLContext::create_texture(const void *initial_data,
        unsigned width, unsigned height,
        Format format)
{
    return unique_ptr<Texture>(new GLTexture(initial_data, width, height, format));
}

unique_ptr<Buffer> GLContext::create_buffer(const void *initial_data, size_t size, AccessMode access)
{
    return unique_ptr<Buffer>(new GLBuffer(initial_data, size, access));
}

unique_ptr<Program> GLContext::compile_compute_shader(const char *source)
{
#ifdef GLFFT_GL_DEBUG
    if (!validate_glsl_source(source))
        return nullptr;
#endif

    GLuint program = glCreateProgram();
    if (!program)
    {
        return nullptr;
    }

    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);

    const char *sources[] = { GLFFT_GLSL_LANG_STRING, source };
    glShaderSource(shader, 2, sources, NULL);
    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        GLint len;
        GLsizei out_len;

        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        vector<char> buf(len);
        glGetShaderInfoLog(shader, len, &out_len, buf.data());
        log("GLFFT: Shader log:\n%s\n\n", buf.data());

        glDeleteShader(shader);
        glDeleteProgram(program);
        return 0;
    }

    glAttachShader(program, shader);
    glLinkProgram(program);
    glDeleteShader(shader);

    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
    {
        GLint len;
        GLsizei out_len;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        vector<char> buf(len);
        glGetProgramInfoLog(program, len, &out_len, buf.data());
        log("Program log:\n%s\n\n", buf.data());

        glDeleteProgram(program);
        glDeleteShader(shader);
        return nullptr;
    }

    return unique_ptr<Program>(new GLProgram(program));
}

void GLContext::log(const char *fmt, ...)
{
    char buffer[4 * 1024];

    va_list va;
    va_start(va, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, va);
    va_end(va);
    glfft_log("%s", buffer);
}

double GLContext::get_time()
{
    return glfft_time();
}

unsigned GLContext::get_max_work_group_threads()
{
    GLint value;
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &value);
    return value;
}

const char* GLContext::get_renderer_string()
{
    return reinterpret_cast<const char*>(glGetString(GL_RENDERER));
}

const void* GLContext::map(Buffer *buffer, size_t offset, size_t size)
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, static_cast<GLBuffer*>(buffer)->name);
    const void *ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, offset, size, GL_MAP_READ_BIT);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return ptr;
}

void GLContext::unmap(Buffer *buffer)
{
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, static_cast<GLBuffer*>(buffer)->name);
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void GLContext::teardown()
{
    if (initialized_ubos)
        glDeleteBuffers(MaxBuffersRing, ubos);
    initialized_ubos = false;
}

GLContext::~GLContext()
{
    teardown();
}

GLTexture::GLTexture(const void *initial_data,
        unsigned width, unsigned height,
        Format format)
{
    glGenTextures(1, &name);
    glBindTexture(GL_TEXTURE_2D, name);
    glTexStorage2D(GL_TEXTURE_2D, 1, convert(format), width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    if (initial_data)
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                convert_format(format), convert_type(format), initial_data);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

GLTexture::~GLTexture()
{
    if (owned)
        glDeleteTextures(1, &name);
}

GLBuffer::GLBuffer(const void *initial_data, size_t size, AccessMode access)
{
    glGenBuffers(1, &name);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, name);
    glBufferData(GL_SHADER_STORAGE_BUFFER, size, initial_data, convert(access));
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

GLBuffer::~GLBuffer()
{
    if (owned)
        glDeleteBuffers(1, &name);
}

GLProgram::GLProgram(GLuint name)
    : name(name)
{}

GLProgram::~GLProgram()
{
    if (name != 0)
    {
        glDeleteProgram(name);
    }
}

GLSampler::~GLSampler()
{
    if (name != 0)
    {
        glDeleteSamplers(1, &name);
    }
}

