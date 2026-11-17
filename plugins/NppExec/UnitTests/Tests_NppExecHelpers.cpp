// Qt port: the headers through the include path, as the folders are laid out differently here
#include "NppExecHelpers.h"
#include <cassert>

namespace
{
    void test_GetFileNamePart()
    {
        using namespace NppExecHelpers;

#ifdef NPPEXEC_QT_PORT
        // Qt port: the paths of Linux, which start at "/" and have no drives
        const tstr empty = tstr();

        tstr path = _T("f.n");
        assert( GetFileNamePart(path, fnpDrive)   == empty );
        assert( GetFileNamePart(path, fnpDirPath) == empty );
        assert( GetFileNamePart(path, fnpNameExt) == _T("f.n") );
        assert( GetFileNamePart(path, fnpName)    == _T("f") );
        assert( GetFileNamePart(path, fnpExt)     == _T(".n") );

        path = _T("/");
        assert( GetFileNamePart(path, fnpDrive)   == empty );
        assert( GetFileNamePart(path, fnpDirPath) == _T("/") );
        assert( GetFileNamePart(path, fnpNameExt) == empty );
        assert( GetFileNamePart(path, fnpName)    == empty );
        assert( GetFileNamePart(path, fnpExt)     == empty );

        path = _T("/d/f.n");
        assert( GetFileNamePart(path, fnpDrive)   == empty );
        assert( GetFileNamePart(path, fnpDirPath) == _T("/d/") );
        assert( GetFileNamePart(path, fnpNameExt) == _T("f.n") );
        assert( GetFileNamePart(path, fnpName)    == _T("f") );
        assert( GetFileNamePart(path, fnpExt)     == _T(".n") );

        path = _T("/d.e/f");
        assert( GetFileNamePart(path, fnpDirPath) == _T("/d.e/") );
        assert( GetFileNamePart(path, fnpNameExt) == _T("f") );
        assert( GetFileNamePart(path, fnpName)    == _T("f") );
        assert( GetFileNamePart(path, fnpExt)     == empty );

        path = _T("/d.e/.n");
        assert( GetFileNamePart(path, fnpDirPath) == _T("/d.e/") );
        assert( GetFileNamePart(path, fnpNameExt) == _T(".n") );
        assert( GetFileNamePart(path, fnpExt)     == _T(".n") );

        // A drive of Windows is a part of a name here
        path = _T("C:");
        assert( GetFileNamePart(path, fnpDrive)   == empty );
        assert( GetFileNamePart(path, fnpNameExt) == _T("C:") );
#else

        const tstr empty = tstr();

        tstr path = _T("C");
        assert( GetFileNamePart(path, fnpDrive)   == empty );
        assert( GetFileNamePart(path, fnpDirPath) == empty );
        assert( GetFileNamePart(path, fnpNameExt) == _T("C") );
        assert( GetFileNamePart(path, fnpName)    == _T("C") );
        assert( GetFileNamePart(path, fnpExt)     == empty );

        path = _T("C:");
        assert( GetFileNamePart(path, fnpDrive)   == _T("C") );
        assert( GetFileNamePart(path, fnpDirPath) == _T("C:") );
        assert( GetFileNamePart(path, fnpNameExt) == empty );
        assert( GetFileNamePart(path, fnpName)    == empty );
        assert( GetFileNamePart(path, fnpExt)     == empty );

        path = _T("C:\\");
        assert( GetFileNamePart(path, fnpDrive)   == _T("C") );
        assert( GetFileNamePart(path, fnpDirPath) == _T("C:\\") );
        assert( GetFileNamePart(path, fnpNameExt) == empty );
        assert( GetFileNamePart(path, fnpName)    == empty );
        assert( GetFileNamePart(path, fnpExt)     == empty );

        path = _T("C:/");
        assert( GetFileNamePart(path, fnpDrive)   == _T("C") );
        assert( GetFileNamePart(path, fnpDirPath) == _T("C:/") );
        assert( GetFileNamePart(path, fnpNameExt) == empty );
        assert( GetFileNamePart(path, fnpName)    == empty );
        assert( GetFileNamePart(path, fnpExt)     == empty );

        path = _T(".C:");
        assert( GetFileNamePart(path, fnpDrive)   == empty );
        assert( GetFileNamePart(path, fnpDirPath) == empty );
        assert( GetFileNamePart(path, fnpNameExt) == _T(".C:") );
        assert( GetFileNamePart(path, fnpName)    == _T(".C:") );
        assert( GetFileNamePart(path, fnpExt)     == _T(".C:") );

        path = _T("\\\\");
        assert( GetFileNamePart(path, fnpDrive)   == empty );
        assert( GetFileNamePart(path, fnpDirPath) == _T("\\\\") );
        assert( GetFileNamePart(path, fnpNameExt) == empty );
        assert( GetFileNamePart(path, fnpName)    == empty );
        assert( GetFileNamePart(path, fnpExt)     == empty );

        path = _T("\\\\f.n");
        assert( GetFileNamePart(path, fnpDrive)   == empty );
        assert( GetFileNamePart(path, fnpDirPath) == _T("\\\\") );
        assert( GetFileNamePart(path, fnpNameExt) == _T("f.n") );
        assert( GetFileNamePart(path, fnpName)    == _T("f") );
        assert( GetFileNamePart(path, fnpExt)     == _T(".n") );

        path = _T("C:\\.n");
        assert( GetFileNamePart(path, fnpDrive)   == _T("C") );
        assert( GetFileNamePart(path, fnpDirPath) == _T("C:\\") );
        assert( GetFileNamePart(path, fnpNameExt) == _T(".n") );
        assert( GetFileNamePart(path, fnpName)    == _T(".n") );
        assert( GetFileNamePart(path, fnpExt)     == _T(".n") );

        path = _T("C:\\d\\f.n");
        assert( GetFileNamePart(path, fnpDrive)   == _T("C") );
        assert( GetFileNamePart(path, fnpDirPath) == _T("C:\\d\\") );
        assert( GetFileNamePart(path, fnpNameExt) == _T("f.n") );
        assert( GetFileNamePart(path, fnpName)    == _T("f") );
        assert( GetFileNamePart(path, fnpExt)     == _T(".n") );
        
        path = _T("C:\\d.e\\f");
        assert( GetFileNamePart(path, fnpDrive)   == _T("C") );
        assert( GetFileNamePart(path, fnpDirPath) == _T("C:\\d.e\\") );
        assert( GetFileNamePart(path, fnpNameExt) == _T("f") );
        assert( GetFileNamePart(path, fnpName)    == _T("f") );
        assert( GetFileNamePart(path, fnpExt)     == empty );

        path = _T("C:\\d.e\\.n");
        assert( GetFileNamePart(path, fnpDrive)   == _T("C") );
        assert( GetFileNamePart(path, fnpDirPath) == _T("C:\\d.e\\") );
        assert( GetFileNamePart(path, fnpNameExt) == _T(".n") );
        assert( GetFileNamePart(path, fnpName)    == _T(".n") );
        assert( GetFileNamePart(path, fnpExt)     == _T(".n") );

        path = _T("f.n");
        assert( GetFileNamePart(path, fnpDrive)   == empty );
        assert( GetFileNamePart(path, fnpDirPath) == empty );
        assert( GetFileNamePart(path, fnpNameExt) == _T("f.n") );
        assert( GetFileNamePart(path, fnpName)    == _T("f") );
        assert( GetFileNamePart(path, fnpExt)     == _T(".n") );
#endif
    }

    void test_IsFullLocalPath()
    {
        using namespace NppExecHelpers;

#ifdef NPPEXEC_QT_PORT
        // Qt port: a full path starts at "/"
        assert( IsFullLocalPath(_T("")) == false );
        assert( IsFullLocalPath(_T("/")) == true );
        assert( IsFullLocalPath(_T("/abc")) == true );
        assert( IsFullLocalPath(_T("//abc")) == true );
        assert( IsFullLocalPath(_T("abc")) == false );
        assert( IsFullLocalPath(_T("./abc")) == false );
        assert( IsFullLocalPath(_T("X:")) == false );
        assert( IsFullLocalPath(_T("X:\\abc")) == false );
#else

        assert( IsFullLocalPath(_T("")) == false );
        assert( IsFullLocalPath(_T("\\")) == false );
        assert( IsFullLocalPath(_T("/")) == false );
        assert( IsFullLocalPath(_T("\\.")) == false );
        assert( IsFullLocalPath(_T("/.")) == false );
        assert( IsFullLocalPath(_T("\\\\")) == false );
        assert( IsFullLocalPath(_T("//")) == false );
        assert( IsFullLocalPath(_T("\\\\X")) == false );
        assert( IsFullLocalPath(_T("//X")) == false );
        assert( IsFullLocalPath(_T("\\\\X:")) == false );
        assert( IsFullLocalPath(_T("//X:")) == false );
        assert( IsFullLocalPath(_T("\\\\\\")) == false );
        assert( IsFullLocalPath(_T("///")) == false );
        assert( IsFullLocalPath(_T("\\\\.")) == false );
        assert( IsFullLocalPath(_T("//.")) == false );
        assert( IsFullLocalPath(_T("\\\\.\\")) == false );
        assert( IsFullLocalPath(_T("//./")) == false );
        assert( IsFullLocalPath(_T("\\\\.\\X")) == false );
        assert( IsFullLocalPath(_T("//./X")) == false );
        assert( IsFullLocalPath(_T("\\\\.\\X:")) == true );
        assert( IsFullLocalPath(_T("//./X:")) == true );
        assert( IsFullLocalPath(_T("\\\\.\\X:\\")) == true );
        assert( IsFullLocalPath(_T("//./X:/")) == true );
        assert( IsFullLocalPath(_T("\\\\.\\X:/")) == true );
        assert( IsFullLocalPath(_T("//./X:\\")) == true );
        assert( IsFullLocalPath(_T("\\\\.\\X:\\abc")) == true );
        assert( IsFullLocalPath(_T("//./X:/abc")) == true );
        assert( IsFullLocalPath(_T("\\\\.\\X:/abc")) == true );
        assert( IsFullLocalPath(_T("//./X:\\abc")) == true );
        assert( IsFullLocalPath(_T("\\:")) == false );
        assert( IsFullLocalPath(_T("/:")) == false );
        assert( IsFullLocalPath(_T("\\:\\")) == false );
        assert( IsFullLocalPath(_T("/:/")) == false );
        assert( IsFullLocalPath(_T("X:")) == true );
        assert( IsFullLocalPath(_T("X:\\")) == true );
        assert( IsFullLocalPath(_T("X:/")) == true );
        assert( IsFullLocalPath(_T("X:\\abc")) == true );
        assert( IsFullLocalPath(_T("X:/abc")) == true );
        assert( IsFullLocalPath(_T("abc")) == false );
        assert( IsFullLocalPath(_T("a:bc")) == false );
        assert( IsFullLocalPath(_T("\abc")) == false );
        assert( IsFullLocalPath(_T("/abc")) == false );
        assert( IsFullLocalPath(_T("\\abc")) == false );
        assert( IsFullLocalPath(_T("//abc")) == false );
#endif
    }
}

void tests_NppExecHelpers()
{
    test_GetFileNamePart();
    test_IsFullLocalPath();
}
