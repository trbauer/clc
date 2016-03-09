#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
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

void fatalMessage(const std::string &str) {
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
};



static void printUsage() {
    const char *msg =
        "OpenCL Offline Compiler\n"
        "usage: clc [OPTS] ARGS\n"
        "where [OPTS]\n"
        " -d=DEV          targets device with a substring in it's name\n"
        " -b=BUILD-OPTS   adds a build option (e.g. -b=-DTILE=32)\n"
        " -q/-v/-v2       quiet/verbose/debug\n"
        "[ARGS]           is list of compilation units (.cl files)\n"
        "\n"
        "EXAMPLES:\n"
        " % clc foo.cl        saves foo.bin as the output for the default device\n"
        " % clc -d=Intel ...  selects a device containing \"Intel\" in it's CL_DEVICE_NAME\n"
        "";
    fputs(msg, stderr);
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
        std::string str = "";
        str = str + argv[ai] + ": " + msg + "\n";
        fatalMessage(msg);
        printUsage();
        exit(EXIT_FAILURE);
    };

    Opts opts;
    for (; ai < argc;) {
        if (argpfx("--help") || argpfx("-h")) {
            printUsage();
            exit(EXIT_SUCCESS);
        } else if (argeq("-q") || argeq("-v-1")) {
            opts.verbosity = -1;
            ai++;
        } else if (argeq("-v") || argeq("-v1")) {
            opts.verbosity = 1;
            ai++;
        } else if (argeq("-v2")) {
            opts.verbosity = 2;
            ai++;
        } else if (argpfx("-v")) {
            badArg("unexpected verbosity option");

        } else if (argeq("-b=")) {
            const char *str = argv[ai] + 3;
            opts.args.emplace_back(argv[ai]);
            ai++;
        } else if (argpfx("-b")) {
            badArg("must be of the form -b=...");

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
            auto ext = arg0.rfind(".cl");
            if (ext != std::string::npos) {
                filename = filename.substr(0, ext); // foo.cl -> foo
            }
        }
        // foo -> foo.8086.bin
        std::stringstream ss;
        ss << filename << ".";
        ss << std::hex << std::setfill('0') << std::setw(4) << dev.getInfo<CL_DEVICE_VENDOR_ID>();
        ss << ".bin";
        opts.output = ss.str();
    }

    if (opts.output == "-") {
        verbose("saving binary to stdout\n");
        writeBinary("", binPtrs[0], binSizes[0]);
    } else {
        verbose("saving binary to %s\n", opts.output.c_str());
        writeBinary(opts.output, binPtrs[0], binSizes[0]);
    }
    return 0;
}

