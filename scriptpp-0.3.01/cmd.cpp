// +-------------------------------------------------------------------------+
// |                     Script Plus Plus vers. 0.3.01                       |
// | Copyright (c) Andrey Vikt. Stolyarov <crocodil_AT_croco.net>  2003-2011 |
// | ----------------------------------------------------------------------- |
// | This is free software.  Permission is granted to everyone to use, copy  |
// |        or modify this software under the terms and conditions of        |
// |                 GNU LESSER GENERAL PUBLIC LICENSE, v. 2.1               |
// |     as published by Free Software Foundation (see the file LGPL.txt)    |
// |                                                                         |
// | Please visit http://www.croco.net/software/scriptpp to get a fresh copy |
// | ----------------------------------------------------------------------- |
// |   This code is provided strictly and exclusively on the "AS IS" basis.  |
// | !!! THERE IS NO WARRANTY OF ANY KIND, NEITHER EXPRESSED NOR IMPLIED !!! |
// +-------------------------------------------------------------------------+




#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <termios.h>


#include "scrvar.hpp"
#include "scrvect.hpp"
#include "cmd.hpp"


static void makeowngroup()
{
#ifdef __FreeBSD__
    int p = getpid();
    setpgrp(p, p);
#else
    setpgrp();
#endif
}

bool ReadStream::FOpen(const char *fname)
{
    f = (void*)fopen(fname, "r");
    return f != 0;
}

bool ReadStream::FDOpen(int fd)
{
    f = (void*)fdopen(fd, "r");
    return f != 0;
}

void ReadStream::FClose()
{
    if(f) fclose((FILE*)f);
    f = 0;
}

bool ReadStream::ReadLine(ScriptVariable &target)
{
    if(!f) return false;
    bool ok = false;
    char buf[1024];
    ScriptVariable res;
    char *r;
    do {
        r = fgets(buf, sizeof(buf), (FILE*)f);
        if(r) {
             ok = true;
             res += r;
        }
    } while(r && res[res.length()-1]!='\n');

    if(!ok) return false;

    if(res[res.length()-1]=='\n') // remove EOL
        res.Range(res.Length()-1, 1).Erase();

    target = res;
    return true;
}

bool ReadStream::ReadLine(ScriptVector &target, int words,
                 const char *delims,
                 const char *trimspaces)
{
    ScriptVariable res;

    target.Clear();

    if(!ReadLine(res)) return false;

    if(words == 1) { // special case
        target[0] = res;
        return true;
    }

    int i=0;
    ScriptVariable::Substring iter(res);
    ScriptVariable::Substring word;
    while(trimspaces ? iter.FetchToken(word, delims, trimspaces) :
                       iter.FetchWord(word, delims))
    {
        target[i] = word.Get();
        i++;
        if(words>0 && i>=words-1) {
            if(!trimspaces) { // if we use words not tokens
                iter.Trim(delims); // remove the 'spaces'
            }
          #if 0
            else
            { // otherwise, just remove the delimiter
                iter.Move(+1);
            }
          #endif
            target[i] = iter.Get();
            break;
        }
    }
    return true;
}


static void safetcsetpgrp(int p)
{
    void (*oldsig)(int) = signal(SIGTTOU, SIG_IGN);
    tcsetpgrp(0, p);
    signal(SIGTTOU, oldsig);
}


ExecProgram::ExecProgram()
{
    status = 0;
    pid = -1;
    save_pgrp = tcgetpgrp(0);
}

bool ExecProgram::CheckChild()
{
    if(pid<=0) return false;
    int r = waitpid(pid, &status, WNOHANG);
    if(r==pid) {
        pid = -1;
        return false;
    }
    return true;
}

void ExecProgram::WaitChild()
{
    if(pid<=0) return;
    waitpid(pid, &status, 0);
    pid = -1;
    if(save_pgrp!=-1) safetcsetpgrp(save_pgrp);
}

void ExecProgram::KillChild(int sig)
{
    if(pid<=0) return;
    kill(pid, sig);
}

bool ExecProgram::Success() const
{
    return WIFEXITED(status) && WEXITSTATUS(status)==0;
}

ExecProgram::~ExecProgram()
{
    if(pid>0) WaitChild();
}


ExecResultParse::ExecResultParse(const char *path, ...)
{
    ScriptVector cmdline;
    cmdline.AddItem(path);
    va_list ap;
    va_start(ap, path);
    const char *a;
    while((a=va_arg(ap, const char *))) cmdline.AddItem(a);
    va_end(ap);

    int fd[2];
    if(-1==pipe(fd)) throw PipeFailed();

    pid = fork();
    if(pid == -1) throw ForkFailed();

    if(pid == 0) { /* child */
        char** argv = cmdline.MakeArgv();
        makeowngroup();
        safetcsetpgrp(getpgrp());
        close(fd[0]);
        dup2(fd[1], 1);
        close(fd[1]);
        execvp(argv[0], argv);
        exit(1);
    }
    close(fd[1]);
    FDOpen(fd[0]);
}

ExecResultParse::~ExecResultParse()
{
    if(pid>0) WaitChild();
}


ExecAndWait::ExecAndWait(const char *path, ...)
{
    ScriptVector cmdline;
    cmdline.AddItem(path);
    va_list ap;
    va_start(ap, path);
    const char *a;
    while((a=va_arg(ap, const char *))) cmdline.AddItem(a);
    va_end(ap);

    pid = fork();
    if(pid == -1) throw ForkFailed();

    if(pid == 0) { /* child */
        char** argv = cmdline.MakeArgv();
        makeowngroup();
        safetcsetpgrp(getpgrp());
        execvp(argv[0], argv);
        exit(1);
    }
    WaitChild();
}

ExecAndWait::ExecAndWait(char * const * argv)
{
    pid = fork();
    if(pid == -1) throw ForkFailed();

    if(pid == 0) { /* child */
        makeowngroup();
        safetcsetpgrp(getpgrp());
        execvp(argv[0], argv);
        exit(1);
    }
    WaitChild();
}

ExecAndWait::~ExecAndWait()
{}



ChrootExecWait::ChrootExecWait(const char *root, const char *path, ...)
{
    ScriptVector cmdline;
    cmdline.AddItem(path);
    va_list ap;
    va_start(ap, path);
    const char *a;
    while((a=va_arg(ap, const char *))) cmdline.AddItem(a);
    va_end(ap);

    pid = fork();
    if(pid == -1) throw ForkFailed();

    if(pid == 0) { /* child */
        char** argv = cmdline.MakeArgv();
        makeowngroup();
        safetcsetpgrp(getpgrp());
        if(-1==chdir(root)) {
            perror("chdir");
            exit(1);
        }
        if(-1==chroot(root)) {
            perror("chroot");
            exit(1);
        }
        execvp(argv[0], argv);
        exit(1);
    }
    WaitChild();
}

ChrootExecWait::~ChrootExecWait()
{}




FileStat::FileStat(const char *filename, bool dereference)
{
    stat_info = new struct stat;
    int res = dereference ?
        stat(filename, (struct stat*) stat_info) :
        lstat(filename, (struct stat*) stat_info);
    if(res == -1) {
        delete (struct stat*) stat_info;
        stat_info = 0;
    }
}

FileStat::~FileStat()
{
    if(stat_info) delete (struct stat*) stat_info;
}

bool FileStat::Exists() const
{
    return stat_info != 0;
}

bool FileStat::IsDir() const
{
    return stat_info && S_ISDIR(((struct stat*)stat_info)->st_mode);
}

bool FileStat::IsRegularFile() const
{
    return stat_info && S_ISREG(((struct stat*)stat_info)->st_mode);
}

bool FileStat::IsEmpty() const
{
    return stat_info && (((struct stat*)stat_info)->st_size == 0);
}

bool FileStat::IsSymlink() const
{
    return stat_info && S_ISLNK(((struct stat*)stat_info)->st_mode);
}

bool FileStat::IsChardev() const
{
    return stat_info && S_ISCHR(((struct stat*)stat_info)->st_mode);
}

bool FileStat::IsBlockdev() const
{
    return stat_info && S_ISBLK(((struct stat*)stat_info)->st_mode);
}

// from <linux/fs.h> // well, it doesn't seem to wish to be included
#define MINORBITS       8
#define MINORMASK       ((1U << MINORBITS) - 1)
#define MAJOR(dev)      ((unsigned int) ((dev) >> MINORBITS))
#define MINOR(dev)      ((unsigned int) ((dev) & MINORMASK))

void FileStat::GetMajorMinor(int &majorn, int &minorn) const
{
    majorn = MAJOR(((struct stat*)stat_info)->st_rdev);
    minorn = MINOR(((struct stat*)stat_info)->st_rdev);
}

ReadDir::ReadDir(const char *path)
{
    dir = (void*) opendir(path);
}

ReadDir::~ReadDir()
{
    if(dir) closedir((DIR*)dir);
}

const char* ReadDir::Next()
{
    if(!dir) return 0;
    struct dirent *d = readdir((DIR*)dir);
    if(!d) return 0;
    return d->d_name;
}

PreserveTerminalMode::PreserveTerminalMode()
{
    p = (void*) new struct termios;
    tcgetattr(0, (struct termios *)p);
}

PreserveTerminalMode::~PreserveTerminalMode()
{

    tcsetattr(0, TCSANOW, (struct termios *)p);
    delete (struct termios*) p;
}
