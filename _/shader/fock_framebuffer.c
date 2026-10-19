/*
 * Compare the genuine compiler-generated Fock fragment against an independent
 * binary64 whole-plane Fock oracle through a real GLES3 framebuffer.
 * The runtime renderer must identify Mesa; this never counts as PowerVR evidence.
 * Built by qualified ICK on an Ubuntu runner; no device-side compiler.
 */
#define _POSIX_C_SOURCE 200809L
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef EGL_OPENGL_ES3_BIT
#define EGL_OPENGL_ES3_BIT 0x00000040
#endif

enum { WIDTH = 9, HEIGHT = 9, CHANNELS = 4, TOLERANCE_LSB = 2 };

static const char *vertex_shader_text =
    "#version 300 es\n"
    "precision highp float;\n"
    "const vec2 p[3]=vec2[3](vec2(-1.0,-1.0),vec2(3.0,-1.0),vec2(-1.0,3.0));\n"
    "out vec2 v_ndc;\n"
    "void main(){v_ndc=p[gl_VertexID];gl_Position=vec4(v_ndc,0.0,1.0);}\n";

static void fatal(const char *why) {
  fprintf(stderr, "Fock framebuffer: %s\n", why);
  exit(1);
}

static char *read_shader(const char *path) {
  FILE *in = fopen(path, "rb");
  if (!in) fatal("cannot open generated shader");
  if (fseek(in, 0, SEEK_END) != 0) fatal("cannot seek shader");
  long bytes = ftell(in);
  if (bytes <= 0 || bytes > 4000000) fatal("shader size invalid");
  rewind(in);
  char *buffer = malloc((size_t)bytes + 1);
  if (!buffer) fatal("out of memory");
  if (fread(buffer, 1, (size_t)bytes, in) != (size_t)bytes)
    fatal("short shader read");
  buffer[bytes] = '\0';
  fclose(in);
  return buffer;
}

static GLuint compile_shader(GLenum type, const char *source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);
  GLint success = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (success != GL_TRUE) {
    char log[4096] = {0};
    glGetShaderInfoLog(shader, sizeof log, NULL, log);
    fprintf(stderr, "GLSL compile: %s\n", log);
    fatal("generated shader failed actual GLES compile");
  }
  return shader;
}

static GLuint link_program(GLuint vertex, GLuint fragment) {
  GLuint program = glCreateProgram();
  glAttachShader(program, vertex);
  glAttachShader(program, fragment);
  glLinkProgram(program);
  GLint linked = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &linked);
  if (linked != GL_TRUE) {
    char log[4096] = {0};
    glGetProgramInfoLog(program, sizeof log, NULL, log);
    fprintf(stderr, "GLSL link: %s\n", log);
    fatal("generated shader failed actual GLES link");
  }
  return program;
}

static GLint required_uniform(GLuint program, const char *name) {
  GLint location = glGetUniformLocation(program, name);
  if (location < 0) {
    fprintf(stderr, "uniform %s missing\n", name);
    fatal("compiler lost a live input");
  }
  return location;
}

struct complex_number {
  double real;
  double imag;
};

static struct complex_number fock_q(double x, double y,
                                    double ax, double ay,
                                    double s,
                                    double amp_real, double amp_imag) {
  double inverse_scale_sq = 1.0 / (s * s);
  double argument_real = (x * ax + y * ay) * inverse_scale_sq;
  double argument_imag = (y * ax - x * ay) * inverse_scale_sq;
  double factor = exp(argument_real);
  double numerator_real = factor * cos(argument_imag) - 1.0;
  double numerator_imag = factor * sin(argument_imag);
  double denominator = exp((ax * ax + ay * ay) * inverse_scale_sq) - 1.0;
  struct complex_number output = {
    (amp_real * numerator_real - amp_imag * numerator_imag) / denominator,
    (amp_real * numerator_imag + amp_imag * numerator_real) / denominator
  };
  return output;
}

static int expected_byte(double component) {
  if (!isfinite(component)) fatal("host oracle produced non-finite output");
  if (component < 0.0 || component > 1.0) fatal("test corpus is clipped");
  return (int)lround(component * 255.0);
}

static int abs_delta(int actual, int expected) {
  int difference = actual - expected;
  return difference < 0 ? -difference : difference;
}

struct test_case {
  float anchor_x, anchor_y;
  float scale;
  float amp_x, amp_y;
};

int main(int argc, char **argv) {
  if (argc != 2) fatal("usage: fock-framebuffer generated-fock-value.frag");
  char *source = read_shader(argv[1]);
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (display == EGL_NO_DISPLAY) fatal("no EGL display");
  EGLint major = 0, minor = 0;
  if (!eglInitialize(display, &major, &minor)) fatal("EGL initialization failed");
  if (!eglBindAPI(EGL_OPENGL_ES_API)) fatal("OpenGL ES API binding failed");
  EGLint config_attributes[] = {
    EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8, EGL_NONE
  };
  EGLConfig config;
  EGLint configurations = 0;
  if (!eglChooseConfig(display, config_attributes, &config, 1, &configurations)
      || configurations != 1) fatal("no GLES3 RGBA8 pbuffer configuration");
  EGLint surface_attributes[] = {
    EGL_WIDTH, WIDTH, EGL_HEIGHT, HEIGHT, EGL_NONE
  };
  EGLSurface surface = eglCreatePbufferSurface(display, config, surface_attributes);
  if (surface == EGL_NO_SURFACE) fatal("cannot create 9x9 framebuffer");
  EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
  EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
  if (context == EGL_NO_CONTEXT) fatal("cannot create GLES3 context");
  if (!eglMakeCurrent(display, surface, surface, context))
    fatal("cannot activate GLES3 context");

  const char *vendor = (const char *)glGetString(GL_VENDOR);
  const char *renderer = (const char *)glGetString(GL_RENDERER);
  if (!vendor || !renderer) fatal("GL identity missing");
  printf("GL_VENDOR=%s\nGL_RENDERER=%s\n", vendor, renderer);
  if (!strstr(vendor, "Mesa") && !strstr(renderer, "llvmpipe") &&
      !strstr(renderer, "softpipe")) fatal("unexpected renderer; this is a Mesa-only gate");

  GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_shader_text);
  GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, source);
  free(source);
  GLuint program = link_program(vertex, fragment);
  glUseProgram(program);
  GLuint vao = 0;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  glDisable(GL_DITHER);
  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);

  GLint anchor_location = required_uniform(program, "u_anchor");
  GLint scale_location = required_uniform(program, "u_scale");
  GLint amplitude_location = required_uniform(program, "u_amplitude");

  const struct test_case cases[] = {
    {2.0f / 9.0f, 4.0f / 9.0f, 1.0f, 0.45f, -0.15f},
    {-4.0f / 9.0f, 2.0f / 9.0f, 1.2f, -0.30f, 0.22f},
    {4.0f / 9.0f, -2.0f / 9.0f, 0.9f, 0.32f, 0.38f}
  };
  int compared = 0;
  int maximum_error = 0;

  for (unsigned int sample = 0; sample < sizeof cases / sizeof cases[0]; ++sample) {
    struct test_case test = cases[sample];
    glUniform2f(anchor_location, test.anchor_x, test.anchor_y);
    glUniform1f(scale_location, test.scale);
    glUniform2f(amplitude_location, test.amp_x, test.amp_y);
    glViewport(0, 0, WIDTH, HEIGHT);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glFinish();
    if (glGetError() != GL_NO_ERROR) fatal("GL draw failed");
    unsigned char pixels[WIDTH * HEIGHT * CHANNELS];
    glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    if (glGetError() != GL_NO_ERROR) fatal("GL readback failed");
    for (int py = 0; py < HEIGHT; ++py) {
      for (int px = 0; px < WIDTH; ++px) {
        double x = ((double)px * 2.0 + 1.0) / WIDTH - 1.0;
        double y = ((double)py * 2.0 + 1.0) / HEIGHT - 1.0;
        struct complex_number q = fock_q(x, y, test.anchor_x,
                                         test.anchor_y, test.scale,
                                         test.amp_x, test.amp_y);
        int expected[4] = {
          expected_byte(0.5 + 0.25 * q.real),
          expected_byte(0.5 + 0.25 * q.imag),
          expected_byte(0.25),
          255
        };
        const unsigned char *actual = &pixels[(py * WIDTH + px) * CHANNELS];
        for (int channel = 0; channel < CHANNELS; ++channel) {
          int difference = abs_delta(actual[channel], expected[channel]);
          if (difference > maximum_error) maximum_error = difference;
          if (difference > TOLERANCE_LSB) {
            fprintf(stderr,
                    "case=%u pixel=(%d,%d) channel=%d actual=%u expected=%d"
                    " difference=%d\n",
                    sample, px, py, channel, actual[channel],
                    expected[channel], difference);
            fatal("binary64/GLES framebuffer parity failed");
          }
          ++compared;
        }
      }
    }
    printf("case_%u: PASS anchor=(%g,%g) scale=%g amplitude=(%g,%g)\n",
           sample, test.anchor_x, test.anchor_y, test.scale,
           test.amp_x, test.amp_y);
  }
  printf("fock_framebuffer: PASS compared=%d max_rgb8_error=%d tolerance=%d"
         " vendor=Mesa\n", compared, maximum_error, TOLERANCE_LSB);
  glDeleteProgram(program);
  glDeleteShader(vertex);
  glDeleteShader(fragment);
  glDeleteVertexArrays(1, &vao);
  eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(display, context);
  eglDestroySurface(display, surface);
  eglTerminate(display);
  return 0;
}
