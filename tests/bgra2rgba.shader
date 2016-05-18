// simple BGRA to RGBA shader
namespace {
const std::string VERTEX_SHADER = "#version 330 core\n"
    "in vec4 ciPosition;\n"
    "in vec2 ciTexCoord0;\n"
    "uniform mat4 ciModelViewProjection;\n"
    "smooth out vec2 vTexCoord;\n"
    "void main() {\n"
    "  gl_Position = ciModelViewProjection * ciPosition;\n"
    "  vTexCoord = vec2(ciTexCoord0.x, 1.0 - ciTexCoord0.y);\n"
    "}";

const std::string FRAG_SHADER = "#version 330 core\n"
    "uniform sampler2D uTex0;\n"
    "smooth in vec2 vTexCoord;\n"
    "out vec4 fColor;\n"
    "void main()\n"
    "{\n"
    "  fColor = texture( uTex0, vTexCoord ).bgra;\n"
    "}\n";
}
