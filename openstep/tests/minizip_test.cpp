/* Real ZIP stream adapters and native checked file offsets. */
#include "dosbox.h"
#include "zipcppstdbuf.h"
#define MAXU32 0xffffffffU
#include "vs/zlib/contrib/minizip/zip.c"
#include "vs/zlib/contrib/minizip/unzip.c"
#include "vs/zlib/contrib/minizip/ioapi.c"
#include <cassert>
#include <sstream>

int main()
{
    const char *path = "openstep/output/minizip-fixture.zip";
    std::string content;
    for (unsigned i = 0; i < 10000; ++i) content += "DOSBox-X native ZIP\n";
    zipFile writer = zipOpen64(path, APPEND_STATUS_CREATE);
    assert(writer);
    assert(zipOpenNewFileInZip64(writer, "test.txt", NULL, NULL, 0, NULL, 0,
                                NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION, 1) == ZIP_OK);
    {
        zip_ostreambuf buffer(writer);
        std::ostream out(&buffer);
        out.write(content.data(), content.size());
        assert(out.good());
    }
    assert(zipClose(writer, NULL) == ZIP_OK);
    unzFile reader = unzOpen64(path);
    assert(reader && unzLocateFile(reader, "test.txt", 1) == UNZ_OK);
    assert(unzOpenCurrentFile(reader) == UNZ_OK);
    {
        zip_istreambuf buffer(reader);
        std::istream in(&buffer);
        std::string restored(content.size(), '\0');
        in.read(&restored[0], restored.size());
        assert(in.gcount() == (std::streamsize)content.size() && restored == content);
    }
    assert(unzClose(reader) == UNZ_OK);

    zlib_filefunc64_def file;
    fill_fopen64_filefunc(&file);
    voidpf stream = file.zopen64_file(file.opaque, path, ZLIB_FILEFUNC_MODE_READ);
    assert(stream);
    assert(file.zseek64_file(file.opaque, stream, 5, ZLIB_FILEFUNC_SEEK_SET) == 0);
    assert(file.ztell64_file(file.opaque, stream) == 5);
    assert(file.zseek64_file(file.opaque, stream, UINT64_C(0x100000000), ZLIB_FILEFUNC_SEEK_SET) != 0);
    assert(file.ztell64_file(file.opaque, stream) == 5);
    assert(file.zseek64_file(file.opaque, stream, UINT64_C(0x80000000), ZLIB_FILEFUNC_SEEK_SET) != 0);
    assert(file.ztell64_file(file.opaque, stream) == 5);
    assert(file.zclose_file(file.opaque, stream) == 0);
    assert(remove(path) == 0);
    puts("OPENSTEP ZIP stream and offset tests passed");
    return 0;
}
