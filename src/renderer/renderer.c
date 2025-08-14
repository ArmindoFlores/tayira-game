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
    int should_close;
};

typedef struct input_callbacks {
    key_callback on_key;
    mouse_button_callback on_mouse_button;
    mouse_move_callback on_mouse_move;
    mouse_move_callback on_scroll;
} input_callbacks;

typedef struct window_adapter {
    renderer_ctx ctx;
    input_callbacks callbacks;
} window_adapter;

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

static void internal_key_cb(GLFWwindow *win, int key, int scancode, int action, int mods) {
    window_adapter *wa = (window_adapter*) glfwGetWindowUserPointer(win);
    if (!wa || !wa->callbacks.on_key) return;
    if (wa->callbacks.on_key(wa->ctx, key, scancode, action, mods) != 0) {
        wa->ctx->should_close = 1;
    }
}

static void internal_mouse_button_cb(GLFWwindow *win, int button, int action, int mods) {
    window_adapter *wa = (window_adapter*) glfwGetWindowUserPointer(win);
    if (!wa || !wa->callbacks.on_mouse_button) return;
    if (wa->callbacks.on_mouse_button(wa->ctx, button, action, mods) != 0) {
        wa->ctx->should_close = 1;
    }
}

static void internal_mouse_move_cb(GLFWwindow *win, double x, double y) {
    window_adapter *wa = (window_adapter*) glfwGetWindowUserPointer(win);
    if (!wa || !wa->callbacks.on_mouse_move) return;
    if (wa->callbacks.on_mouse_move(wa->ctx, x, y) != 0) {
        wa->ctx->should_close = 1;
    }
}

static void internal_scroll_cb(GLFWwindow *win, double dx, double dy) {
    window_adapter *wa = (window_adapter*) glfwGetWindowUserPointer(win);
    if (!wa || !wa->callbacks.on_scroll) return;
    if (wa->callbacks.on_scroll(wa->ctx, dx, dy) != 0) {
        wa->ctx->should_close = 1;
    }
}

static int init_user_window(GLFWwindow *win, renderer_ctx ctx) {
    if (!win || !ctx) return 1;

    window_adapter *wa = (window_adapter*) malloc(sizeof *wa);
    if (!wa) return 1;

    wa->ctx = ctx;
    wa->callbacks = (input_callbacks) {0};

    glfwSetWindowUserPointer(win, wa);

    glfwSetKeyCallback(win, internal_key_cb);
    glfwSetMouseButtonCallback(win, internal_mouse_button_cb);
    glfwSetCursorPosCallback(win, internal_mouse_move_cb);
    glfwSetScrollCallback(win, internal_scroll_cb);

    return 0;
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

    ctx->should_close = 0;
    ctx->window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (ctx->window == NULL) {
        log_error("Failed to create window");
        glfwTerminate();
        free(ctx);
        return NULL;
    }

    glfwMakeContextCurrent(ctx->window);

    init_user_window(ctx->window, ctx);

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

void renderer_run(
    renderer_ctx ctx, 
    update_callback update_cb, 
    key_callback key_cb,
    mouse_button_callback mouse_button_cb,
    mouse_move_callback mouse_move_cb,
    mouse_move_callback scroll_cb
) {
    window_adapter *wa = (window_adapter*) glfwGetWindowUserPointer(ctx->window);
    if (wa) {
        wa->callbacks.on_key = key_cb;
        wa->callbacks.on_mouse_button = mouse_button_cb;
        wa->callbacks.on_mouse_move = mouse_move_cb;
        wa->callbacks.on_scroll = scroll_cb;
    }
    glfwSwapInterval(1);

    double last_frame_time = glfwGetTime();
    while (!glfwWindowShouldClose(ctx->window) && !ctx->should_close) {
        glClear(GL_COLOR_BUFFER_BIT);

        double current_time = glfwGetTime();
        if (update_cb(ctx, current_time - last_frame_time) != 0) {
            log_info("Exiting main loop");
            break;
        }
        last_frame_time = current_time;

        glfwSwapBuffers(ctx->window);
        glfwPollEvents();
    }
}

void renderer_cleanup(renderer_ctx ctx) {
    window_adapter *wa = (window_adapter *)glfwGetWindowUserPointer(ctx->window);
    if (wa) free(wa);
    glfwSetWindowUserPointer(ctx->window, NULL);
    glDeleteProgram(ctx->shader_program);
    glfwDestroyWindow(ctx->window);
    glfwTerminate();
    free(ctx);
}
