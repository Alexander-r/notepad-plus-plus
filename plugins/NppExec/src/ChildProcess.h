#ifndef _child_process_h_
#define _child_process_h_
//--------------------------------------------------------------------

#include "base.h"
#include "cpp/CStrT.h"
#include "qtport/NppExecProcess.h" // Qt port: processes, pipes and signals
#include <pty.h>

class CNppExec;
class CScriptEngine;

// Qt port: the console events of Windows, which become signals
inline constexpr unsigned int CTRL_C_EVENT = 0;
inline constexpr unsigned int CTRL_BREAK_EVENT = 1;

class CProcessKiller
{
    public:
        enum eKillMethod {
            killNone = 0,      // do nothing
            killCtrlBreak = 1,
            killCtrlC,
            killWmClose
        };

        CProcessKiller(const PROCESS_INFORMATION* pProcInfo);
        ~CProcessKiller();

        bool Kill(eKillMethod nKillMethod, unsigned int nWaitTimeout = 0);

    protected:
        bool KillByCtrlBreak(unsigned int nWaitTimeout);
        bool KillByCtrlC(unsigned int nWaitTimeout);
        bool KillByWmClose(unsigned int nWaitTimeout);

        bool KillByConsoleCtrlEvent(unsigned int nCtrlEvent, unsigned int nWaitTimeout);
        bool IsProcessActive() const;

    protected:
        PROCESS_INFORMATION m_ProcInfo;
};

// Qt port: a pseudo terminal of forkpty in place of Windows' pseudo console, which is always there
struct PseudoConsoleHelper
{
    typedef HANDLE typeHPCON; // the terminal's master

    // Not null, as the pseudo terminal can always be made
    const void* pfnCreatePseudoConsole = reinterpret_cast<const void*>(&::forkpty);
};

class CChildProcess
{
    public:
        enum eAnsiEscSeq {
            escKeepRaw = 0,  // keep the esc-sequence characters without processing
            escRemove,       // remove the esc-sequence characters
            escProcess = escRemove, // = escRemove (the processing is not implemented yet)

            escTotalCount    // the last item here!
        };

    public:
        //CChildProcess();
        CChildProcess(CScriptEngine* pScriptEngine);
        ~CChildProcess();
        const TCHAR* GetInstanceStr() const;
        bool Create(HWND hParentWnd, LPCTSTR cszCommandLine);
        bool Kill(const CProcessKiller::eKillMethod arrKillMethods[], int nKillMethods,
                  unsigned int nWaitTimeout,
                  CProcessKiller::eKillMethod* pnSucceededKillMethod);
        void MustBreak(unsigned int nBreakMethod);

        bool WriteInput(const TCHAR* szLine, bool bFFlush = false);

        tstr& GetOutput(); // non-const allows to avoid copying at the very end
        int   GetExitCode() const;
        DWORD GetProcessId() const;
        const PROCESS_INFORMATION* GetProcessInfo() const;

        bool IsPseudoCon() const;
        const TCHAR* GetNewLine() const; // usually "\n" or "\r"
        unsigned int GetEncoding() const;
        unsigned int GetAnsiEscSeq() const;

        static bool RemoveAnsiEscSequencesFromLine(tstr& Line);

    protected:
        static bool IsWindowsNT();

        enum eCommandLinePolicy {
            clpNone = 0, // do nothing
            clpPathExt,  // try %PATH% and extensions from %PATHEXT%
            clpComSpec   // start with "%COMSPEC% /C" or "cmd /C"
        };
        static void applyCommandLinePolicy(tstr& sCmdLine, eCommandLinePolicy mode);

        void  reset();
        bool  isBreaking() const;
        void  closePipes();
        void  closePseudoConsole();
        bool  applyOutputFilters(const tstr& _line, bool bOutput);
        bool  applyReplaceFilters(tstr& _line, tstr& printLine, bool bOutput);
        DWORD readPipesAndOutput(CStrT<char>& bufLine, 
                                 bool& bPrevLineEmpty,
                                 int&  nPrevState,
                                 bool  bOutputAll,
                                 bool& bDoOutputNext);

    private:
        CNppExec*           m_pNppExec;
        CScriptEngine*      m_pScriptEngine;
        tstr                m_strInstance;
        tstr                m_strOutput;
        int                 m_nExitCode;
        unsigned int        m_nBreakMethod;
        HANDLE              m_hStdInReadPipe;
        HANDLE              m_hStdInWritePipe; 
        HANDLE              m_hStdOutReadPipe;
        HANDLE              m_hStdOutWritePipe;
        PseudoConsoleHelper::typeHPCON m_hPseudoCon;
        void*               m_pAttributeList; // Qt port: unused
        PROCESS_INFORMATION m_ProcessInfo;
};

//--------------------------------------------------------------------
#endif
