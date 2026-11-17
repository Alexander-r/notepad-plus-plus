#include "NppExecHelpers.h"
#include "encodings/SysUniConv.h"
#include <shlwapi.h>

// Qt port: files, the clipboard and the environment through Qt and POSIX
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <cstdio>

namespace
{
    // Compares two strings case-insensitively.
    // Returns 0 when equal; 1 when S1 > S2; -1 when S1 < S2.
    inline int strCompareNoCase(const TCHAR* s1, int len1, const TCHAR* s2, int len2) noexcept
    {
        // Qt port: in place of CompareString with NORM_IGNORECASE
        const QString q1 = QString::fromWCharArray(s1, len1 < 0 ? static_cast<int>(wcslen(s1)) : len1);
        const QString q2 = QString::fromWCharArray(s2, len2 < 0 ? static_cast<int>(wcslen(s2)) : len2);
        const int ret = q1.compare(q2, Qt::CaseInsensitive);
        return (ret < 0) ? -1 : ((ret > 0) ? 1 : 0);
    }

    inline bool isPathSep(const TCHAR ch) noexcept
    {
        return (ch == _T('\\') || ch == _T('/'));
    }

    bool isPathRoot(const tstr& part) noexcept
    {
        // Qt port: returns true for "/", the only root here
        return ( StrUnsafeCmp(part.c_str(), _T("/")) == 0 );
    };

    inline bool isNullOrPathSep(const TCHAR ch) noexcept
    {
        return (ch == 0 || ch == _T('\\') || ch == _T('/'));
    }

    inline int findPathSep(const tstr& path, int nStartPos = 0) noexcept
    {
        return path.FindOneOf(_T("\\/"), nStartPos);
    }

    inline int rfindPathSep(const tstr& path, int nStartPos = -1) noexcept
    {
        return path.RFindOneOf(_T("\\/"), nStartPos);
    }

    bool impl_createDirectoryTree(const tstr& dir) noexcept
    {
        // Qt port: in place of CreateDirectory for each part of the path
        return QDir().mkpath(QString::fromWCharArray(dir.c_str(), dir.length()));
    }
}

namespace NppExecHelpers
{
    bool CreateNewThread(LPTHREAD_START_ROUTINE lpFunc, LPVOID lpParam, HANDLE* lphThread /* = NULL */) noexcept
    {
        DWORD  dwThreadID;
        HANDLE hThread;

        hThread = ::CreateThread(
            NULL,
            0,
            lpFunc,
            lpParam,
            CREATE_SUSPENDED,
            &dwThreadID);

        if ( hThread == NULL )
            return false;

        ::ResumeThread(hThread);
        if ( !lphThread )
        {
            ::CloseHandle(hThread);
        }
        else
        {
            *lphThread = hThread;
        }
        return true;
    }

    HWND GetFocusedWnd() noexcept
    {
        // Qt port: the application's focus widget, from the GUI thread
        HWND hFocusedWnd = NULL;
        nppExecOnGuiThread( [&hFocusedWnd]() { hFocusedWnd = QApplication::focusWidget(); } );
        return hFocusedWnd;
    }

    tstr GetClipboardText(HWND /*hWndOwner*/ )
    {
        // Qt port: Qt's clipboard, from the GUI thread
        tstr sText;
        nppExecOnGuiThread( [&sText]() { sText = QApplication::clipboard()->text().toStdWString().c_str(); } );
        return sText;
    }

    bool SetClipboardText(const tstr& text, HWND /*hWndOwner*/ )
    {
        // Qt port: Qt's clipboard, from the GUI thread
        nppExecOnGuiThread( [&text]() { QApplication::clipboard()->setText(QString::fromWCharArray(text.c_str(), text.length())); } );
        return true;
    }

    tstr GetEnvironmentVar(const TCHAR* szVarName)
    {
        // Qt port: in place of GetEnvironmentVariable
        const QByteArray value = qgetenv(QString::fromWCharArray(szVarName).toLocal8Bit().constData());
        return tstr(QString::fromLocal8Bit(value).toStdWString().c_str());
    }

    tstr GetEnvironmentVar(const tstr& sVarName)
    {
        return GetEnvironmentVar(sVarName.c_str());
    }

    tstr GetCurrentDir()
    {
        // Qt port: in place of GetCurrentDirectory
        return tstr(QDir::currentPath().toStdWString().c_str());
    }

    bool IsFullPath(const tstr& path) noexcept
    {
        return IsFullPath( path.c_str() );
    }

    bool IsFullPath(const TCHAR* path) noexcept
    {
        // Qt port: a full path starts at the root, "/"
        return (path[0] == _T('/'));
    }

    bool IsFullLocalPath(const tstr& path) noexcept
    {
        return IsFullLocalPath( path.c_str() );
    }

    bool IsFullLocalPath(const TCHAR* path) noexcept
    {
        // Qt port: every full path is local here, without the drives and network paths of Windows
        return IsFullPath(path);
    }

    tstr NormalizePath(const tstr& path)
    {
        return NormalizePath(path.c_str());
    }

    tstr NormalizePath(const TCHAR* path)
    {
        auto remove_trailing_sep = [](CListT<tstr>& pathParts)
        {
            auto itr = pathParts.GetLast();
            if ( itr )
            {
                tstr& path_part = itr->GetItem();
                if ( !isPathRoot(path_part) )
                    path_part.DeleteLastChar(); // remove the trailing '\'
            }
        };

        bool isEndOfString = false;
        tstr part;
        CListT<tstr> pathParts;

        for ( const TCHAR* p = path; !isEndOfString; ++p )
        {
            const TCHAR ch = *p;
            if ( ch == 0 )
                isEndOfString = true;

            if ( isEndOfString || isPathSep(ch) )
            {
                if ( part.IsEmpty() )
                {
                    if ( (p == path) && pathParts.IsEmpty() && !isEndOfString )
                    {
                        pathParts.Add( _T("/") ); // Qt port: the root, without the network paths of Windows
                    }
                    continue;
                }

                if ( StrUnsafeCmp(part.c_str(), _T(".")) == 0 )
                {
                    if ( isEndOfString ) // ends with "."
                    {
                        remove_trailing_sep(pathParts);
                    }
                    part.Clear();
                    continue;
                }

                if ( StrUnsafeCmp(part.c_str(), _T("..")) == 0 )
                {
                    auto itr = pathParts.GetLast();
                    if ( itr )
                    {
                        if ( !isPathRoot(itr->GetItem()) )
                        {
                            pathParts.DeleteLast();
                            if ( isEndOfString ) // ends with ".."
                            {
                                remove_trailing_sep(pathParts);
                            }
                        }
                    }
                    part.Clear();
                    continue;
                }

                if ( isPathSep(ch) )
                    part += _T('/'); // Qt port: the separator of Linux

                pathParts.Add(part);
                part.Clear();
            }
            else
                part += ch;
        }

        int expectedLength = 0;
        for ( auto itr = pathParts.GetFirst(); itr != NULL; itr = itr->GetNext() )
        {
            expectedLength += itr->GetItem().length();
        }

        tstr nrmPath;
        nrmPath.Reserve(expectedLength);

        for ( auto itr = pathParts.GetFirst(); itr != NULL; itr = itr->GetNext() )
        {
            nrmPath += itr->GetItem();
        }

        return nrmPath;
    }

    tstr GetFileNamePart(const TCHAR* path, eFileNamePart whichPart)
    {
        const TCHAR* p1;
        const TCHAR* p2;
        int n;

        switch ( whichPart )
        {
            case fnpDrive:   // "C" in "C:\User\Docs\name.ext"
                // Qt port: there are no drives
                (void) n;
                break;

            case fnpDirPath:    // "C:\User\Docs\" in "C:\User\Docs\name.ext"
                p1 = path;
                p2 = ::PathFindFileName(p1);
                if ( p2 > p1 )
                {
                    return tstr(p1, static_cast<int>(p2 - p1));
                }
                else if ( ::PathIsRoot(p1) || ::PathIsDirectory(p1) )
                {
                    return tstr(p1);
                }
                break;

            case fnpNameExt: // "name.ext" in "C:\User\Docs\name.ext"
                p1 = ::PathFindFileName(path);
                if ( !::PathIsRoot(p1) && !::PathIsDirectory(p1) )
                {
                    return tstr(p1);
                }
                break;

            case fnpName:    // "name" in "C:\User\Docs\name.ext"
                p1 = ::PathFindFileName(path);
                if ( !::PathIsRoot(p1) && !::PathIsDirectory(p1) )
                {
                    p2 = ::PathFindExtension(path);
                    return ((p2 > p1) ? tstr(p1, static_cast<int>(p2 - p1)) : tstr(p1));
                }
                break;

            case fnpExt:     // ".ext" in "C:\User\Docs\name.ext"
                return tstr(::PathFindExtension(path));
        }

        return tstr(); // empty
    }

    tstr GetFileNamePart(const tstr& path, eFileNamePart whichPart)
    {
        return GetFileNamePart(path.c_str(), whichPart);
    }

    bool CreateDirectoryTree(const tstr& dir) noexcept
    {
        return impl_createDirectoryTree(dir);
    }

    bool CreateDirectoryTree(const TCHAR* dir) noexcept
    {
        return impl_createDirectoryTree(dir);
    }

    bool CheckDirectoryExists(const tstr& dir) noexcept
    {
        return CheckDirectoryExists(dir.c_str());
    }

    bool CheckDirectoryExists(const TCHAR* dir) noexcept
    {
        // Qt port: in place of GetFileAttributes
        return QFileInfo(QString::fromWCharArray(dir)).isDir();
    }

    bool CheckFileExists(const tstr& filename) noexcept
    {
        return CheckFileExists(filename.c_str());
    }

    bool CheckFileExists(const TCHAR* filename) noexcept
    {
        // Qt port: in place of GetFileAttributes
        const QFileInfo info(QString::fromWCharArray(filename));
        return (info.exists() && !info.isDir());
    }

    bool IsValidTextFile(const tstr& filename) noexcept
    {
        return IsValidTextFile(filename.c_str());
    }

    bool IsValidTextFile(const TCHAR* filename) noexcept
    {
        bool bResult = false;
        // Qt port: with stdio in place of CreateFile and ReadFile
        FILE* f = std::fopen(QString::fromWCharArray(filename).toLocal8Bit().constData(), "rb");
        
        if ( f != NULL )
        {
            const DWORD dwBytesToVerify = 16;
            BYTE buf[dwBytesToVerify];
            const DWORD dwBytesRead = static_cast<DWORD>(std::fread(buf, 1, dwBytesToVerify, f));
            {
                if ( dwBytesRead == dwBytesToVerify )
                {
                    DWORD dwZeroBytes = 0;
                    DWORD dwContinuousZeroBytes = 0;
                    DWORD dwMaxContinuousZeroBytes = 0;
                    for ( DWORD i = 0; i < dwBytesToVerify; ++i )
                    {
                        if ( buf[i] == 0x00 )
                        {
                            ++dwZeroBytes;
                            ++dwContinuousZeroBytes;
                        }
                        else
                        {
                            if ( dwMaxContinuousZeroBytes < dwContinuousZeroBytes )
                                dwMaxContinuousZeroBytes = dwContinuousZeroBytes;

                            dwContinuousZeroBytes = 0;
                        }
                    }
                    if ( dwMaxContinuousZeroBytes < dwContinuousZeroBytes )
                        dwMaxContinuousZeroBytes = dwContinuousZeroBytes;

                    if ( (dwMaxContinuousZeroBytes <= 2) && (dwZeroBytes*2 <= dwBytesToVerify) )
                        bResult = true;
                }
            }

            std::fclose(f);
        }

        return bResult;
    }

    bool GetFileWriteTime(const TCHAR* filename, FILETIME* pLastWriteTime) noexcept
    {
        // Qt port: the modification time from stat, as a FILETIME
        return nppExecGetFileWriteTime(filename, pLastWriteTime);
    }

    bool SetFileWriteTime(const TCHAR* filename, const FILETIME* pLastWriteTime) noexcept
    {
        // Qt port: the modification time through utimensat
        return nppExecSetFileWriteTime(filename, pLastWriteTime);
    }

    tstr GetInstanceAsString(const void* pInstance)
    {
        tstr S;
        SYSTEMTIME t;

        ::GetLocalTime(&t);
        S.Format(60, _T("0x%X @ %02d:%02d:%02d.%03d"), pInstance, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
        return S;
    }

    void StrLower(tstr& S) noexcept
    {
        if ( S.length() != 0 )
            ::CharLower( S.data() );
    }

    void StrLower(TCHAR* S) noexcept
    {
        ::CharLower( S );
    }

    void StrUpper(tstr& S) noexcept
    {
        if ( S.length() != 0 )
            ::CharUpper( S.data() );
    }

    void StrUpper(TCHAR* S) noexcept
    {
        ::CharUpper( S );
    }

    TCHAR LatinCharUpper(TCHAR ch) noexcept
    {
        if ( ch >= _T('a') && ch <= _T('z') )
        {
            ch -= _T('a');
            ch += _T('A');
        }
        return ch;
    }

    TCHAR LatinCharLower(TCHAR ch) noexcept
    {
        if ( ch >= _T('A') && ch <= _T('Z') )
        {
            ch -= _T('A');
            ch += _T('a');
        }
        return ch;
    }

    void StrQuote(tstr& S)
    {
        if ( IsStrNotQuoted(S) )
        {
            S.Append( _T('\"') );
            S.Insert( 0, _T('\"') );
        }
    }

    void StrUnquote(tstr& S)
    {
        if ( IsStrQuoted(S) )
        {
            S.DeleteLastChar();
            S.DeleteFirstChar();
        }
    }

    void StrUnquoteEx(tstr& S)
    {
        if ( IsStrQuotedEx(S) )
        {
            S.DeleteLastChar();
            S.DeleteFirstChar();
        }
    }

    bool IsStrQuoted(const tstr& S) noexcept
    {
        return ( (S.length() > 1) && (S.GetFirstChar() == _T('\"')) && (S.GetLastChar() == _T('\"')) );
    }

    bool IsStrQuotedEx(const tstr& S) noexcept
    {
        if ( S.length() > 1 )
        {
            const tstr::value_type firstChar = S.GetFirstChar();
            if ( firstChar == _T('\"') || firstChar == _T('\'') || firstChar == _T('`') )
                return ( firstChar == S.GetLastChar() );
        }
        return false;
    }

    bool IsStrNotQuoted(const tstr& S) noexcept
    {
        return ( S.GetFirstChar() != _T('\"') && S.GetLastChar() != _T('\"') );
    }

    bool IsStrNotQuotedEx(const tstr& S) noexcept
    {
        const tstr::value_type firstChar = S.GetFirstChar();
        const tstr::value_type lastChar = S.GetLastChar();
        return ( firstChar != _T('\"') && lastChar != _T('\"') &&
                 firstChar != _T('\'') && lastChar != _T('\'') &&
                 firstChar != _T('`')  && lastChar != _T('`') );
    }

    void StrEscape(tstr& S)
    {
        tstr R;
        R.Reserve(2 * S.length());

        const TCHAR* p = S.c_str();
        TCHAR ch;

        while ( (ch = *p) != 0 )
        {
            switch ( ch )
            {
                case _T('\n'):
                    R += _T('\\');
                    R += _T('n');
                    break;
                case _T('\r'):
                    R += _T('\\');
                    R += _T('r');
                    break;
                case _T('\t'):
                    R += _T('\\');
                    R += _T('t');
                    break;
                case _T('\\'):
                    R += _T('\\');
                    R += _T('\\');
                    break;
                case _T('"'):
                    R += _T('\\');
                    R += _T('"');
                    break;
                default:
                    R += ch;
                    break;
            }

            ++p;
        }

        S.Swap(R);
    }

    void StrUnescape(tstr& S)
    {
        if ( S.Find(_T('\\')) >= 0 )
        {
            tstr R;
            R.Reserve(S.length());

            const TCHAR* p = S.c_str();
            TCHAR ch;

            while ( (ch = *p) != 0 )
            {
                if ( ch == _T('\\') )
                {
                    switch ( *(p + 1) )
                    {
                        case _T('n'):
                            R += _T('\n');
                            ++p;
                            break;
                        case _T('r'):
                            R += _T('\r');
                            ++p;
                            break;
                        case _T('t'):
                            R += _T('\t');
                            ++p;
                            break;
                        case _T('\\'):
                            R += _T('\\');
                            ++p;
                            break;
                        case 0:
                            // trailing '\'
                            R += _T('\\');
                            break;
                        default:
                            // e.g. '\q' becomes 'q'
                            break;
                    }
                }
                else
                {
                    R += ch;
                }

                ++p;
            }

            S.Swap(R);
        }
    }

    int StrCmpNoCase(const tstr& S1, const tstr& S2) noexcept
    {
        return strCompareNoCase( S1.c_str(), S1.length(), 
                                 S2.c_str(), S2.length() );
    }

    int StrCmpNoCase(const tstr& S1, const TCHAR* S2) noexcept
    {
        return strCompareNoCase( S1.c_str(), S1.length(), 
                                 S2 ? S2 : _T(""), GetStrSafeLength(S2) );
    }

    int StrCmpNoCase(const TCHAR* S1, const tstr& S2) noexcept
    {
        return strCompareNoCase( S1 ? S1 : _T(""), GetStrSafeLength(S1), 
                                 S2.c_str(), S2.length() );
    }

    int StrCmpNoCase(const TCHAR* S1, const TCHAR* S2) noexcept
    {
        return strCompareNoCase( S1 ? S1 : _T(""), GetStrSafeLength(S1), 
                                 S2 ? S2 : _T(""), GetStrSafeLength(S2) );
    }

    int StrCmpNoCase(const TCHAR* S1, int Len1, const TCHAR* S2, int Len2) noexcept
    {
        if ( !S1 )
        {
            S1 = _T("");
            Len1 = 0;
        }
        if ( !S2 )
        {
            S2 = _T("");
            Len2 = 0;
        }
        return strCompareNoCase(S1, Len1, S2, Len2);
    }

    template<typename T> struct is_wtsp {};

    template<> struct is_wtsp<char>
    {
        bool operator()(char ch) { return IsWhiteSpaceChar(ch); }
    };

    template<> struct is_wtsp<wchar_t>
    {
        bool operator()(wchar_t ch) { return IsWhiteSpaceChar(ch); }
    };

    template<typename T> struct is_anysp {};

    template<> struct is_anysp<char>
    {
        bool operator()(char ch) { return IsAnySpaceChar(ch); }
    };

    template<> struct is_anysp<wchar_t>
    {
        bool operator()(wchar_t ch) { return IsAnySpaceChar(ch); }
    };

    template<typename T> struct is_tabsp {};

    template<> struct is_tabsp<char>
    {
        bool operator()(char ch) { return IsTabSpaceChar(ch); }
    };

    template<> struct is_tabsp<wchar_t>
    {
        bool operator()(wchar_t ch) { return IsTabSpaceChar(ch); }
    };

    template<typename T> struct is_newln {};

    template<> struct is_newln<char>
    {
        bool operator()(char ch) { return IsNewLineChar(ch); }
    };

    template<> struct is_newln<wchar_t>
    {
        bool operator()(wchar_t ch) { return IsNewLineChar(ch); }
    };

    template<typename T, typename F> void StrStripTrailing(CStrT<T>& S, F isSp) noexcept
    {
        if ( S.IsEmpty() )
            return;

        int i = S.length() - 1;
        while ( isSp(S.GetAt(i)) )
        {
            --i;
        }
        S.Delete(i + 1, -1);
    }

    template<typename T, typename F> void StrStripLeading(CStrT<T>& S, F isSp) noexcept
    {
        int i = 0;
        while ( isSp(S.GetAt(i)) )
        {
            ++i;
        }
        S.Delete(0, i);
    }

    template<typename T> void StrStripImpl(CStrT<T>& S, eStrStripKind kind) noexcept
    {
        switch ( kind )
        {
        case sskWhiteTrailing:
        case sskWhiteBoth:
            StrStripTrailing(S, is_wtsp<T>());
            break;
        case sskAnyTrailing:
        case sskAnyBoth:
            StrStripTrailing(S, is_anysp<T>());
            break;
        case sskTabSpTrailing:
        case sskTabSpBoth:
            StrStripTrailing(S, is_tabsp<T>());
            break;
        case sskNewLnTrailing:
        case sskNewLnBoth:
            StrStripTrailing(S, is_newln<T>());
            break;
        }

        switch ( kind )
        {
        case sskWhiteLeading:
        case sskWhiteBoth:
            StrStripLeading(S, is_wtsp<T>());
            break;
        case sskAnyLeading:
        case sskAnyBoth:
            StrStripLeading(S, is_anysp<T>());
            break;
        case sskTabSpLeading:
        case sskTabSpBoth:
            StrStripLeading(S, is_tabsp<T>());
            break;
        case sskNewLnLeading:
        case sskNewLnBoth:
            StrStripLeading(S, is_newln<T>());
            break;
        }
    }

    void StrStrip(CStrT<char>& S, eStrStripKind kind) noexcept
    {
        StrStripImpl(S, kind);
    }

    void StrStrip(CStrT<wchar_t>& S, eStrStripKind kind) noexcept
    {
        StrStripImpl(S, kind);
    }

    CWStr CStrToWStr(const CStr& S, UINT aCodePage )
    {
        if ( S.IsEmpty() )
            return CWStr();

        int nLenW = 0;
        wchar_t* pStrW = SysUniConv::newMultiByteToUnicode( S.c_str(), S.length(), aCodePage, &nLenW );
        CWStr ret(pStrW, nLenW);
        delete [] pStrW;
        return ret;
    }

    CStr WStrToCStr(const CWStr& S, UINT aCodePage )
    {
        if ( S.IsEmpty() )
            return CStr();

        int nLenA = 0;
        char* pStrA = SysUniConv::newUnicodeToMultiByte( S.c_str(), S.length(), aCodePage, &nLenA );
        CStr ret(pStrA, nLenA);
        delete [] pStrA;
        return ret;
    }

    tstr CStrToTStr(const CStr& S, UINT aCodePage )
    {
      #ifdef UNICODE
        return CStrToWStr(S, aCodePage);
      #else
        return S;
      #endif
    }

    CStr TStrToCStr(const tstr& S, UINT aCodePage )
    {
      #ifdef UNICODE
        return WStrToCStr(S, aCodePage);
      #else
        return S;
      #endif
    }
}

//-------------------------------------------------------------------------

/*
CValue::CValue() : m_type(vtNone)
{
}

CValue::CValue(int i) : m_type(vtInt)
{
    m_data.i = i;
}

CValue::CValue(unsigned int u) : m_type(vtUint)
{
    m_data.u = u;
}

CValue::CValue(long l) : m_type(vtLong)
{
    m_data.l = l;
}

CValue::CValue(unsigned long ul) : m_type(vtUlong)
{
    m_data.ul = ul;
}

CValue::CValue(long long ll) : m_type(vtLongLong)
{
    m_data.ll = ll;
}

CValue::CValue(unsigned long long ull) : m_type(vtUlongLong)
{
    m_data.ull = ull;
}

CValue::CValue(bool b) : m_type(vtBool)
{
    m_data.u = b ? 1 : 0;
}

CValue::CValue(float f) : m_type(vtFloat)
{
    m_data.f = f;
}

CValue::CValue(double d) : m_type(vtDouble)
{
    m_data.d = d;
}

CValue::CValue(std::nullptr_t) : m_type(vtPtr)
{
    m_data.p = nullptr;
}

CValue::CValue(const void* p) : m_type(vtPtr)
{
    m_data.p = p;
}

CValue::CValue(const TCHAR* s) : m_type(vtStr)
{
    setStrData(s, s != nullptr ? lstrlen(s) : 0);
}

CValue::CValue(const TCHAR* s, size_t len)
{
    setStrData(s, len);
}

CValue::CValue(const CValue& v) : m_type(v.m_type)
{
    setData(v);
}

CValue::CValue(CValue&& v) : m_type(vtNone)
{
    Swap(v);
}

CValue::~CValue()
{
    clearData();
}

CValue& CValue::operator=(const CValue& v)
{
    clearData();
    m_type = vtNone;
    setData(v);
    m_type = v.m_type;
    return *this;
}

CValue& CValue::operator=(CValue&& v)
{
    clearData();
    m_type = vtNone;
    Swap(v);
    return *this;
}

CValue::eValueType CValue::GetType() const
{
    return m_type;
}

int CValue::GetInt() const
{
    return getNumber<int>();
}

unsigned int CValue::GetUint() const
{
    return getNumber<unsigned int>();
}

long CValue::GetLong() const
{
    return getNumber<long>();
}

unsigned long CValue::GetUlong() const
{
    return getNumber<unsigned long>();
}

long long CValue::GetLongLong() const
{
    return getNumber<long long>();
}

unsigned long long CValue::GetUlongLong() const
{
    return getNumber<unsigned long long>();
}

bool CValue::GetBool() const
{
    bool b = false;
    switch ( m_type )
    {
        case vtBool:
        case vtUint:
            b = (m_data.u != 0);
            break;
        case vtInt:
            b = (m_data.i != 0);
            break;
        case vtLong:
            b = (m_data.l != 0);
            break;
        case vtUlong:
            b = (m_data.ul != 0);
            break;
        case vtLongLong:
            b = (m_data.ll != 0);
            break;
        case vtUlongLong:
            b = (m_data.ull != 0);
            break;
        case vtFloat:
            b = (m_data.f != (float)(0));
            break;
        case vtDouble:
            b = (m_data.d != (double)(0));
            break;
        case vtPtr:
            b = (m_data.p != nullptr);
            break;
        case vtStr:
            b = (m_data.s.str != nullptr && m_data.s.len != 0);
            break;
    }
    return b;
}

float CValue::GetFloat() const
{
    return getNumber<float>();
}

double CValue::GetDouble() const
{
    return getNumber<double>();
}

const void* CValue::GetPtr() const
{
    const void* p = nullptr;
    switch ( m_type )
    {
        case vtPtr:
            p = m_data.p;
            break;
        case vtStr:
            p = m_data.s.str;
            break;
    }
    return p;
}

const TCHAR* CValue::GetStr(size_t* len) const
{
    static const TCHAR emptyStr[] = _T("");
    switch ( m_type )
    {
        case vtStr:
            if ( m_data.s.len != 0 )
            {
                if ( len != nullptr )
                    *len = m_data.s.len;
                return m_data.s.str;
            }
            break;
    }
    if ( len != nullptr )
        *len = 0;
    return emptyStr;
}

template<> int CValue::GetValue<int>() const
{
    return GetInt();
}

template<> unsigned int CValue::GetValue<unsigned int>() const
{
    return GetUint();
}

template<> long CValue::GetValue<long>() const
{
    return GetLong();
}

template<> unsigned long CValue::GetValue<unsigned long>() const
{
    return GetUlong();
}

template<> long long CValue::GetValue<long long>() const
{
    return GetLongLong();
}

template<> unsigned long long CValue::GetValue<unsigned long long>() const
{
    return GetUlongLong();
}

template<> bool CValue::GetValue<bool>() const
{
    return GetBool();
}

template<> float CValue::GetValue<float>() const
{
    return GetFloat();
}

template<> double CValue::GetValue<double>() const
{
    return GetDouble();
}

template<> const void* CValue::GetValue<const void*>() const
{
    return GetPtr();
}

template<> const TCHAR* CValue::GetValue<const TCHAR*>() const
{
    return GetStr();
}

void CValue::Swap(CValue& v)
{
    uData tempData;
    eValueType tempType;

    if ( m_type != vtNone)
    {
        memcpy(&tempData, &m_data, sizeof(uData));
    }
    tempType = m_type;

    if ( v.m_type != vtNone )
    {
        memcpy(&m_data, &v.m_data, sizeof(uData));
    }
    m_type = v.m_type;

    if ( tempType != vtNone)
    {
        memcpy(&v.m_data, &tempData, sizeof(uData));
    }
    v.m_type = tempType;
}

template<typename N> N CValue::getNumber() const
{
    N n = 0;
    switch ( m_type )
    {
        case vtInt:
            n = static_cast<N>(m_data.i);
            break;
        case vtUint:
            n = static_cast<N>(m_data.u);
            break;
        case vtLong:
            n = static_cast<N>(m_data.l);
            break;
        case vtUlong:
            n = static_cast<N>(m_data.ul);
            break;
        case vtLongLong:
            n = static_cast<N>(m_data.ll);
            break;
        case vtUlongLong:
            n = static_cast<N>(m_data.ull);
            break;
        case vtBool:
            n = static_cast<N>(m_data.u != 0 ? 1 : 0);
            break;
        case vtFloat:
            n = static_cast<N>(m_data.f);
            break;
        case vtDouble:
            n = static_cast<N>(m_data.d);
            break;
    }
    return n;
}

void CValue::setStrData(const TCHAR* s, size_t len)
{
    if ( len != 0 )
    {
        m_data.s.str = new TCHAR[len + 1];
        lstrcpy(m_data.s.str, s);
    }
    else
    {
        m_data.s.str = nullptr;
    }
    m_data.s.len = len;
}

void CValue::setData(const CValue& v)
{
    if ( v.m_type == vtStr )
    {
        setStrData(v.m_data.s.str, v.m_data.s.len);
    }
    else if ( v.m_type != vtNone )
    {
        memcpy(&m_data, &v.m_data, sizeof(uData));
    }
}

void CValue::clearData()
{
    if ( m_type == vtStr )
    {
        if ( m_data.s.str != nullptr )
        {
            delete [] m_data.s.str;
            m_data.s.len = 0;
        }
    }
}
*/

//-------------------------------------------------------------------------

