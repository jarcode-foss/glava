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

#ifndef GLFFT_GL_INTERFACE_HPP__
#define GLFFT_GL_INTERFACE_HPP__

#include "glfft_interface.hpp"

#include "glfft_gl_api_headers.hpp"

/* GLava additions (POSIX) */
extern "C" {
    #include <time.h>
    #include <stdarg.h>
    #include <stdlib.h>
    #include <string.h>
    #include <error.h>
    #include <errno.h>
    #include <stdio.h>
}

#ifndef GLFFT_GLSL_LANG_STRING
#error GLFFT_GLSL_LANG_STRING must be defined to e.g. "#version 310 es\n" or "#version 430 core\n".
#endif

#ifndef GLFFT_LOG_OVERRIDE
void glfft_log(const char *fmt, ...) {
    va_list l;
    va_start(l, fmt);
    vfprintf(stdout, fmt, l);
    va_end(l);
}
#else
#define glfft_log GLFFT_LOG_OVERRIDE
#endif

#ifndef GLFFT_TIME_OVERRIDE
double glfft_time() {
    struct timespec tv;
    if (clock_gettime(CLOCK_REALTIME, &tv)) {
        fprintf(stderr, "clock_gettime(CLOCK_REALTIME, ...): %s\n", strerror(errno));
    }
    return (double) tv.tv_sec + ((double) tv.tv_nsec / 1000000000.0);
}
#else
#define glfft_time GLFFT_TIME_OVERRIDE
#endif

namespace GLFFT
{
    class GLContext;

    class GLTexture : public Texture
    {
        public:
            friend class GLContext;
            friend class GLCommandBuffer;
            ~GLTexture();

            GLTexture(GLuint obj) : name(obj), owned(false) {}
            GLuint get() const { return name; }

        private:
            GLTexture(const void *initial_data,
                    unsigned width, unsigned height,
                    Format format);
            GLuint name;
            bool owned = true;
    };

    // Not really used by test and bench code, but can be useful for API users.
    class GLSampler : public Sampler
    {
        public:
            friend class GLContext;
            friend class GLCommandBuffer;
            ~GLSampler();

            GLSampler(GLuint obj) : name(obj) {}
            GLuint get() const { return name; }

        private:
            GLuint name;
    };

    class GLBuffer : public Buffer
    {
        public:
            friend class GLContext;
            friend class GLCommandBuffer;
            ~GLBuffer();

            GLBuffer(GLuint obj) : name(obj), owned(false) {}
            GLuint get() const { return name; }

        private:
            GLuint name;
            GLBuffer(const void *initial_data, size_t size, AccessMode access);
            bool owned = true;
    };

    class GLProgram : public Program
    {
        public:
            friend class GLContext;
            friend class GLCommandBuffer;
            ~GLProgram();

            GLuint get() const { return name; }

        private:
            GLProgram(GLuint name);
            GLuint name;
    };

    class GLCommandBuffer : public CommandBuffer
    {
        public:
            ~GLCommandBuffer() = default;

            void set_constant_data_buffers(const GLuint *ubos, unsigned count)
            {
                this->ubos = ubos;
                ubo_index = 0;
                ubo_count = count;
            }

            void bind_program(Program *program) override;
            void bind_storage_texture(unsigned binding, Texture *texture, Format format) override;
            void bind_texture(unsigned binding, Texture *texture) override;
            void bind_sampler(unsigned binding, Sampler *sampler) override;
            void bind_storage_buffer(unsigned binding, Buffer *texture) override;
            void bind_storage_buffer_range(unsigned binding, size_t offset, size_t length, Buffer *texture) override;
            void dispatch(unsigned x, unsigned y, unsigned z) override;

            void barrier(Buffer *buffer) override;
            void barrier(Texture *buffer) override;
            void barrier() override;

            void push_constant_data(unsigned binding, const void *data, size_t size) override;

        private:
            const GLuint *ubos = nullptr;
            unsigned ubo_count = 0;
            unsigned ubo_index = 0;
    };

    class GLContext : public Context
    {
        public:
            ~GLContext();

            std::unique_ptr<Texture> create_texture(const void *initial_data,
                    unsigned width, unsigned height,
                    Format format) override;

            std::unique_ptr<Buffer> create_buffer(const void *initial_data, size_t size, AccessMode access) override;
            std::unique_ptr<Program> compile_compute_shader(const char *source) override;

            CommandBuffer* request_command_buffer() override;
            void submit_command_buffer(CommandBuffer *cmd) override;
            void wait_idle() override;

            const char* get_renderer_string() override;
            void log(const char *fmt, ...) override;
            double get_time() override;

            unsigned get_max_work_group_threads() override;

            const void* map(Buffer *buffer, size_t offset, size_t size) override;
            void unmap(Buffer *buffer) override;

            // Not supported in GLES, so override when creating platform-specific context.
            bool supports_texture_readback() override { return false; }
            void read_texture(void*, Texture*, Format) override {}

        protected:
            void teardown();

        private:
            static GLCommandBuffer static_command_buffer;

            enum { MaxBuffersRing = 256 };
            GLuint ubos[MaxBuffersRing];
            bool initialized_ubos = false;
    };

    static inline GLenum convert(AccessMode mode)
    {
        switch (mode)
        {
            case AccessStreamCopy: return GL_STREAM_COPY;
            case AccessStaticCopy: return GL_STATIC_COPY;
            case AccessStreamRead: return GL_STREAM_READ;
        }
        return 0;
    }

    static inline GLenum convert(Format format)
    {
        switch (format)
        {
            case FormatR16G16B16A16Float: return GL_RGBA16F;
            case FormatR32G32B32A32Float: return GL_RGBA32F;
            case FormatR32Float: return GL_R32F;
            case FormatR16G16Float: return GL_RG16F;
            case FormatR32G32Float: return GL_RG32F;
            case FormatR32Uint: return GL_R32UI;
            case FormatUnknown: return 0;
        }
        return 0;
    }

    static inline GLenum convert_format(Format format)
    {
        switch (format)
        {
            case FormatR16G16Float: return GL_RG;
            case FormatR32G32Float: return GL_RG;
            case FormatR16G16B16A16Float: return GL_RGBA;
            case FormatR32G32B32A32Float: return GL_RGBA;
            case FormatR32Float: return GL_RED;
            case FormatR32Uint: return GL_RED_INTEGER;
            case FormatUnknown: return 0;
        }
        return 0;
    }

    static inline GLenum convert_type(Format format)
    {
        switch (format)
        {
            case FormatR16G16Float: return GL_HALF_FLOAT;
            case FormatR16G16B16A16Float: return GL_HALF_FLOAT;
            case FormatR32Float: return GL_FLOAT;
            case FormatR32G32Float: return GL_FLOAT;
            case FormatR32G32B32A32Float: return GL_FLOAT;
            case FormatR32Uint: return GL_UNSIGNED_INT;
            case FormatUnknown: return 0;
        }
        return 0;
    }
}

#endif
