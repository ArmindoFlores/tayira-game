#include "renderer.h"
#include "utils/utils.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

#define DECLARE_UNIFORM(__obj, __name)\
    __obj.__name ## Loc = get_uniform_location(__obj.base_data.shader_program, #__name);\
    if (__obj.__name ## Loc == -1) {\
        return 1;\
    }\

#define DECLARE_SHADERS(__obj, __vertex, __fragment)\
    GLuint vs = compile_shader(GL_VERTEX_SHADER, __vertex);\
    if (vs == 0) {\
        return 1;\
    }\
\
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, __fragment);\
    if (fs == 0) {\
        glDeleteShader(vs);\
        return 1;\
    }\
\
    __obj.base_data.shader_program = glCreateProgram();\
    glAttachShader(__obj.base_data.shader_program, vs);\
    glAttachShader(__obj.base_data.shader_program, fs);\
    glLinkProgram(__obj.base_data.shader_program);\
\
    glDeleteShader(vs);\
    glDeleteShader(fs);

static const char BASE_SHADER_PATH[] = "assets/shaders/";
static const float QUAD_VERTS[] = {
    0.0f, 1.0f, 0.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f
};
static const unsigned QUAD_IDX[] = { 0, 1, 2, 0, 2, 3 };
static const float LINE_QUAD_VERTS[] = {
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.5f,  0.5f,
    -0.5f,  0.5f
};
static const unsigned LINE_IDX[] = { 0, 1, 2, 0, 2, 3 };

typedef struct batch_renderer_data {
    GLuint shader_program;
    GLuint vertex_array_buffer, vertex_buffer_object, element_buffer_object;
    GLuint instance_vertex_buffer_object;
    size_t max_instances;
    size_t instance_count;
} batch_renderer_data;

typedef struct gl_texture_instance {
    float x, y;
    float w, h;
    float u0, v0, u1, v1;
    float z;
} gl_texture_instance;

typedef struct texture_renderer_data {
    batch_renderer_data base_data;
    gl_texture_instance *instances;
    GLuint current_texture;

    // Uniforms
    GLint uScreenLoc;
    GLint uPanLoc; 
    GLint uColorLoc;
    GLint uAlphaClipLoc;
} texture_renderer_data;

typedef struct gl_line_instance {
    float start_x, start_y;
    float dir_x, dir_y;
    float length;
    float width;
    float r, g, b;
    float z;
} gl_line_instance;

typedef struct line_renderer_data {
    batch_renderer_data base_data;
    gl_line_instance *instances;

    // Uniforms
    GLint uScreenLoc;
    GLint uPanLoc; 
} line_renderer_data;

struct renderer_ctx_s {
    GLFWwindow *window;

    texture_renderer_data texture_renderer_data; 
    line_renderer_data line_renderer_data; 

    unsigned int layer;
    float layer_step;

    int logical_w, logical_h;

    // Offscreen framebuffer
    GLuint fbo;
    GLuint fbo_color;
    GLuint fbo_depth;

    // Present pass pipeline
    GLuint present_shader;
    GLuint present_vao;
    GLuint present_vbo;

    int is_fullscreen;
    int screen_w, screen_h;
    int windowed_x, windowed_y;
    int windowed_w, windowed_h;

    float pan_x, pan_y;

    void *user_context;
    int should_close;

    size_t draw_calls, last_draw_calls;
    size_t drawn_instances, last_drawn_instances;

    blending_mode blending_mode;
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
        log_error("Failed to read file at '{s}'", full_name);
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
        log_error("Failed to obtain shader source for shader '{s}'", filename);
        return 0;
    }

    log_info("Compiling shader '{s}'", filename);
    glShaderSource(shader, 1, (const char* const*) &shader_source, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        log_error("Shader compilation error: {s}\n", log);
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

static int renderer_create_offscreen(renderer_ctx ctx, int logical_w, int logical_h) {
    if (logical_w <= 0 || logical_h <= 0) return 1;
    ctx->logical_w = logical_w;
    ctx->logical_h = logical_h;

    glGenFramebuffers(1, &ctx->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, ctx->fbo);

    glGenTextures(1, &ctx->fbo_color);
    glBindTexture(GL_TEXTURE_2D, ctx->fbo_color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, logical_w, logical_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ctx->fbo_color, 0);

    glGenRenderbuffers(1, &ctx->fbo_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, ctx->fbo_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, logical_w, logical_h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, ctx->fbo_depth);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return status == GL_FRAMEBUFFER_COMPLETE ? 0 : 1;
}

static int renderer_create_present_pipeline(renderer_ctx ctx) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, "present/vertex.vs");
    if (vs == 0) {
        return 1;
    }
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, "present/fragment.fs");
    if (fs == 0) { 
        glDeleteShader(vs); 
        return 1; 
    }

    ctx->present_shader = glCreateProgram();
    if (ctx->present_shader == 0) {
        log_error("Failed to create present shader");
        glDeleteShader(vs); 
        glDeleteShader(fs); 
        return 1;
    }
    glAttachShader(ctx->present_shader, vs);
    glAttachShader(ctx->present_shader, fs);
    glLinkProgram(ctx->present_shader);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked = 0;
    glGetProgramiv(ctx->present_shader, GL_LINK_STATUS, &linked);
    if (!linked) {
        log_error("Failed to link present shader");
        glDeleteProgram(ctx->present_shader);
        ctx->present_shader = 0;
        return 1;
    }

    const float fs_triangle[] = {
        -1.0f, -1.0f,  0.0f, 0.0f,
         3.0f, -1.0f,  2.0f, 0.0f,
        -1.0f,  3.0f,  0.0f, 2.0f
    };

    glGenVertexArrays(1, &ctx->present_vao);
    glBindVertexArray(ctx->present_vao);

    glGenBuffers(1, &ctx->present_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->present_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(fs_triangle), fs_triangle, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return 0;
}

static void renderer_destroy_present_pipeline(renderer_ctx ctx) {
    if (ctx->present_vbo != 0) glDeleteBuffers(1, &ctx->present_vbo);
    if (ctx->present_vao != 0) glDeleteVertexArrays(1, &ctx->present_vao);
    if (ctx->present_shader != 0) glDeleteProgram(ctx->present_shader);
}

static void renderer_destroy_offscreen(renderer_ctx ctx) {
    if (ctx->fbo_depth != 0) glDeleteRenderbuffers(1, &ctx->fbo_depth);
    if (ctx->fbo_color != 0) glDeleteTextures(1, &ctx->fbo_color);
    if (ctx->fbo != 0) glDeleteFramebuffers(1, &ctx->fbo);
}

static GLint get_uniform_location(GLuint shader_program, const char* name) {
    GLint result = glGetUniformLocation(shader_program, name);
    if (result == -1) {
        log_error("Invalid uniform '{s}'", name);
    }
    return result;
}

static int create_texture_buffers(renderer_ctx ctx) {
    ctx->texture_renderer_data.instances = (gl_texture_instance*) malloc(sizeof(gl_texture_instance) * ctx->texture_renderer_data.base_data.max_instances);
    if (ctx->texture_renderer_data.instances == NULL) {
        return 1;
    }

    glGenVertexArrays(1, &ctx->texture_renderer_data.base_data.vertex_array_buffer);
    glBindVertexArray(ctx->texture_renderer_data.base_data.vertex_array_buffer);

    glGenBuffers(1, &ctx->texture_renderer_data.base_data.vertex_buffer_object);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->texture_renderer_data.base_data.vertex_buffer_object);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTS), QUAD_VERTS, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &ctx->texture_renderer_data.base_data.element_buffer_object);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->texture_renderer_data.base_data.element_buffer_object);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(QUAD_IDX), QUAD_IDX, GL_STATIC_DRAW);

    glGenBuffers(1, &ctx->texture_renderer_data.base_data.instance_vertex_buffer_object);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->texture_renderer_data.base_data.instance_vertex_buffer_object);
    glBufferData(GL_ARRAY_BUFFER, ctx->texture_renderer_data.base_data.max_instances * sizeof(gl_texture_instance), NULL, GL_STREAM_DRAW);

    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(gl_texture_instance), (void*) offsetof(gl_texture_instance, x));
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);

    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(gl_texture_instance), (void*) offsetof(gl_texture_instance, u0));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);

    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(gl_texture_instance), (void*) offsetof(gl_texture_instance, z));
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);

    DECLARE_SHADERS(ctx->texture_renderer_data, "texture/vertex.vs", "texture/fragment.fs");

    DECLARE_UNIFORM(ctx->texture_renderer_data, uScreen);
    DECLARE_UNIFORM(ctx->texture_renderer_data, uPan);
    DECLARE_UNIFORM(ctx->texture_renderer_data, uColor);
    DECLARE_UNIFORM(ctx->texture_renderer_data, uAlphaClip);

    return 0;
}

static int create_line_buffers(renderer_ctx ctx) {
    ctx->line_renderer_data.instances = (gl_line_instance*) malloc(sizeof(gl_line_instance) * ctx->line_renderer_data.base_data.max_instances);
    if (ctx->line_renderer_data.instances == NULL) {
        return 1;
    }

    glGenVertexArrays(1, &ctx->line_renderer_data.base_data.vertex_array_buffer);
    glBindVertexArray(ctx->line_renderer_data.base_data.vertex_array_buffer);

    glGenBuffers(1, &ctx->line_renderer_data.base_data.vertex_buffer_object);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->line_renderer_data.base_data.vertex_buffer_object);
    glBufferData(GL_ARRAY_BUFFER, sizeof(LINE_QUAD_VERTS), LINE_QUAD_VERTS, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glGenBuffers(1, &ctx->line_renderer_data.base_data.element_buffer_object);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->line_renderer_data.base_data.element_buffer_object);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(LINE_IDX), LINE_IDX, GL_STATIC_DRAW);

    glGenBuffers(1, &ctx->line_renderer_data.base_data.instance_vertex_buffer_object);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->line_renderer_data.base_data.instance_vertex_buffer_object);
    glBufferData(GL_ARRAY_BUFFER, ctx->line_renderer_data.base_data.max_instances * sizeof(gl_line_instance), NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(gl_line_instance), (void*) offsetof(gl_line_instance, start_x));
    glVertexAttribDivisor(1, 1);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(gl_line_instance), (void*) offsetof(gl_line_instance, dir_x));
    glVertexAttribDivisor(2, 1);

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(gl_line_instance), (void*) offsetof(gl_line_instance, length));
    glVertexAttribDivisor(3, 1);

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(gl_line_instance), (void*) offsetof(gl_line_instance, width));
    glVertexAttribDivisor(4, 1);

    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(gl_line_instance), (void*) offsetof(gl_line_instance, r));
    glVertexAttribDivisor(5, 1);

    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, sizeof(gl_line_instance), (void*) offsetof(gl_line_instance, z));
    glVertexAttribDivisor(6, 1);

    glBindVertexArray(0);

    DECLARE_SHADERS(ctx->line_renderer_data, "line/vertex.vs", "line/fragment.fs");

    DECLARE_UNIFORM(ctx->line_renderer_data, uScreen);
    DECLARE_UNIFORM(ctx->line_renderer_data, uPan);

    return 0;
}

renderer_ctx renderer_init(int width, int height, const char *title) {
    renderer_ctx ctx = (renderer_ctx) calloc(1, sizeof(struct renderer_ctx_s));
    if (ctx == NULL) {
        log_error("Failed to allocate memory for renderer context");
        return NULL;
    }
    
    if (!glfwInit()) {
        free(ctx);
        log_error("Failed to init GLFW");
        return NULL;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    ctx->windowed_w = 1;
    ctx->windowed_h = 1;
    ctx->logical_w = 480;
    ctx->logical_h = 320;
    ctx->layer = 1;
    ctx->layer_step = 5e-4;
    ctx->user_context = NULL;
    ctx->pan_x = 0;
    ctx->pan_y = 0;
    ctx->screen_w = 1;
    ctx->screen_h = 1;
    ctx->blending_mode = BLEND_MODE_TRANSPARENCY;

    ctx->texture_renderer_data.base_data.max_instances = 16384;
    ctx->line_renderer_data.base_data.max_instances = 4096;

    ctx->window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (ctx->window == NULL) {
        log_error("Failed to create window");
        renderer_cleanup(ctx);
        return NULL;
    }

    glfwMakeContextCurrent(ctx->window);

    if (init_user_window(ctx->window, ctx) != 0) {
        log_error("Failed to init window callbacks\n");
        renderer_cleanup(ctx);
        return NULL;
    }

    if (glewInit() != GLEW_OK) {
        log_error("Failed to init GLEW\n");
        renderer_cleanup(ctx);
        return NULL;
    }

    if (create_texture_buffers(ctx) != 0) {
        log_error("Failed to initialize texture buffers");
        renderer_cleanup(ctx);
        return NULL;
    }

    if (create_line_buffers(ctx) != 0) {
        log_error("Failed to initialize line buffers");
        renderer_cleanup(ctx);
        return NULL;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_FALSE);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    if (renderer_create_offscreen(ctx, ctx->logical_w, ctx->logical_h) != 0) {
        log_error("Failed to create offscreen framebuffer");
        renderer_cleanup(ctx);
        return NULL;
    }
    if (renderer_create_present_pipeline(ctx) != 0) {
        log_error("Failed to create present pipeline");
        renderer_cleanup(ctx);
        return NULL;
    }

    return ctx;
}

void renderer_register_user_context(renderer_ctx ctx, void *user_context) {
    ctx->user_context = user_context;
}

void *renderer_get_user_context(renderer_ctx ctx) {
    return ctx->user_context;
}

unsigned int renderer_set_layer(renderer_ctx ctx, unsigned int layer) {
    return ctx->layer = layer;
}

unsigned int renderer_get_layer(renderer_ctx ctx) {
    return ctx->layer;
}

unsigned int renderer_increment_layer(renderer_ctx ctx) {
    if ((ctx->layer+1) * ctx->layer_step <= 1) {
        return ++ctx->layer;
    }
    log_throttle_warning(5000, "Tried to increment render layer beyond maximum ({u})", (unsigned int) (1.0 / ctx->layer_step));
    return ctx->layer;
}

unsigned int renderer_decrement_layer(renderer_ctx ctx) {
    if (ctx->layer > 1) {
        return --ctx->layer;
    }
    log_throttle_warning(5000, "Tried to decrement render layer below 1");
    return ctx->layer;
}

void renderer_fill(renderer_ctx, color_rgb color) {
    glClearColor(color.r, color.g, color.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

static void renderer_flush_texture_batch(renderer_ctx ctx) {
    glUseProgram(ctx->texture_renderer_data.base_data.shader_program);
    glBindVertexArray(ctx->texture_renderer_data.base_data.vertex_array_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->texture_renderer_data.base_data.instance_vertex_buffer_object);

    glUniform1f(ctx->texture_renderer_data.uAlphaClipLoc, ctx->blending_mode == BLEND_MODE_BINARY ? 0.5 : 0);
    glUniform2f(ctx->texture_renderer_data.uScreenLoc, (float)ctx->logical_w, (float)ctx->logical_h);
    if (ctx->texture_renderer_data.uPanLoc >= 0) {
        glUniform2f(ctx->texture_renderer_data.uPanLoc, ctx->pan_x, ctx->pan_y);
    }
    glUniform4f(ctx->texture_renderer_data.uColorLoc, 1.0f, 1.0f, 1.0f, 1.0f);

    glBufferData(
        GL_ARRAY_BUFFER, 
        (GLsizeiptr)(ctx->texture_renderer_data.base_data.instance_count * sizeof(gl_texture_instance)),
        ctx->texture_renderer_data.instances, 
        GL_STREAM_DRAW
    );

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ctx->texture_renderer_data.current_texture);

    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, ctx->texture_renderer_data.base_data.instance_count);

    ctx->texture_renderer_data.base_data.instance_count = 0;
    ctx->draw_calls++;
}

static void renderer_flush_line_batch(renderer_ctx ctx) {
    glUseProgram(ctx->line_renderer_data.base_data.shader_program);
    glBindVertexArray(ctx->line_renderer_data.base_data.vertex_array_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->line_renderer_data.base_data.instance_vertex_buffer_object);

    glUniform2f(ctx->line_renderer_data.uScreenLoc, (float)ctx->logical_w, (float)ctx->logical_h);
    if (ctx->line_renderer_data.uPanLoc >= 0) {
        glUniform2f(ctx->line_renderer_data.uPanLoc, ctx->pan_x, ctx->pan_y);
    }

    glBufferData(
        GL_ARRAY_BUFFER, 
        (GLsizeiptr) ctx->line_renderer_data.base_data.instance_count * sizeof(gl_line_instance),
        ctx->line_renderer_data.instances,
        GL_STREAM_DRAW
    );

    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, ctx->line_renderer_data.base_data.instance_count);

    ctx->line_renderer_data.base_data.instance_count = 0;
    ctx->draw_calls++;
}

void renderer_flush_batch(renderer_ctx ctx) {
    if (ctx->texture_renderer_data.base_data.instance_count > 0 && ctx->texture_renderer_data.current_texture != 0) {
        renderer_flush_texture_batch(ctx);
    }
    if (ctx->line_renderer_data.base_data.instance_count > 0) {
        renderer_flush_line_batch(ctx);
    }
}

void renderer_set_blend_mode(renderer_ctx ctx, blending_mode mode) {
    if (ctx->blending_mode == mode) return;
    renderer_flush_texture_batch(ctx);

    ctx->blending_mode = mode;
    if (mode == BLEND_MODE_BINARY) {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
    else {
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
    }
}

void renderer_draw_line(renderer_ctx ctx, float start_x, float start_y, float end_x, float end_y, color_rgb color, float thickness) {
    float dx = end_x - start_x;
    float dy = end_y - start_y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len <= 1e-6f) return;

    float inv_len = 1.0f / len;
    float dir_x = dx * inv_len;
    float dir_y = dy * inv_len;

    if (ctx->line_renderer_data.base_data.instance_count >= ctx->line_renderer_data.base_data.max_instances) {
        renderer_flush_line_batch(ctx);
    }

    gl_line_instance *inst = &ctx->line_renderer_data.instances[ctx->line_renderer_data.base_data.instance_count++];
    inst->start_x = start_x;
    inst->start_y = start_y;
    inst->dir_x = dir_x;
    inst->dir_y = dir_y;
    inst->length = len;
    inst->width = thickness;
    inst->r = color.r; inst->g = color.g; inst->b = color.b;

    inst->z = ctx->layer_step;
}

void renderer_draw_texture(renderer_ctx ctx, texture t, float x, float y) {
    const unsigned int tex_id = texture_get_id(t);

    if (ctx->texture_renderer_data.current_texture != 0 && ctx->texture_renderer_data.current_texture != tex_id) {
        renderer_flush_texture_batch(ctx);
    }
    ctx->texture_renderer_data.current_texture = tex_id;

    if (ctx->texture_renderer_data.base_data.instance_count >= ctx->texture_renderer_data.base_data.max_instances) {
        renderer_flush_texture_batch(ctx);
        ctx->texture_renderer_data.current_texture = tex_id;
    }

    const float w = (float) texture_get_width(t);
    const float h = (float) texture_get_height(t);

    gl_texture_instance *inst = &ctx->texture_renderer_data.instances[ctx->texture_renderer_data.base_data.instance_count++];
    inst->x = x; inst->y = y; inst->w = w; inst->h = h;
    inst->z = 1.0 - ctx->layer * ctx->layer_step;
    
    float vertices[16];
    texture_get_vertices(t, vertices);
    inst->u0 = vertices[2]; inst->v0 = vertices[3];
    inst->u1 = vertices[10]; inst->v1 = vertices[11];
    ctx->drawn_instances++;
}

void renderer_set_tint(renderer_ctx ctx, color_rgb color) {
    renderer_flush_texture_batch(ctx);
    glUniform4f(ctx->texture_renderer_data.uColorLoc, color.r, color.g, color.b, 1.0f);
}

void renderer_clear_tint(renderer_ctx ctx) {
    renderer_flush_texture_batch(ctx);
    glUniform4f(ctx->texture_renderer_data.uColorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
}

static void renderer_begin_batch(renderer_ctx ctx) {
    glBindFramebuffer(GL_FRAMEBUFFER, ctx->fbo);
    glViewport(0, 0, ctx->logical_w, ctx->logical_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderer_set_blend_mode(ctx, BLEND_MODE_TRANSPARENCY);

    ctx->draw_calls = 0;
    ctx->drawn_instances = 0;
    ctx->layer = 1;
}

static void renderer_end_batch(renderer_ctx ctx) {
    renderer_flush_batch(ctx);

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glfwGetFramebufferSize(ctx->window, &ctx->screen_w, &ctx->screen_h);
    const float sx = (float)ctx->screen_w  / (float)ctx->logical_w;
    const float sy = (float)ctx->screen_h / (float)ctx->logical_h;
    int scale = (int)floorf(fminf(sx, sy));
    if (scale < 1) scale = 1;
    
    const int vp_w = ctx->logical_w * scale;
    const int vp_h = ctx->logical_h * scale;
    const int vp_x = (ctx->screen_w  - vp_w) / 2;
    const int vp_y = (ctx->screen_h - vp_h) / 2;

    glViewport(0, 0, ctx->screen_w, ctx->screen_h);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.f, 0.f, 0.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glViewport(vp_x, vp_y, vp_w, vp_h);
    glUseProgram(ctx->present_shader);
    glBindVertexArray(ctx->present_vao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ctx->fbo_color);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);

    ctx->last_draw_calls = ctx->draw_calls;
    ctx->last_drawn_instances = ctx->drawn_instances;
}

void renderer_toggle_fullscreen(renderer_ctx ctx) {
    if (!ctx->is_fullscreen) {
        // Save current window position and size
        glfwGetWindowPos(ctx->window, &ctx->windowed_x, &ctx->windowed_y);
        int width, height, top, left, bottom, right;
        glfwGetWindowSize(ctx->window, &width, &height);
        glfwGetWindowFrameSize(ctx->window, &left, &top, &right, &bottom);
        ctx->windowed_w = width + left + right;
        ctx->windowed_h = height + bottom + top;

        // Go fullscreen on the primary monitor
        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        if (!monitor) return;

        const GLFWvidmode *mode = glfwGetVideoMode(monitor);
        if (!mode) return;

        glfwSetWindowMonitor(
            ctx->window,
            monitor,
            0, 0,
            mode->width, mode->height,
            mode->refreshRate
        );
        ctx->is_fullscreen = 1;
    } else {
        glfwSetWindowMonitor(
            ctx->window,
            NULL,
            ctx->windowed_x, ctx->windowed_y,
            ctx->windowed_w, ctx->windowed_h,
            0
        );
        ctx->is_fullscreen = 0;
    }
}

renderer_statistics renderer_get_stats(renderer_ctx ctx) {
    return (renderer_statistics) {
        .draw_calls = ctx->last_draw_calls,
        .drawn_instances = ctx->last_drawn_instances
    };
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
        double current_time = glfwGetTime();

        renderer_begin_batch(ctx);
        if (update_cb(ctx, current_time - last_frame_time, current_time) != 0) {
            log_info("Exiting main loop");
            break;
        }
        renderer_end_batch(ctx);
        
        glfwSwapBuffers(ctx->window);
        last_frame_time = current_time;
        glfwPollEvents();
    }
}

void destroy_batch_renderer_objects(batch_renderer_data *data) {
    if (data->shader_program != 0) glDeleteProgram(data->shader_program);
    if (data->vertex_array_buffer != 0) glDeleteVertexArrays(1, &data->vertex_array_buffer);
    if (data->vertex_buffer_object != 0) glDeleteBuffers(1, &data->vertex_buffer_object);
    if (data->instance_vertex_buffer_object != 0) glDeleteBuffers(1, &data->instance_vertex_buffer_object);
    if (data->element_buffer_object != 0) glDeleteBuffers(1, &data->element_buffer_object);
}

void renderer_cleanup(renderer_ctx ctx) {
    if (ctx == NULL) return;

    // Free the window user pointer
    window_adapter *wa = (window_adapter *)glfwGetWindowUserPointer(ctx->window);
    if (wa != NULL) free(wa);
    glfwSetWindowUserPointer(ctx->window, NULL);

    renderer_destroy_present_pipeline(ctx);
    renderer_destroy_offscreen(ctx);
    
    if (ctx->window != NULL) glfwDestroyWindow(ctx->window);
    glfwTerminate();

    // Destroy *_renderer_data GL objects
    destroy_batch_renderer_objects(&ctx->texture_renderer_data.base_data);
    destroy_batch_renderer_objects(&ctx->line_renderer_data.base_data);

    // Free *_renderer_data instances
    free(ctx->texture_renderer_data.instances);
    free(ctx->line_renderer_data.instances);

    free(ctx);
}
