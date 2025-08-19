#include "renderer.h"
#include "utils/utils.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>

static const char BASE_SHADER_PATH[] = "assets/shaders/";
static const int MAX_DRAW_INSTANCES = 16384;
static const float QUAD_VERTS[] = {
    0.0f, 1.0f, 0.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f
};
static const unsigned QUAD_IDX[] = { 0, 1, 2, 0, 2, 3 };

struct texture_buffers_s {
    GLuint vao, instance_vbo, quad_vbo, ebo;
};

typedef struct gl_instance_data {
    float x, y;
    float w, h;
    float u0, v0, u1, v1;
    float z;
} gl_instance_data;

struct renderer_ctx_s {
    GLFWwindow *window;
    GLuint shader_program;
    struct texture_buffers_s t_buf; 

    GLuint current_texture;
    gl_instance_data *instances_cpu;
    GLsizei instance_count;

    unsigned int layer;
    float layer_step;

    // cached uniforms
    GLint uScreenLoc;
    GLint uPanLoc; 
    GLint uColorLoc;

    int logical_w, logical_h;

    // Offscreen framebuffer
    GLuint fbo;
    GLuint fbo_color;
    GLuint fbo_depth;

    // Present pass pipeline
    GLuint present_shader;
    GLuint present_vao;
    GLuint present_vbo;

    int screen_w, screen_h;

    float pan_x, pan_y;

    void *user_context;
    int should_close;

    size_t draw_calls, last_draw_calls;
    size_t drawn_instances, last_drawn_instances;
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

static void init_texture_buffers(renderer_ctx ctx) {
    glGenVertexArrays(1, &ctx->t_buf.vao);
    glBindVertexArray(ctx->t_buf.vao);

    glGenBuffers(1, &ctx->t_buf.quad_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->t_buf.quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTS), QUAD_VERTS, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &ctx->t_buf.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->t_buf.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(QUAD_IDX), QUAD_IDX, GL_STATIC_DRAW);

    glGenBuffers(1, &ctx->t_buf.instance_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, ctx->t_buf.instance_vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_DRAW_INSTANCES * sizeof(gl_instance_data), NULL, GL_STREAM_DRAW);

    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(gl_instance_data), (void*) offsetof(gl_instance_data, x));
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);

    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(gl_instance_data), (void*) offsetof(gl_instance_data, u0));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);

    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(gl_instance_data), (void*) offsetof(gl_instance_data, z));
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);

    glBindVertexArray(0);
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
    if (!ctx) return;
    if (ctx->present_vbo) { glDeleteBuffers(1, &ctx->present_vbo); ctx->present_vbo = 0; }
    if (ctx->present_vao) { glDeleteVertexArrays(1, &ctx->present_vao); ctx->present_vao = 0; }
    if (ctx->present_shader) { glDeleteProgram(ctx->present_shader); ctx->present_shader = 0; }
}

static void renderer_destroy_offscreen(renderer_ctx ctx) {
    if (!ctx) return;
    if (ctx->fbo_depth) { glDeleteRenderbuffers(1, &ctx->fbo_depth); ctx->fbo_depth = 0; }
    if (ctx->fbo_color) { glDeleteTextures(1, &ctx->fbo_color); ctx->fbo_color = 0; }
    if (ctx->fbo)       { glDeleteFramebuffers(1, &ctx->fbo); ctx->fbo = 0; }
}

static GLint get_uniform_location(GLuint shader_program, const char* name) {
    GLint result = glGetUniformLocation(shader_program, name);
    if (result == -1) {
        log_error("Invalid uniform '{s}'", name);
    }
    return result;
}

renderer_ctx renderer_init(int width, int height, const char *title) {
    renderer_ctx ctx = (renderer_ctx) malloc(sizeof(struct renderer_ctx_s));
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
    
    ctx->logical_w = 480;
    ctx->logical_h = 320;
    ctx->fbo = 0;
    ctx->fbo_color = 0;
    ctx->fbo_depth = 0;
    ctx->present_shader = 0;
    ctx->present_vao = 0;
    ctx->present_vbo = 0;
    ctx->layer = 1;
    ctx->layer_step = 5e-4;
    ctx->user_context = NULL;
    ctx->should_close = 0;
    ctx->instance_count = 0;
    ctx->drawn_instances = 0;
    ctx->draw_calls = 0;
    ctx->last_drawn_instances = 0;
    ctx->last_draw_calls = 0;
    ctx->pan_x = 0;
    ctx->pan_y = 0;
    ctx->screen_w = 1;
    ctx->screen_h = 1;
    ctx->current_texture = 0;
    ctx->instances_cpu = (gl_instance_data*) malloc(sizeof(gl_instance_data) * MAX_DRAW_INSTANCES);
    if (ctx->instances_cpu == NULL) {
        log_error("Failed to allocate instance buffer");
        glfwTerminate();
        free(ctx);
        return NULL;
    }
    ctx->window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (ctx->window == NULL) {
        log_error("Failed to create window");
        glfwTerminate();
        free(ctx->instances_cpu);
        free(ctx);
        return NULL;
    }

    glfwMakeContextCurrent(ctx->window);

    init_user_window(ctx->window, ctx);

    if (glewInit() != GLEW_OK) {
        log_error("Failed to init GLEW\n");
        free(ctx->instances_cpu);
        free(ctx);
        return NULL;
    }

    init_texture_buffers(ctx);

    GLuint vs = compile_shader(GL_VERTEX_SHADER, "texture/vertex.vs");
    if (vs == 0) {
        free(ctx->instances_cpu);
        free(ctx);
        return NULL;
    }

    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, "texture/fragment.fs");
    if (fs == 0) {
        free(ctx->instances_cpu);
        free(ctx);
        return NULL;
    }

    ctx->shader_program = glCreateProgram();
    glAttachShader(ctx->shader_program, vs);
    glAttachShader(ctx->shader_program, fs);
    glLinkProgram(ctx->shader_program);
    ctx->uScreenLoc = get_uniform_location(ctx->shader_program, "uScreen");
    ctx->uPanLoc = get_uniform_location(ctx->shader_program, "uPan");
    ctx->uColorLoc = get_uniform_location(ctx->shader_program, "uColor");
    if (ctx->uScreenLoc == -1) {
        free(ctx->instances_cpu);
        free(ctx);
        return NULL;
    }
    if (ctx->uPanLoc == -1) {
        free(ctx->instances_cpu);
        free(ctx);
        return NULL;
    }
    if (ctx->uColorLoc == -1) {
        free(ctx->instances_cpu);
        free(ctx);
        return NULL;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

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

void renderer_flush_batch(renderer_ctx ctx) {
    if (ctx->instance_count == 0 || ctx->current_texture == 0) return;

    glBindBuffer(GL_ARRAY_BUFFER, ctx->t_buf.instance_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(ctx->instance_count * sizeof(gl_instance_data)), ctx->instances_cpu, GL_STREAM_DRAW);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ctx->current_texture);

    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, ctx->instance_count);

    ctx->instance_count = 0;
    ctx->draw_calls++;
}

void renderer_draw_texture(renderer_ctx ctx, texture t, float x, float y) {
    const unsigned int tex_id = texture_get_id(t);

    if (ctx->current_texture != 0 && ctx->current_texture != tex_id) {
        renderer_flush_batch(ctx);
    }
    ctx->current_texture = tex_id;

    if (ctx->instance_count >= MAX_DRAW_INSTANCES) {
        renderer_flush_batch(ctx);
        ctx->current_texture = tex_id;
    }

    const float w = (float) texture_get_width(t);
    const float h = (float) texture_get_height(t);

    gl_instance_data *inst = &ctx->instances_cpu[ctx->instance_count++];
    inst->x = x; inst->y = y; inst->w = w; inst->h = h;
    inst->z = 1.0 - ctx->layer * ctx->layer_step;
    
    float vertices[16];
    texture_get_vertices(t, vertices);
    inst->u0 = vertices[2]; inst->v0 = vertices[3];
    inst->u1 = vertices[10]; inst->v1 = vertices[11];
    ctx->drawn_instances++;
}

void renderer_set_tint(renderer_ctx ctx, color_rgb color) {
    renderer_flush_batch(ctx);
    glUniform4f(ctx->uColorLoc, color.r, color.g, color.b, 1.0f);
}

void renderer_clear_tint(renderer_ctx ctx) {
    renderer_flush_batch(ctx);
    glUniform4f(ctx->uColorLoc, 1.0f, 1.0f, 1.0f, 1.0f);
}

static void renderer_begin_batch(renderer_ctx ctx) {
    glBindFramebuffer(GL_FRAMEBUFFER, ctx->fbo);
    glViewport(0, 0, ctx->logical_w, ctx->logical_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(ctx->shader_program);
    glBindVertexArray(ctx->t_buf.vao);

    glUniform2f(ctx->uScreenLoc, (float)ctx->logical_w, (float)ctx->logical_h);
    if (ctx->uPanLoc >= 0) {
        glUniform2f(ctx->uPanLoc, ctx->pan_x, ctx->pan_y);
    }
    glUniform4f(ctx->uColorLoc, 1.0f, 1.0f, 1.0f, 1.0f);

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

void renderer_cleanup(renderer_ctx ctx) {
    if (ctx == NULL) return;
    window_adapter *wa = (window_adapter *)glfwGetWindowUserPointer(ctx->window);
    if (wa != NULL) free(wa);
    if (ctx->instances_cpu != NULL) free(ctx->instances_cpu);
    glfwSetWindowUserPointer(ctx->window, NULL);
    glDeleteProgram(ctx->shader_program);
    glDeleteBuffers(1, &ctx->t_buf.instance_vbo);
    glDeleteBuffers(1, &ctx->t_buf.quad_vbo);
    glDeleteBuffers(1, &ctx->t_buf.ebo);
    glDeleteVertexArrays(1, &ctx->t_buf.vao);
    renderer_destroy_present_pipeline(ctx);
    renderer_destroy_offscreen(ctx);
    glfwDestroyWindow(ctx->window);
    glfwTerminate();
    free(ctx);
}
