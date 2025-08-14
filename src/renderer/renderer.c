#include "renderer.h"
#include "utils/utils.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <string.h>

static const char BASE_SHADER_PATH[] = "src/shaders/";

struct renderer_ctx_s {
    GLFWwindow *window;
    GLuint shader_program;
};

static char *get_full_shader_path(const char *filename) {
    size_t size = strlen(filename) + sizeof(BASE_SHADER_PATH);
    char *complete_filename = (char*) malloc(size);
    if (complete_filename == NULL) {
        return NULL;
    }
    strcpy(complete_filename, BASE_SHADER_PATH);
    strcat(complete_filename, filename);
    return complete_filename;
}

static char *load_shader(const char *filename) {
    char *full_name = get_full_shader_path(filename);
    if (full_name == NULL) {
        return NULL;
    }

    char *shader_source = utils_read_whole_file(full_name);
    if (shader_source == NULL) {
        log_error("Failed to read file at '%s'", full_name);
        free(full_name);
        return NULL;
    }
    free(full_name);

    return shader_source;
}

static GLuint compile_shader(GLenum type, const char *filename) {
    GLuint shader = glCreateShader(type);
    char *shader_source = load_shader(filename);
    if (shader_source == NULL) {
        log_error("Failed to obtain shader source for shader '%s'", filename);
        return 0;
    }

    log_info("Compiling shader '%s'", filename);
    glShaderSource(shader, 1, (const char* const*) &shader_source, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        log_error("Shader compilation error: %s\n", log);
        shader = 0;
    }
    free(shader_source);
    return shader;
}

renderer_ctx renderer_init(int width, int height, const char *title) {
    renderer_ctx ctx = (renderer_ctx) malloc(sizeof(struct renderer_ctx_s));
    if (ctx == NULL) {
        log_error("Failed to allocate memory for renderer context");
        return NULL;
    }

    if (!glfwInit()) {
        log_error("Failed to init GLFW");
        return NULL;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    ctx->window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (ctx->window == NULL) {
        log_error("Failed to create window");
        glfwTerminate();
        free(ctx);
        return NULL;
    }

    glfwMakeContextCurrent(ctx->window);

    if (glewInit() != GLEW_OK) {
        log_error("Failed to init GLEW\n");
        free(ctx);
        return NULL;
    }

    GLuint vs = compile_shader(GL_VERTEX_SHADER, "texture/vertex.glsl");
    if (vs == 0) {
        free(ctx);
        return NULL;
    }

    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, "texture/fragment.glsl");
    if (fs == 0) {
        free(ctx);
        return NULL;
    }

    ctx->shader_program = glCreateProgram();
    glAttachShader(ctx->shader_program, vs);
    glAttachShader(ctx->shader_program, fs);
    glLinkProgram(ctx->shader_program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    return ctx;
}

void renderer_fill(renderer_ctx ctx, float r, float g, float b) {
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void renderer_run(renderer_ctx ctx, int (*update) (renderer_ctx)) {
    while (!glfwWindowShouldClose(ctx->window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        if (update(ctx) != 0) {
            log_info("Exiting main loop");
            break;
        }

        glfwSwapBuffers(ctx->window);
        glfwPollEvents();
    }
}

void renderer_cleanup(renderer_ctx ctx) {
    glDeleteProgram(ctx->shader_program);
    glfwDestroyWindow(ctx->window);
    glfwTerminate();
    free(ctx);
}
