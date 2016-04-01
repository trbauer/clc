#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <algorithm>
#include <iterator>
#ifdef _WIN32
#include <Windows.h>
#include <malloc.h>
// for _setmode(_fileno())...
#include <fcntl.h>
#include <io.h>
#define VSCPRINTF(PAT,VA) \
    _vscprintf(PAT,VA)
#define VSPRINTF(B,BLEN,...) \
    vsprintf_s(B, BLEN, __VA_ARGS__)
#define alloca _alloca
#else
#define VSCPRINTF(PAT,VA) \
    vsnprintf(NULL, 0, PAT, VA)
#define VSPRINTF(B,BLEN,...) \
    vsprintf(B,__VA_ARGS__)
#include <alloca.h>
#endif
#define __CL_ENABLE_EXCEPTIONS
#define CL_USE_DEPRECATED_OPENCL_2_0_APIS
#include <CL/cl.hpp>

#include "clerrs.h"

#define MKBUF(F, PAT) \
    va_list ap; \
    va_start(ap, PAT); \
    size_t _ebuflen = VSCPRINTF(PAT, ap) + 1; \
    va_end(ap); \
    \
    char *_buf = (char *)alloca(_ebuflen); \
    va_start(ap, pat); \
    VSPRINTF(_buf, _ebuflen, PAT, ap); \
    va_end(ap); \
    _buf[_ebuflen - 1] = 0; \
    F((std::string)_buf)

static std::string g_exe;

void fatalMessage(const std::string &str) {
    fputs(g_exe.c_str(), stdout);
    fputs(": ", stdout);
    fputs(str.c_str(), stderr);
    if (str.length() > 0 && str[str.length() - 1] != '\n')
        fputc('\n', stderr);
}
void fatal(const std::string &str) {
    fatalMessage(str);
    exit(EXIT_FAILURE);
}
void fatal(const char *pat,...) {
    MKBUF(fatal, pat);
}
void warning(const std::string &str) {
    fputs(str.c_str(), stderr);
}
void warning(const char *pat,...) {
    MKBUF(warning, pat);
}
int g_verbosity = 0;
void verbose(const std::string &str) {
    if (g_verbosity > 0)
        fputs(str.c_str(), stderr);
}
void verbose(const char *pat,...) {
    if (g_verbosity > 0) { MKBUF(verbose, pat); }
}
void debug(const std::string &str) {
    if (g_verbosity > 1)
        fputs(str.c_str(), stderr);
}
void debug(const char *pat,...) {
    if (g_verbosity > 1) { MKBUF(debug, pat); }
}


struct Opts {
    std::vector<std::string>   args;
    std::string                device; // substring match
    std::string                output;
    std::vector<std::string>   buildOpts;
    int                        verbosity = 0;
    bool                       listDevices = false;
};


static void printUsage(FILE *stream) {
    fprintf(stream,
        "OpenCL Offline Compiler (" VERSION_STRING ")\n"
        "usage: %s [OPTS] ARGS\n"
        "where [OPTS]\n"
        " -d=DEV          targets device with a substring in it's name\n"
        " -b=BUILD-OPTS   adds a build option (e.g. -b=-DTILE=32)\n"
        " -q/-v/-v2       quiet/verbose/debug\n"
        " -h=d            list devices\n"
        "[ARGS]           is list of compilation units (.cl files)\n"
        "\n"
        "EXAMPLES:\n"
        " %% clc foo.cl        saves foo.bin as the output for the default device\n"
        " %% clc -d=Intel ...  selects a device containing \"Intel\" in it's CL_DEVICE_NAME\n"
        "",
        g_exe.c_str());

}

static Opts parseOpts(int argc, const char **argv)
{
    int ai = 0;
    auto argpfx = [&](const char *pfx) {
        return (strncmp(argv[ai], pfx, strlen(pfx)) == 0);
    };
    auto argeq = [&](const char *pfx) {
        return (strcmp(argv[ai], pfx) == 0);
    };
    auto badArg = [&](const char *msg) {
        std::string str = g_exe;
        str += ": ";
        str = str + argv[ai] + ": " + msg + "\n";
        fatalMessage(msg);
        printUsage(stderr);
        exit(EXIT_FAILURE);
    };

    Opts opts;
    for (; ai < argc;) {
        if (argeq("--help") || argeq("-h")) {
            printUsage(stdout);
            exit(EXIT_SUCCESS);

        } else if (argeq("-h=d")) {
            opts.listDevices = true;
            ai++;

        // verbosity
        } else if (argeq("-q") || argeq("-v-1") || argeq("-v=-1")) {
            opts.verbosity = -1;
            ai++;
        } else if (argeq("-v") || argeq("-v1") || argeq("-v=1")) {
            opts.verbosity = 1;
            ai++;
        } else if (argeq("-v2") || argeq("-v=2")) {
            opts.verbosity = 2;
            ai++;
        } else if (argpfx("-v")) {
            badArg("unexpected verbosity option");

        // build options
        } else if (argpfx("-b=")) {
            opts.buildOpts.emplace_back(argv[ai] + 3);
            ai++;
        } else if (argpfx("-b")) {
            badArg("must be of the form -b=...");

        // -d=device selection
        } else if (argpfx("-d=")) {
            const char *str = argv[ai] + 3;
            if (opts.device.length() > 0) {
                badArg("argument respecified");
            }
            opts.device = str;
            ai++;
        } else if (argpfx("-d")) {
            badArg("must be of the form -d=...");

        } else if (argpfx("-o=")) {
            if (opts.output.length() > 0) {
                badArg("argument respecified");
            }
            opts.output = argv[ai] + 3;
            ai++;
        } else if (argpfx("-o")) {
            badArg("must be of the form -o=...");

        } else if (!argeq("-") && argpfx("-")) {
            // - is output
            badArg("unexpected option");
        } else {
            opts.args.emplace_back(argv[ai++]);
        }
    }
    return opts;
}


static cl::Device findDevice(const Opts &opts)
{
    debug("selecting matching device %s\n", opts.device.c_str());
    std::vector<cl::Device> matching;
    if (opts.device.length() == 0) {
        matching.emplace_back(cl::Device::getDefault());
    } else {
        std::vector<cl::Platform> ps;
        cl::Platform::get(&ps);

        bool alreadyMatched = false;
        for (auto &p : ps) {
            debug("scanning platform %s\n", p.getInfo<CL_PLATFORM_NAME>().c_str());
            std::vector<cl::Device> ds;
            p.getDevices(CL_DEVICE_TYPE_ALL, &ds);

            for (auto &d : ds) {
                std::string dstr = d.getInfo<CL_DEVICE_NAME>();
                debug("  scanning device %s\n", dstr.c_str());

                if (dstr.find(opts.device) != std::string::npos) {
                    matching.emplace_back(d);
                }
            }
        }
    }
    if (matching.empty()) {
        fatal("-d=%s: unable to find matching device", opts.device.c_str());
    } else if (matching.size() > 1) {
        std::stringstream ss;
        ss << "-d=" << opts.device << ": matches multiple devices\n";
        for (auto &d : matching) {
            auto p = d.getInfo<CL_DEVICE_PLATFORM>();
            cl::Platform p2(p);
            ss << "  - " << d.getInfo<CL_DEVICE_NAME>() <<
                "  (from " << p2.getInfo<CL_PLATFORM_NAME>() << ")\n";
        }
        fatal(ss.str());
    }
    debug("  => picked device %s\n", matching[0].getInfo<CL_DEVICE_NAME>().c_str());
    return matching[0];
}

static void emitProperty(const char *prop)
{
    std::cout << "  " << prop << ": ";
    for (int i = 0, len = 48 - (int)strlen(prop); i < len; i++)
        std::cout << ' ';

}
template <typename T>
static void emitPropertyIntegralUnits(const char *prop, const T &val, const char *units)
{
    emitProperty(prop);
    std::cout << val;
    if (units && *units)
        std::cout << " " << units;
    std::cout << "\n";
}
template <typename T>
static void emitPropertyIntegralBytes(const char *prop, T val)
{
    const char *units = "B";
    if (val % (1024 * 1024) == 0) {
        val = (val >> 20);
        units = "MB";
    } else if (val % 1024 == 0) {
        val = (val >> 10);
        units = "KB";
    }
    emitPropertyIntegralUnits(prop, val, units);
}
static void emitBoolProperty(const char *prop, cl_bool val)
{
    emitProperty(prop);
    std::cout << (val ? "CL_TRUE" : "CL_FALSE") << "\n";
}
#define EMIT_DEVICE_PROPERTY_UNITS(SYM,UNITS)  emitPropertyIntegralUnits(#SYM, d.getInfo<SYM>(), UNITS)
#define EMIT_DEVICE_PROPERTY_MEM(SYM)  emitPropertyIntegralBytes(#SYM, d.getInfo<SYM>())
#define EMIT_DEVICE_PROPERTY_BOOL(SYM)  emitBoolProperty(#SYM, d.getInfo<SYM>())


static void listDevices(const Opts &opts)
{
    std::vector<cl::Platform> ps;
    cl::Platform::get(&ps);
    for (auto &p : ps) {
        std::vector<cl::Device> ds;
        p.getDevices(CL_DEVICE_TYPE_ALL, &ds);

        for (auto &d : ds) {
            std::cout << "DEVICE: \"" << d.getInfo<CL_DEVICE_NAME>() << "\"";
            std::cout << "\n";
            if ( opts.verbosity > 0) {
                EMIT_DEVICE_PROPERTY_UNITS(CL_DEVICE_VERSION,nullptr);
                EMIT_DEVICE_PROPERTY_UNITS(CL_DEVICE_VENDOR,nullptr);
                EMIT_DEVICE_PROPERTY_UNITS(CL_DRIVER_VERSION,nullptr);
                emitProperty("CL_DEVICE_TYPE");
                switch (d.getInfo<CL_DEVICE_TYPE>()) {
                case CL_DEVICE_TYPE_CPU: std::cout << "CL_DEVICE_TYPE_CPU"; break;
                case CL_DEVICE_TYPE_GPU: std::cout << "CL_DEVICE_TYPE_GPU"; break;
                case CL_DEVICE_TYPE_ACCELERATOR: std::cout << "CL_DEVICE_TYPE_ACCELERATOR"; break;
                case CL_DEVICE_TYPE_DEFAULT: std::cout << "CL_DEVICE_TYPE_DEFAULT"; break;
                default: std::cout << d.getInfo<CL_DEVICE_TYPE>() << "?"; break;
                }
                std::cout << "\n";
                std::cout << "  COMPUTE:\n";
                EMIT_DEVICE_PROPERTY_UNITS(CL_DEVICE_MAX_CLOCK_FREQUENCY,"MHz");
                EMIT_DEVICE_PROPERTY_UNITS(CL_DEVICE_MAX_COMPUTE_UNITS, nullptr);
                EMIT_DEVICE_PROPERTY_UNITS(CL_DEVICE_PROFILING_TIMER_RESOLUTION, "ns");
                EMIT_DEVICE_PROPERTY_BOOL(CL_DEVICE_ENDIAN_LITTLE);
                // EMIT_DEVICE_PROPERTY_MEM(CL_DEVICE_PRINTF_BUFFER_SIZE); (not supported

                emitProperty("CL_DEVICE_SINGLE_FP_CONFIG"); {
                    auto fpcfg = d.getInfo<CL_DEVICE_SINGLE_FP_CONFIG>();
                    const char *sep = ""; // "|";
                    if ((fpcfg & CL_FP_DENORM)) {
                        std::cout << sep << "CL_FP_DENORM";
                        sep = "|";
                    }
                    if ((fpcfg & CL_FP_INF_NAN)) {
                        std::cout << sep << "CL_FP_INF_NAN";
                        sep = "|";
                    }
                    if ((fpcfg & CL_FP_ROUND_TO_NEAREST)) {
                        std::cout << sep << "CL_FP_ROUND_TO_NEAREST";
                        sep = "|";
                    }
                    if ((fpcfg & CL_FP_ROUND_TO_ZERO)) {
                        std::cout << sep << "CL_FP_ROUND_TO_ZERO";
                        sep = "|";
                    }
                    if ((fpcfg & CL_FP_ROUND_TO_INF)) {
                        std::cout << sep << "CL_FP_ROUND_TO_INF";
                        sep = "|";
                    }
                    if ((fpcfg & CL_FP_FMA)) {
                        std::cout << sep << "CL_FP_FMA";
                        sep = "|";
                    }
                    if ((fpcfg & CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT)) {
                        std::cout << sep << "CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT";
                        sep = "|";
                    }
                    if ((fpcfg & CL_FP_SOFT_FLOAT)) {
                        std::cout << sep << "CL_FP_SOFT_FLOAT";
                        sep = "|";
                    }
                    std::cout << "\n";
                }

                emitProperty("CL_DEVICE_QUEUE_PROPERTIES"); {
                    const char *sep = ""; // "|";
                    auto devq = d.getInfo<CL_DEVICE_QUEUE_PROPERTIES>();
                    if ((devq & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE)) {
                        std::cout << sep << "CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE";
                        sep = "|";
                    }
                    if ((devq & CL_QUEUE_PROFILING_ENABLE)) {
                        std::cout << sep << "CL_QUEUE_PROFILING_ENABLE";
                        sep = "|";
                    }
                    std::cout << "\n";
                }

                std::cout << "  WORKGROUPS:\n";
                EMIT_DEVICE_PROPERTY_UNITS(CL_DEVICE_MAX_WORK_GROUP_SIZE,"items");
                auto dim = d.getInfo<CL_DEVICE_MAX_WORK_ITEM_SIZES>();
                emitProperty("CL_DEVICE_MAX_WORK_ITEM_SIZES");
                std::cout << dim[0];
                for (size_t i = 1; i < dim.size(); i++) {
                    std::cout << 'x' << dim[i];
                }
                std::cout << "\n";
                EMIT_DEVICE_PROPERTY_UNITS(CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR,nullptr);
                EMIT_DEVICE_PROPERTY_UNITS(CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT,nullptr);
                std::cout << "  MEMORY:\n";
                EMIT_DEVICE_PROPERTY_UNITS(CL_DEVICE_ADDRESS_BITS,"b");
                EMIT_DEVICE_PROPERTY_MEM(CL_DEVICE_MEM_BASE_ADDR_ALIGN);
                EMIT_DEVICE_PROPERTY_MEM(CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE);
                EMIT_DEVICE_PROPERTY_MEM(CL_DEVICE_LOCAL_MEM_SIZE);
                EMIT_DEVICE_PROPERTY_MEM(CL_DEVICE_GLOBAL_MEM_SIZE);
                emitProperty("CL_DEVICE_GLOBAL_MEM_CACHE_TYPE");
                switch (d.getInfo<CL_DEVICE_GLOBAL_MEM_CACHE_TYPE>()) {
                case CL_NONE: std::cout << "CL_NONE"; break;
                case CL_READ_ONLY_CACHE: std::cout << "CL_READ_ONLY_CACHE"; break;
                case CL_READ_WRITE_CACHE: std::cout << "CL_READ_WRITE_CACHE"; break;
                default: std::cout << d.getInfo<CL_DEVICE_GLOBAL_MEM_CACHE_TYPE>() << "?\n"; break;
                }
                std::cout << "\n";
                EMIT_DEVICE_PROPERTY_MEM(CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE);
                EMIT_DEVICE_PROPERTY_MEM(CL_DEVICE_GLOBAL_MEM_CACHE_SIZE);
                std::cout << "  IMAGES:\n";
                EMIT_DEVICE_PROPERTY_BOOL(CL_DEVICE_IMAGE_SUPPORT);
                EMIT_DEVICE_PROPERTY_UNITS(CL_DEVICE_IMAGE2D_MAX_HEIGHT,"px");
                EMIT_DEVICE_PROPERTY_UNITS(CL_DEVICE_IMAGE2D_MAX_WIDTH,"px");
                // EMIT_DEVICE_PROPERTY_UNITS(CL_DEVICE_IMAGE_PITCH_ALIGNMENT,nullptr); // OCL 2.0?
                std::cout << "  EXTENSIONS:\n";
                std::istringstream iss(d.getInfo<CL_DEVICE_EXTENSIONS>());
                std::vector<std::string> tokens;
                std::copy(std::istream_iterator<std::string>(iss),
                    std::istream_iterator<std::string>(),
                    std::back_inserter(tokens));
                for (auto tk : tokens) {
                    std::cout << "  " << tk << "\n";
                }
            }
        }
    }
}

static std::string readTextStream(
    const std::string &streamName,
    std::istream &is)
{
    std::string s;
    is.clear();
    s.append(std::istreambuf_iterator<char>(is),
             std::istreambuf_iterator<char>());
    if (!is.good()) {
        fatal("%s: error reading", streamName.c_str());
    }
    return s;
}

static std::string readTextFile(
    const std::string &fileName)
{
    std::ifstream file(fileName);
    if (!file.good()) {
        fatal("%s: failed to open file", fileName.c_str());
    }
    return readTextStream(fileName, file);
}

static void writeBinary(
    const std::string &streamName, const void *bits, size_t bitsLen)
{
    if (streamName.length() == 0) {
// have to use stdio here since C++ will not let us output binary
#ifdef WIN32
        _setmode(_fileno(stdout), _O_BINARY);
#else
        setmode(fileno(stdout), O_BINARY);
#endif
        if (fwrite(bits, 1, bitsLen, stdout) < bitsLen) {
            fatal("failed to write entire output");
        }
    } else {
        std::ofstream os(streamName,std::ios::binary);
        if (!os.good()) {
            fatal("%s: failed to open file", streamName.c_str());
        }
        os.clear();
        os.write((const char *)bits, bitsLen);
        if (!os.good()) {
            fatal("%s: error writing", streamName.c_str());
        }
    }
}

int main(int argc, const char **argv)
{
    g_exe = argv[0];
    auto ix = g_exe.rfind('\\');
    if (ix != std::string::npos)
        g_exe = g_exe.substr(ix + 1);
    else if ((ix = g_exe.rfind('/')) != std::string::npos)
        g_exe = g_exe.substr(ix + 1);

    Opts opts = parseOpts(argc - 1, argv + 1);
    g_verbosity = opts.verbosity;

    cl::Device dev = findDevice(opts);

    std::stringstream bss;
    for (size_t i = 0; i < opts.buildOpts.size(); i++) {
        if (i > 0)
            bss << " ";
        bss << opts.buildOpts[i];
    }
    std::string buildOpts = bss.str();

    if (opts.listDevices) {
        listDevices(opts);
        if (!opts.args.empty()) {
            fatal("-h=d specified, ignorning arguments");
        }
        return 0;
    }

    // load the source
    if (opts.args.empty()) {
        fatal("expected input argument");
    }
    cl::Program::Sources sources;
    std::vector<std::string> sourceStrs;
    for (auto &f : opts.args) {
        auto str = f == "-" ?
            readTextStream("stdin", std::cin) : readTextFile(f);
        sourceStrs.emplace_back(str);
        sources.emplace_back(sourceStrs.back().c_str(), str.size());
    }

    // create the context
    cl::Context ctx(dev);

    // create the program
    cl::Program prog(ctx, sources, false); // no autobuild
    // build it
    try {
        // attempt to build the program
        prog.build(buildOpts.c_str());
        auto bl = prog.getBuildInfo<CL_PROGRAM_BUILD_LOG>(dev);
        if (bl.find("warning") != std::string::npos) {
            warning("warnings during build:\n%s", bl.c_str());
        } else {
            verbose("build log:\n%s", bl.c_str());
        }
    } catch (const cl::Error &err) {
        auto bl = prog.getBuildInfo<CL_PROGRAM_BUILD_LOG>(dev);
        fatal("during build: %s (%s):\n%s",
            err.what(), clErrStr(err.err()).c_str(), bl.c_str());
    }

    // save the binary
    std::vector<size_t> binSizes =
        prog.getInfo<CL_PROGRAM_BINARY_SIZES>();
    char *binBuf = (char *)alloca(binSizes[0]);
    std::vector<char *> binPtrs;
    binPtrs.push_back(binBuf);
    try {
        prog.getInfo(CL_PROGRAM_BINARIES, &binPtrs);
    } catch (const cl::Error &err) {
        fatal("clGetProgramInfo(..CL_PROGRAM_BINARIES..): %s (%s)",
            err.what(), clErrStr(err.err()).c_str());
    }

    if (opts.output.length() == 0) {
        std::string filename;
        if (opts.args[0] == "--") {
            filename = "stdin";
        } else {
            auto arg0 = opts.args[0];
            auto fslash = arg0.rfind('/');
            auto bslash = arg0.rfind('\\');
            if (fslash != std::string::npos) {
                // foo/bar/baz/foo.cl
                //            ^
                filename = arg0.substr(fslash+1);
            } else if (bslash != std::string::npos) {
                // foo\\bar\\baz\\foo.cl
                //              ^^
                filename = arg0.substr(bslash+1);
            } else {
                // foo.cl
                filename = arg0;
            }
            auto ext = filename.rfind(".cl");
            if (ext != std::string::npos) {
                filename = filename.substr(0, ext); // foo.cl -> foo
            }
        }
        // foo -> foo.8086.bin
        std::stringstream ss;
        ss << filename << ".";
        if (dev.getInfo<CL_DEVICE_VENDOR_ID>() == 0x10de) {
            ss << "ptx";
        } else if (dev.getInfo<CL_DEVICE_VENDOR_ID>() == 0x8086) {
            ss << "elf";
        } else {
            ss << std::hex << std::setfill('0') << std::setw(4) << dev.getInfo<CL_DEVICE_VENDOR_ID>();
            ss << "bin";
        }
        opts.output = ss.str();
    }

    if (opts.output == "--") {
        verbose("saving binary to stdout\n");
        writeBinary("", binPtrs[0], binSizes[0]);
    } else {
        verbose("saving binary to %s\n", opts.output.c_str());
        writeBinary(opts.output, binPtrs[0], binSizes[0]);
    }
    return 0;
}

