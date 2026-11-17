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

#include "DlgHelpAbout.h"
#include "CAnyWindow.h"
#include "NppExec.h"
#include <shellapi.h>
#include <CommCtrl.h>


INT_PTR CALLBACK HelpAboutDlgProc(
  HWND   hDlg, 
  UINT   uMessage, 
  WPARAM wParam, 
  LPARAM lParam)
{
  if (uMessage == WM_COMMAND)
  {
    switch (LOWORD(wParam)) 
    {
      case IDOK:
      case IDCANCEL:
      {
        EndDialog(hDlg, 1);
        return 1;
      }
      case IDC_BT_DONATE:
      {
        // Qt port: the desktop's browser
        nppExecShellExecute(_T("https://www.paypal.com/donate/?hosted_button_id=W852AH392AZDJ"), NULL);
        break;
      }
      default:
        break;
    }
  }

  else if (uMessage == WM_NOTIFY)
  {
    NMHDR* pnmHdr = (NMHDR*) lParam;
    if (pnmHdr->code == NM_CLICK)
    {
      // Qt port: the label opens its link itself, so this comes from nowhere
      nppExecShellExecute(_T("https://github.com/d0vgan/nppexec/"), NULL);
    }
  }

  else if (uMessage == WM_SYSCOMMAND)
  {
    if (wParam == SC_CLOSE)
    {
      EndDialog(hDlg, 0);
      return 1;
    }
  }

  else if (uMessage == WM_INITDIALOG)
  {
    CAnyWindow Wnd;
    HWND       hEd;

    Wnd.m_hWnd = hDlg;
    Wnd.CenterWindow(Runtime::GetNppExec().m_nppData._nppHandle);

    hEd = GetDlgItem(hDlg, IDC_ED_INFO);  
    if (hEd)
    {
      tstr Notes = 
          _T("Notes:\r\n") \
          _T("- You can execute commands and scripts directly from the Console window.\r\n") \
          _T("- Type HELP in the Console window to see available commands and ") \
          _T("environment variables. ") \
          _T("Commands are case-insensitive.\r\n\r\n") \
          _T("Additional information:\r\n") \
          _T("- NppExec\\\\Advanced Options... allows tweaking of NppExec.\r\n") \
          _T("- NppExec\\\\Help/Docs... opens the NppExec_TechInfo document.\r\n") \
          _T("- Inspect the folder \"plugins/NppExec/doc/NppExec\" for NppExec's documentation.\r\n") /* Qt port: the folders here */ \
          _T("- Inspect the folder \"plugins/NppExec\" for header files used by NppExec at runtime.\r\n") \
          _T("- NppExec's config files are located in the \"plugins/Config\" folder of the settings.\r\n") \
          _T("- Temporary script is saved to \"");
      Notes += SCRIPTFILE_TEMP;
      Notes +=
          _T("\".\r\n") \
          _T("- User scripts are saved to \"");
      Notes += SCRIPTFILE_SAVED;
      Notes +=
          _T("\".\r\n") \
          _T("- Console commands history is saved to \"");
      Notes += CMDHISTORY_FILENAME;
      Notes +=
          _T("\".");

      SendMessage(hEd, EM_LIMITTEXT, 2048, 0);
      SetWindowText(hEd, Notes.c_str());
    }
  }

  return 0;
}

