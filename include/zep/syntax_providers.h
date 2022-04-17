#pragma once

#include <unordered_set>

namespace Zep {

struct ZepEditor;

// Faust syntax highlighting https://github.com/grame-cncm/faust/tree/bc9536414dcaf3ff199be54888eea5d93dee90b9/syntax-highlighting
// The simplest one is probably https://github.com/grame-cncm/faust/blob/bc9536414dcaf3ff199be54888eea5d93dee90b9/syntax-highlighting/faust.nanorc

// TODO need more robust syntax highlighting in general.
// TODO faust emacs syntax is a good example for pulling more faust keywords from the faust libraries
//  https://github.com/grame-cncm/faust/blob/bc9536414dcaf3ff199be54888eea5d93dee90b9/syntax-highlighting/faust-mode.el

// From https://github.com/grame-cncm/faust/blob/bc9536414dcaf3ff199be54888eea5d93dee90b9/syntax-highlighting/faust.vim
static std::unordered_set<std::string> faust_keywords = {
    "syn", "keyword", "fstPrims", "mem", "prefix", "int", "float", "rdtable", "rwtable", "select2", "select3", "ffunction", "fconstant", "fvariable", "route", "waveform", "soundfile", "button", "checkbox", "vslider",
    "hslider", "nentry", "vgroup", "hgroup", "tgroup", "vbargraph", "hbargraph", "attach", "acos", "asin", "atan", "atan2", "cos", "sin", "tan", "exp", "log", "log10", "pow", "sqrt", "abs", "min", "max", "fmod",
    "remainder", "floor", "ceil", "rint",
};
static std::unordered_set<std::string> faust_identifiers = {
    "syn", "keyword", "fstOps", "process", "with", "case", "seq", "par", "sum", "prod", "import", "component", "library", "environment", "declare",
};

// Most of these keyword values taken from : https://github.com/BalazsJako/ImGuiColorTextEdit
// another great ImGui based text editor.
// I'll fill these out when I get time.  At the moment the syntax code matches on these, comments and numbers.
static std::unordered_set<std::string> cpp_keywords = {
    "alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit", "atomic_noexcept", "auto", "bitand", "bitor", "bool", "break", "case", "catch", "char", "char16_t", "char32_t", "class",
    "compl", "concept", "const", "constexpr", "const_cast", "continue", "decltype", "default", "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false", "float",
    "for", "friend", "goto", "if", "import", "inline", "int", "long", "module", "mutable", "namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected", "public",
    "register", "reinterpret_cast", "requires", "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast", "struct", "switch", "synchronized", "template", "this", "thread_local",
    "throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq", "#define", "#include",
    "uint32_t", "int32_t", "uint64_t", "int64_t", "size_t", "uint8_t", "int8_t", "int16_t", "uint16_t"
};
static std::unordered_set<std::string> cpp_identifiers = {
    "abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph",
    "ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "printf", "sprintf", "snprintf", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat",
    "strcmp", "strerror", "time", "tolower", "toupper",
    "std", "string", "vector", "map", "unordered_map", "set", "unordered_set", "min", "max"
};

static std::unordered_set<std::string> toml_keywords = {};
static std::unordered_set<std::string> toml_identifiers = {};

static std::unordered_set<std::string> hlsl_keywords = {
    "CompileShader", "const", "continue", "ComputeShader", "ConsumeStructuredBuffer", "default", "DepthStencilState", "DepthStencilView", "discard", "do", "double", "DomainShader", "dword", "else", "export", "extern",
    "false", "float", "for", "fxgroup", "GeometryShader", "groupshared", "half", "Hullshader", "if", "in", "inline", "inout", "InputPatch", "int", "interface", "line", "lineadj", "linear", "LineStream", "matrix",
    "min16float",
    "min10float", "min16int", "min12int", "min16uint", "namespace", "nointerpolation", "noperspective", "NULL", "out", "OutputPatch", "packoffset", "pass", "pixelfragment", "PixelShader", "point", "PointStream",
    "precise",
    "RasterizerState", "RenderTargetView", "return", "register", "row_major", "RWBuffer", "RWByteAddressBuffer", "RWStructuredBuffer", "RWTexture1D", "RWTexture1DArray", "RWTexture2D", "RWTexture2DArray", "RWTexture3D",
    "sample", "sampler", "SamplerState", "SamplerComparisonState", "shared", "snorm", "stateblock", "stateblock_state", "static", "string", "struct", "switch", "StructuredBuffer", "tbuffer", "technique", "technique10",
    "technique11", "texture", "Texture1D", "Texture1DArray", "Texture2D", "Texture2DArray", "Texture2DMS", "Texture2DMSArray", "Texture3D", "TextureCube", "TextureCubeArray", "true", "typedef", "triangle", "triangleadj",
    "TriangleStream", "uint", "uniform", "unorm", "unsigned", "vector", "vertexfragment", "VertexShader", "void", "volatile", "while", "bool1", "bool2", "bool3", "bool4", "double1", "double2", "double3", "double4",
    "float1",
    "float2", "float3", "float4", "int1", "int2", "int3", "int4", "in", "out", "inout", "uint1", "uint2", "uint3", "uint4", "dword1", "dword2", "dword3", "dword4", "half1", "half2", "half3", "half4", "float1x1",
    "float2x1",
    "float3x1", "float4x1", "float1x2", "float2x2", "float3x2", "float4x2", "float1x3", "float2x3", "float3x3", "float4x3", "float1x4", "float2x4", "float3x4", "float4x4", "half1x1", "half2x1", "half3x1", "half4x1",
    "half1x2",
    "half2x2", "half3x2", "half4x2", "half1x3", "half2x3", "half3x3", "half4x3", "half1x4", "half2x4", "half3x4", "half4x4"
};
static std::unordered_set<std::string> hlsl_identifiers = {
    "abort", "abs", "acos", "all", "AllMemoryBarrier", "AllMemoryBarrierWithGroupSync", "any", "asdouble", "asfloat", "asin", "asint", "asint", "asuint", "asuint", "atan", "atan2", "ceil", "CheckAccessFullyMapped",
    "clamp",
    "clip", "cos", "cosh", "countbits", "cross", "D3DCOLORtoUBYTE4", "ddx", "ddx_coarse", "ddx_fine", "ddy", "ddy_coarse", "ddy_fine", "degrees", "determinant", "DeviceMemoryBarrier", "DeviceMemoryBarrierWithGroupSync",
    "distance", "dot", "dst", "errorf", "EvaluateAttributeAtCentroid", "EvaluateAttributeAtSample", "EvaluateAttributeSnapped", "exp", "exp2", "f16tof32", "f32tof16", "faceforward", "firstbithigh", "firstbitlow",
    "floor",
    "fma", "fmod", "frac", "frexp", "fwidth", "GetRenderTargetSampleCount", "GetRenderTargetSamplePosition", "GroupMemoryBarrier", "GroupMemoryBarrierWithGroupSync", "InterlockedAdd", "InterlockedAnd",
    "InterlockedCompareExchange",
    "InterlockedCompareStore", "InterlockedExchange", "InterlockedMax", "InterlockedMin", "InterlockedOr", "InterlockedXor", "isfinite", "isinf", "isnan", "ldexp", "length", "lerp", "lit", "log", "log10", "log2", "mad",
    "max", "min",
    "modf", "msad4", "mul", "noise", "normalize", "pow", "printf", "Process2DQuadTessFactorsAvg", "Process2DQuadTessFactorsMax", "Process2DQuadTessFactorsMin", "ProcessIsolineTessFactors", "ProcessQuadTessFactorsAvg",
    "ProcessQuadTessFactorsMax",
    "ProcessQuadTessFactorsMin", "ProcessTriTessFactorsAvg", "ProcessTriTessFactorsMax", "ProcessTriTessFactorsMin", "radians", "rcp", "reflect", "refract", "reversebits", "round", "rsqrt", "saturate", "sign", "sin",
    "sincos", "sinh", "smoothstep",
    "sqrt", "step", "tan", "tanh", "tex1D", "tex1D", "tex1Dbias", "tex1Dgrad", "tex1Dlod", "tex1Dproj", "tex2D", "tex2D", "tex2Dbias", "tex2Dgrad", "tex2Dlod", "tex2Dproj", "tex3D", "tex3D", "tex3Dbias", "tex3Dgrad",
    "tex3Dlod", "tex3Dproj",
    "texCUBE", "texCUBE", "texCUBEbias", "texCUBEgrad", "texCUBElod", "texCUBEproj", "transpose", "trunc"
};

// From here: https://www.khronos.org/registry/OpenGL/specs/gl/GLSLangSpec.4.00.pdf
static std::unordered_set<std::string> glsl_keywords{
    "#version", "attribute", "const", "uniform", "varying", "layout", "centroid", "flat", "smooth", "noperspective", "patch", "sample", "break", "continue", "do", "for", "while", "switch", "case", "default",
    "if", "else", "subroutine", "in", "out", "inout", "float", "double", "int", "void", "bool", "true", "false", "invariant", "discard", "return", "mat2", "mat3", "mat4", "dmat2", "dmat3", "dmat4",
    "mat2x2", "mat2x3", "mat2x4", "dmat2x2", "dmat2x3", "dmat2x4", "mat3x2", "mat3x3", "mat3x4", "dmat3x2", "dmat3x3", "dmat3x4", "mat4x2", "mat4x3", "mat4x4", "dmat4x2", "dmat4x3", "dmat4x4",
    "vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4", "bvec2", "bvec3", "bvec4", "dvec2", "dvec3", "dvec4", "uint", "uvec2", "uvec3", "uvec4", "lowp", "mediump", "highp", "precision",
    "sampler1D", "sampler2D", "sampler3D", "samplerCube", "sampler1DShadow", "sampler2DShadow", "samplerCubeShadow", "sampler1DArray", "sampler2DArray", "sampler1DArrayShadow", "sampler2DArrayShadow",
    "isampler1D", "isampler2D", "isampler3D", "isamplerCube", "isampler1DArray", "isampler2DArray", "usampler1D", "usampler2D", "usampler3D", "usamplerCube", "usampler1DArray", "usampler2DArray",
    "sampler2DRect", "sampler2DRectShadow", "isampler2DRect", "usampler2DRect", "samplerBuffer", "isamplerBuffer", "usamplerBuffer", "sampler2DMS", "isampler2DMS", "usampler2DMS",
    "sampler2DMSArray", "isampler2DMSArray", "usampler2DMSArray", "samplerCubeArray", "samplerCubeArrayShadow", "isamplerCubeArray", "usamplerCubeArray", "struct"
};
static std::unordered_set<std::string> glsl_identifiers = {
    "abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph",
    "ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time",
    "tolower", "toupper", "gl_Position"
};

static std::unordered_set<std::string> c_keywords = {
    "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short",
    "signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary",
    "_Noreturn", "_Static_assert", "_Thread_local"
};
static std::unordered_set<std::string> c_identifiers = {
    "abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph",
    "ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time",
    "tolower", "toupper"
};

} // namespace Zep
