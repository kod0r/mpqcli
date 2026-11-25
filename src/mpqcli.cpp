#include "StormLib.h"

#include <cstdio>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>

static void usage()
{
    std::fprintf(stderr,
        "mpqcli - StormLib based MPQ utility\n"
        "usage:\n"
        "  mpqcli list   <archive>\n"
        "  mpqcli has    <archive> <file>\n"
        "  mpqcli get    <archive> <file> <out>\n"
        "  mpqcli add    <archive> <local> <archived-name>\n"
        "  mpqcli remove <archive> <file>\n"
        "  mpqcli create <archive> <max-files> [v1|v2|v3|v4]\n"
        "  mpqcli verify <archive>\n");
}

static bool openArchive(const char* path, HANDLE& h, bool readOnly)
{
    DWORD flags = readOnly ? MPQ_OPEN_READ_ONLY : 0;
    if (!SFileOpenArchive(path, 0, flags, &h))
    {
        std::fprintf(stderr, "open %s failed: error %lu\n", path, (unsigned long)SErrGetLastError());
        return false;
    }
    return true;
}

static int doList(const char* path)
{
    HANDLE h = nullptr;
    if (!openArchive(path, h, true)) return 1;
    SFILE_FIND_DATA fd;
    HANDLE find = SFileFindFirstFile(h, "*", &fd, nullptr);
    if (find)
    {
        do
        {
            std::printf("%s\n", fd.cFileName);
        } while (SFileFindNextFile(find, &fd));
        SFileFindClose(find);
    }
    SFileCloseArchive(h);
    return 0;
}

static int doHas(const char* path, const char* file)
{
    HANDLE h = nullptr;
    if (!openArchive(path, h, true)) return 1;
    bool ok = SFileHasFile(h, file);
    std::printf("%s\n", ok ? "yes" : "no");
    SFileCloseArchive(h);
    return ok ? 0 : 2;
}

static int doGet(const char* path, const char* file, const char* out)
{
    HANDLE h = nullptr;
    if (!openArchive(path, h, true)) return 1;
    bool ok = SFileExtractFile(h, file, out, SFILE_OPEN_FROM_MPQ);
    if (!ok) std::fprintf(stderr, "extract %s failed: error %lu\n", file, (unsigned long)SErrGetLastError());
    SFileCloseArchive(h);
    return ok ? 0 : 2;
}

static bool ieq(const char* a, const char* b)
{
    for (; *a && *b; ++a, ++b)
    {
        int ca = std::tolower((unsigned char)*a), cb = std::tolower((unsigned char)*b);
        if (ca != cb) return false;
    }
    return *a == *b;
}

static int doAdd(const char* path, const char* local, const char* archived)
{
    // StormLib rejects pseudo-names ("File########.ext") and internal names
    // ("(listfile)", "(attributes)", "(signature)") with ERROR_INVALID_PARAMETER (22).
    // Detect them up front so the user gets a clear reason, not a bare error code.
    {
        bool pseudo = (archived[0]=='F'||archived[0]=='f') && (archived[1]=='i'||archived[1]=='I') &&
                      (archived[2]=='l'||archived[2]=='L') && (archived[3]=='e'||archived[3]=='E');
        for (int i = 4; pseudo && i < 12; ++i)
            if (archived[i] < '0' || archived[i] > '9') pseudo = false;
        if (pseudo && archived[12] == '.')
        {
            std::fprintf(stderr, "add %s rejected: pseudo-name (File########.ext) not allowed; use a real asset path\n", archived);
            return 2;
        }
        if (archived[0] == '(' &&
            (ieq(archived, "(listfile)") || ieq(archived, "(attributes)") || ieq(archived, "(signature)")))
        {
            std::fprintf(stderr, "add %s rejected: internal MPQ name not allowed\n", archived);
            return 2;
        }
    }
    HANDLE h = nullptr;
    if (!openArchive(path, h, false)) return 1;
    DWORD flags = MPQ_FILE_COMPRESS | MPQ_FILE_REPLACEEXISTING;
    bool ok = SFileAddFileEx(h, local, archived, flags, MPQ_COMPRESSION_ZLIB, MPQ_COMPRESSION_NEXT_SAME);
    if (!ok) std::fprintf(stderr, "add %s failed: error %lu\n", archived, (unsigned long)SErrGetLastError());
    SFileCloseArchive(h);
    return ok ? 0 : 2;
}

static int doRemove(const char* path, const char* file)
{
    HANDLE h = nullptr;
    if (!openArchive(path, h, false)) return 1;
    bool ok = SFileRemoveFile(h, file, SFILE_OPEN_FROM_MPQ);
    if (!ok) std::fprintf(stderr, "remove %s failed: error %lu\n", file, (unsigned long)SErrGetLastError());
    SFileCloseArchive(h);
    return ok ? 0 : 2;
}

static int doCreate(const char* path, const char* maxFilesArg, const char* version)
{
    DWORD maxFiles = 0x1000;
    if (maxFilesArg) maxFiles = (DWORD)std::strtoul(maxFilesArg, nullptr, 0);
    DWORD createFlags = MPQ_CREATE_ARCHIVE_V1;
    if (version)
    {
        if (!std::strcmp(version, "v2")) createFlags = MPQ_CREATE_ARCHIVE_V2;
        else if (!std::strcmp(version, "v3")) createFlags = MPQ_CREATE_ARCHIVE_V3;
        else if (!std::strcmp(version, "v4")) createFlags = MPQ_CREATE_ARCHIVE_V4;
        else if (std::strcmp(version, "v1")) { usage(); return 1; }
    }
    HANDLE h = nullptr;
    if (!SFileCreateArchive(path, createFlags, maxFiles, &h))
    {
        std::fprintf(stderr, "create %s failed: error %lu\n", path, (unsigned long)SErrGetLastError());
        return 1;
    }
    SFileCloseArchive(h);
    return 0;
}

static int doVerify(const char* path)
{
    HANDLE h = nullptr;
    if (!openArchive(path, h, true)) return 1;
    SFILE_FIND_DATA fd;
    HANDLE find = SFileFindFirstFile(h, "*", &fd, nullptr);
    int bad = 0;
    if (find)
    {
        do
        {
            HANDLE f = nullptr;
            if (!SFileOpenFileEx(h, fd.cFileName, SFILE_OPEN_FROM_MPQ, &f))
            {
                std::fprintf(stderr, "BAD %s (error %lu)\n", fd.cFileName, (unsigned long)SErrGetLastError());
                ++bad;
            }
            else
            {
                char buf[65536];
                DWORD read = 0;
                while (SFileReadFile(f, buf, sizeof(buf), &read, nullptr) && read) {}
                SFileCloseFile(f);
            }
        } while (SFileFindNextFile(find, &fd));
        SFileFindClose(find);
    }
    SFileCloseArchive(h);
    std::printf("verified %s: %d bad files\n", path, bad);
    return bad ? 2 : 0;
}

int main(int argc, char** argv)
{
    if (argc < 3) { usage(); return 1; }
    const char* cmd = argv[1];
    if (!std::strcmp(cmd, "list")) return doList(argv[2]);
    if (!std::strcmp(cmd, "has")) return argc >= 4 ? doHas(argv[2], argv[3]) : (usage(), 1);
    if (!std::strcmp(cmd, "get")) return argc >= 5 ? doGet(argv[2], argv[3], argv[4]) : (usage(), 1);
    if (!std::strcmp(cmd, "add")) return argc >= 5 ? doAdd(argv[2], argv[3], argv[4]) : (usage(), 1);
    if (!std::strcmp(cmd, "remove")) return argc >= 4 ? doRemove(argv[2], argv[3]) : (usage(), 1);
    if (!std::strcmp(cmd, "create")) return doCreate(argv[2], argc >= 4 ? argv[3] : nullptr, argc >= 5 ? argv[4] : nullptr);
    if (!std::strcmp(cmd, "verify")) return doVerify(argv[2]);
    usage();
    return 1;
}
