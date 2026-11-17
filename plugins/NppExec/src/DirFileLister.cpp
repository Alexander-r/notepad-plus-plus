/*
This file is part of NppExec
Copyright (C) 2013 DV <dvv81 (at) ukr (dot) net>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "DirFileLister.h"

// Qt port: the directory listing
#include <shlwapi.h>
#include <QDir>
#include <QFileInfo>


CDirFileLister::CDirFileLister() : _SearchFlags(0), _isDirItr(false), _isEndItr(true)
{
    _itr = _FilesList.end();
}

CDirFileLister::~CDirFileLister()
{
}

void CDirFileLister::Clear()
{
    _SearchFlags = 0;
    _isDirItr = false;
    _isEndItr = true;
    _PathName.clear();
    _FilesList.clear();
    _DirsList.clear();
    _itr = _FilesList.end();
}

const TCHAR* CDirFileLister::FindNext(const TCHAR* szPathName, 
                                      UINT uSearchFlags , bool bAllowCyclicSearch )
{
    if ( (uSearchFlags & ESF_FILES) || (uSearchFlags & ESF_DIRS) )
    {
        if ( szPathName && szPathName[0] )
        {
            if ( (_SearchFlags != uSearchFlags) || 
                 (_PathName != szPathName) )
            {
                return initSearch(szPathName, uSearchFlags) ? _itr->c_str() : NULL;
            }

            return GetNext(bAllowCyclicSearch);
        }
    }
    return NULL;
}

const TCHAR* CDirFileLister::FindPrev(const TCHAR* szPathName, 
                                      UINT uSearchFlags , bool bAllowCyclicSearch )
{
    if ( (uSearchFlags & ESF_FILES) || (uSearchFlags & ESF_DIRS) )
    {
        if ( szPathName && szPathName[0] )
        {
            if ( (_SearchFlags != uSearchFlags) || 
                 (_PathName != szPathName) )
            {
                if ( initSearch(szPathName, uSearchFlags) )
                {
                    if ( (!_FilesList.empty()) && 
                         (_DirsList.empty() || !(uSearchFlags & ESF_PLACEFILESFIRST)) )
                    {
                        _itr = _FilesList.end(); // after last item
                        --_itr; // iterator to last item
                        _isDirItr = false;
                    }
                    else if ( !_DirsList.empty() )
                    {
                        _itr = _DirsList.end(); // after last item
                        --_itr; // iterator to last item
                        _isDirItr = true;
                    }
                    return _itr->c_str();
                }
                return NULL;
            }

            return GetPrev(bAllowCyclicSearch);
        }
    }
    return NULL;
}

const TCHAR* CDirFileLister::GetItem(UINT* pLength , bool* pIsDir )
{
    if ( !_isEndItr )
    {
        if ( pLength )  *pLength = (UINT) _itr->length();
        if ( pIsDir )  *pIsDir = _isDirItr;
        return _itr->c_str();
    }
    if ( pLength )  *pLength = 0;
    if ( pIsDir)  *pIsDir = false;
    return NULL;
}

const TCHAR* CDirFileLister::GetNext(bool bAllowCyclicSearch , UINT* pLength , bool* pIsDir )
{
    if ( !_isEndItr )
    {
        // next item
        ++_itr;
    }
    else
    {
        if ( !(_SearchFlags & ESF_PLACEFILESFIRST) )
        {
            _itr = _FilesList.end();
            _isDirItr = false;
        }
        else
        {
            _itr = _DirsList.end();
            _isDirItr = true;
        }
    }
    
    if ( (!_isDirItr) && (_itr == _FilesList.end()) )
    {
        _isEndItr = true;
        // we are at the end of file list
        if ( (bAllowCyclicSearch || (_SearchFlags & ESF_PLACEFILESFIRST)) && !_DirsList.empty() )
        {
            // beginning of dir list
            _itr = _DirsList.begin();
            _isDirItr = true;
            _isEndItr = false;
        }
        else if ( bAllowCyclicSearch && !_FilesList.empty() )
        {
            // beginning of file list
            _itr = _FilesList.begin();
            _isDirItr = false;
            _isEndItr = false;
        }
    }
    else if ( _isDirItr && (_itr == _DirsList.end()) )
    {
        _isEndItr = true;
        // we are at the end of dir list
        if ( (bAllowCyclicSearch || !(_SearchFlags & ESF_PLACEFILESFIRST)) && !_FilesList.empty() )
        {
            // beginning of file list
            _itr = _FilesList.begin();
            _isDirItr = false;
            _isEndItr = false;
        }
        else if ( bAllowCyclicSearch && !_DirsList.empty() )
        {
            // beginning of dir list
            _itr = _DirsList.begin();
            _isDirItr = true;
            _isEndItr = false;
        }
    }

    return GetItem(pLength, pIsDir);
}

const TCHAR* CDirFileLister::GetPrev(bool bAllowCyclicSearch , UINT* pLength , bool* pIsDir )
{
    if ( _isEndItr )
    {
        if ( !(_SearchFlags & ESF_PLACEFILESFIRST) )
        {
            _itr = _DirsList.begin();
            _isDirItr = true;
        }
        else
        {
            _itr = _FilesList.begin();
            _isDirItr = false;
        }
    }
    
    if ( (!_isDirItr) && (_itr == _FilesList.begin()) )
    {
        _isEndItr = true;
        if ( (bAllowCyclicSearch || !(_SearchFlags & ESF_PLACEFILESFIRST)) && !_DirsList.empty() )
        {
            // end of dir list
            _itr = _DirsList.end();
            --_itr; // iterator to last item
            _isDirItr = true;
            _isEndItr = false;
        }
        else if ( bAllowCyclicSearch && !_FilesList.empty() )
        {
            // end of file list
            _itr = _FilesList.end();
            --_itr; // iterator to last item
            _isDirItr = false;
            _isEndItr = false;
        }
    }
    else if ( _isDirItr && (_itr == _DirsList.begin()) )
    {
        _isEndItr = true;
        if ( (bAllowCyclicSearch || (_SearchFlags & ESF_PLACEFILESFIRST)) && !_FilesList.empty() )
        {
            // end of file list
            _itr = _FilesList.end();
            --_itr; // iterator to last item
            _isDirItr = false;
            _isEndItr = false;
        }
        else if ( bAllowCyclicSearch && !_DirsList.empty() )
        {
            // end of dir list
            _itr = _DirsList.end();
            --_itr; // iterator to last item
            _isDirItr = true;
            _isEndItr = false;
        }
    }
    else
    {
        // previous item
        --_itr;
    }

    return GetItem(pLength, pIsDir);
}

bool CDirFileLister::initSearch(const TCHAR* szPathName, UINT uSearchFlags)
{
    Clear();

    _SearchFlags = uSearchFlags;
    _PathName = szPathName;

    TStr path = _PathName;
    if ( (_PathName.rfind(_T('*')) == TStr::npos) &&
         (_PathName.rfind(_T('?')) == TStr::npos) )
    {
        path += _T("*.*");
    }

    // Qt port: the directory's entries through Qt, in place of FindFirstFile and FindNextFile, which also take a
    // pattern of * and ? after the directory, matched whatever the case
    const QString fullPattern = QString::fromStdWString(path);
    const int sep = fullPattern.lastIndexOf(u'/');
    const QString dirPath = (sep >= 0) ? fullPattern.left(sep + 1) : QStringLiteral(".");
    const std::wstring pattern = fullPattern.mid(sep + 1).toStdWString();
    const QDir dir(dirPath);

    if ( dir.exists() )
    {
        const QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::NoSort);
        for ( const QFileInfo& entry : entries )
        {
            const TStr name = entry.fileName().toStdWString();
            if ( !::PathMatchSpec(name.c_str(), pattern.c_str()) )
                continue;
            {
                {
                    if ( entry.isDir() )
                    {
                        if ( uSearchFlags & ESF_DIRS )
                            _DirsList.push_back( name );
                    }
                    else
                    {
                        if ( uSearchFlags & ESF_FILES )
                            _FilesList.push_back( name );
                    }
                }
            }
        } 

        if ( !_FilesList.empty() )
        {
            if ( uSearchFlags & ESF_SORTED )
            {
                _FilesList.sort(StrLess);
            }
            _itr = _FilesList.begin();
            _isDirItr = false;
            _isEndItr = false;
        }
        
        if ( !_DirsList.empty() )
        {
            if ( uSearchFlags & ESF_SORTED )
            {
                _DirsList.sort(StrLess);
            }
            if ( _isEndItr || !(uSearchFlags & ESF_PLACEFILESFIRST) )
            {
                _itr = _DirsList.begin();
                _isDirItr = true;
                _isEndItr = false;
            }
        }

    }
    
    return !_isEndItr;
}
