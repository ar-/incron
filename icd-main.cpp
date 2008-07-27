
/// inotify cron daemon main file
/**
 * \file icd-main.cpp
 * 
 * inotify cron system
 * 
 * Copyright (C) 2006, 2007 Lukas Jelinek, <lukas@aiken.cz>
 * 
 * This program is free software; you can use it, redistribute
 * it and/or modify it under the terms of the GNU General Public
 * License, version 2 (see LICENSE-GPL).
 *  
 */

#include <map>
#include <signal.h>
#include <wait.h>
#include <fcntl.h>
#include <pwd.h>
#include <dirent.h>
#include <syslog.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/stat.h>

#include "inotify-cxx.h"
#include "appinst.h"
#include "appargs.h"

#include "incron.h"
#include "incrontab.h"
#include "usertable.h"
#include "incroncfg.h"


/// Logging options (console as fallback, log PID)
#define INCRON_LOG_OPTS (LOG_CONS | LOG_PID)

/// Logging facility (use CRON)
#define INCRON_LOG_FACIL LOG_CRON

/// incrond version string
#define INCROND_VERSION INCROND_NAME " " INCRON_VERSION

/// incrontab description string
#define INCROND_DESCRIPTION "incrond - inotify cron daemon\n" \
                            "(c) Lukas Jelinek, 2006, 2007"

/// incrontab help string
#define INCROND_HELP INCROND_DESCRIPTION "\n\n" \
          "usage: incrond [<options>]\n\n" \
          "<operation> may be one of the following:\n" \
          "These options may be used:\n" \
          "  -?, --about                  gives short information about program\n" \
          "  -h, --help                   prints this help text\n" \
          "  -n, --foreground             runs on foreground (no daemonizing)\n" \
          "  -k, --kill                   terminates running instance of incrond\n" \
          "  -f <FILE>, --config=<FILE>   overrides default configuration file  (requires root privileges)\n" \
          "  -V, --version                prints program version\n\n" \
          "For reporting bugs please use http:://bts.aiken.cz\n"



/// User name to user table mapping table
SUT_MAP g_ut;

/// Finish program yes/no
volatile bool g_fFinish = false;

/// Pipe for notifying about dead children
int g_cldPipe[2];

// Buffer for emptying child pipe
#define CHILD_PIPE_BUF_LEN 32
char g_cldPipeBuf[CHILD_PIPE_BUF_LEN];

/// Daemonize true/false
bool g_daemon = true;

/// Handles a signal.
/**
 * For SIGTERM and SIGINT it sets the program finish variable.
 * For SIGCHLD it writes a character into the notification pipe
 * (this is a workaround made due to disability to reliably
 * wait for dead children).
 * 
 * \param[in] signo signal number
 */
void on_signal(int signo)
{
  switch (signo) {
    case SIGTERM:
    case SIGINT:
      g_fFinish = true;
      break;
    case SIGCHLD:
      // first empty pipe (to prevent internal buffer overflow)
      do {} while (read(g_cldPipe[0], g_cldPipeBuf, CHILD_PIPE_BUF_LEN) > 0);
      
      // now write one character
      write(g_cldPipe[1], "X", 1);
      break;
    default:;
  }
}




/// Attempts to load all user incron tables.
/**
 * Loaded tables are registered for processing events.
 * 
 * \param[in] pEd inotify event dispatcher
 * 
 * \throw InotifyException thrown if base table directory cannot be read
 */
void load_tables(EventDispatcher* pEd) throw (InotifyException)
{
  // WARNING - this function has not been optimized!!!
  
  std::string s;
  if (!IncronCfg::GetValue("system_table_dir", s))
    throw InotifyException("configuration system is corrupted", EINVAL);
  
  DIR* d = opendir(s.c_str());
  if (d != NULL) {
    syslog(LOG_NOTICE, "loading system tables");
      
    struct dirent* pDe = NULL;
    while ((pDe = readdir(d)) != NULL) {
      std::string un(pDe->d_name);
      std::string path(IncronCfg::BuildPath(s, pDe->d_name)); 
      
      bool ok = pDe->d_type == DT_REG;
      if (pDe->d_type == DT_UNKNOWN) {
        struct stat st;
        if (stat(path.c_str(), &st) == 0)
          ok = S_ISREG(st.st_mode);
      }
      
      if (ok) {
        syslog(LOG_INFO, "loading table %s", pDe->d_name);
        UserTable* pUt = new UserTable(pEd, un, true);
        g_ut.insert(SUT_MAP::value_type(path, pUt));
        pUt->Load();
      }
    }
    
    closedir(d);
  }
  else {
    syslog(LOG_WARNING, "cannot open system table directory (ignoring)");
  }
  
  if (!IncronCfg::GetValue("user_table_dir", s))
    throw InotifyException("configuration system is corrupted", EINVAL);
    
  d = opendir(s.c_str());
  if (d == NULL)
    throw InotifyException("cannot open user table directory", errno);
  
  syslog(LOG_NOTICE, "loading user tables");
    
  struct dirent* pDe = NULL;
  while ((pDe = readdir(d)) != NULL) {
    std::string un(pDe->d_name);
    std::string path(IncronCfg::BuildPath(s, pDe->d_name));
    
    bool ok = pDe->d_type == DT_REG;
    if (pDe->d_type == DT_UNKNOWN) {
      struct stat st;
      if (stat(path.c_str(), &st) == 0)
        ok = S_ISREG(st.st_mode);
    }
    
    if (ok) {
      if (UserTable::CheckUser(pDe->d_name)) {
        syslog(LOG_INFO, "loading table for user %s", pDe->d_name);
        UserTable* pUt = new UserTable(pEd, un, false);
        g_ut.insert(SUT_MAP::value_type(path, pUt));
        pUt->Load();
      }
      else {
        syslog(LOG_WARNING, "table for invalid user %s found (ignored)", pDe->d_name);
      }
    }
  }
  
  closedir(d);
}

/// Prepares a 'dead/done child' notification pipe.
/**
 * This function returns no value at all and on error it
 * throws an exception.
 */
void prepare_pipe()
{
  g_cldPipe[0] = -1;
  g_cldPipe[1] = -1;
  
  if (pipe(g_cldPipe) != 0)
      throw InotifyException("cannot create notification pipe", errno, NULL);
  
  for (int i=0; i<2; i++) {
    int res = fcntl(g_cldPipe[i], F_GETFL);
    if (res == -1)
      throw InotifyException("cannot get pipe flags", errno, NULL);
    
    res |= O_NONBLOCK;
        
    if (fcntl(g_cldPipe[i], F_SETFL, res) == -1)
      throw InotifyException("cannot set pipe flags", errno, NULL);
      
    res = fcntl(g_cldPipe[i], F_GETFD);
    if (res == -1)
      throw InotifyException("cannot get pipe flags", errno, NULL);
    
    res |= FD_CLOEXEC;
        
    if (fcntl(g_cldPipe[i], F_SETFD, res) == -1)
      throw InotifyException("cannot set pipe flags", errno, NULL);
  }
}

/// Checks whether a parameter string is a specific command.
/**
 * The string is accepted if it equals either the short or long
 * form of the command.
 * 
 * \param[in] s checked string
 * \param[in] shortCmd short form of command
 * \param[in] longCmd long form of command
 * \return true = string accepted, false = otherwise
 */  
bool check_parameter(const char* s, const char* shortCmd, const char* longCmd)
{
  return strcmp(s, shortCmd)  == 0
      || strcmp(s, longCmd)   == 0;
}

/// Initializes a poll array.
/**
 * \param[out] pfd poll structure array 
 * \param[in] pipefd pipe file descriptor
 * \param[in] infd inotify infrastructure file descriptor
 */
void init_poll_array(struct pollfd pfd[], int pipefd, int infd)
{
  pfd[0].fd = pipefd;
  pfd[0].events = (short) POLLIN;
  pfd[0].revents = (short) 0;
  pfd[1].fd = infd;
  pfd[1].events = (short) POLLIN;
  pfd[1].revents = (short) 0;
}


/// Main application function.
/**
 * \param[in] argc argument count
 * \param[in] argv argument array
 * \return 0 on success, 1 on error
 * 
 * \attention In daemon mode, it finishes immediately.
 */
int main(int argc, char** argv)
{
  AppArgs::Init();

  if (!(  AppArgs::AddOption("about",       '?', AAT_NO_VALUE, false)
      &&  AppArgs::AddOption("help",        'h', AAT_NO_VALUE, false)
      &&  AppArgs::AddOption("foreground",  'n', AAT_NO_VALUE, false)
      &&  AppArgs::AddOption("kill",        'k', AAT_NO_VALUE, false)
      &&  AppArgs::AddOption("config",      'f', AAT_MANDATORY_VALUE, false))
      &&  AppArgs::AddOption("version",     'V', AAT_NO_VALUE, false))
  {
    fprintf(stderr, "error while initializing application");
    return 1;
  }
  
  AppArgs::Parse(argc, argv);
  
  if (AppArgs::ExistsOption("help")) {
    fprintf(stderr, "%s\n", INCROND_HELP);
    return 0;
  }
  
  if (AppArgs::ExistsOption("about")) {
    fprintf(stderr, "%s\n", INCROND_DESCRIPTION);
    return 0;
  }
  
  if (AppArgs::ExistsOption("version")) {
    fprintf(stderr, "%s\n", INCROND_VERSION);
    return 0;
  }
  
  AppInstance app("incrond");
  
  IncronCfg::Init();
  
  if (AppArgs::ExistsOption("kill")) {
    fprintf(stderr, "attempting to terminate a running instance of incrond...\n");
    if (app.SendSignal(SIGTERM)) {
      fprintf(stderr, "instance(s) notified, going down\n");
      return 0;
    }
    else { 
      fprintf(stderr, "error - incrond probably not running\n");
      return 1;
    }
  }
  
  if (AppArgs::ExistsOption("foreground"))
    g_daemon = false;
  
  
  openlog(INCROND_NAME, INCRON_LOG_OPTS, INCRON_LOG_FACIL);
  
  syslog(LOG_NOTICE, "starting service (version %s, built on %s %s)", INCRON_VERSION, __DATE__, __TIME__);
  
  std::string cfg;
  if (!AppArgs::GetOption("config", cfg))
    cfg = INCRON_CONFIG;
  IncronCfg::Load(cfg);
  
  AppArgs::Destroy();
  
  int ret = 0;
  
  std::string sysBase;
  std::string userBase;
  
  if (!IncronCfg::GetValue("system_table_dir", sysBase))
    throw InotifyException("configuration is corrupted", EINVAL);
  
  if (access(sysBase.c_str(), R_OK) != 0) {
    syslog(LOG_CRIT, "cannot read directory for system tables (%s): (%i) %s", sysBase.c_str(), errno, strerror(errno));
    if (!g_daemon)
        fprintf(stderr, "cannot read directory for system tables (%s): (%i) %s", sysBase.c_str(), errno, strerror(errno));
    ret = 1;
    goto error;
  }
  
  if (!IncronCfg::GetValue("user_table_dir", userBase))
    throw InotifyException("configuration is corrupted", EINVAL);
  
  if (access(userBase.c_str(), R_OK) != 0) {
    syslog(LOG_CRIT, "cannot read directory for user tables (%s): (%i) %s", userBase.c_str(), errno, strerror(errno));
    if (!g_daemon)
        fprintf(stderr, "cannot read directory for user tables (%s): (%i) %s", userBase.c_str(), errno, strerror(errno));
    ret = 1;
    goto error;
  }
  
  try {
    if (g_daemon)
      daemon(0, 0);
  
    try {
    if (!app.Lock()) {
      syslog(LOG_CRIT, "another instance of incrond already running");
      if (!g_daemon)
        fprintf(stderr, "another instance of incrond already running\n");
      ret = 1;
      goto error;
      }
    } catch (AppInstException e) {
      syslog(LOG_CRIT, "instance lookup failed: (%i) %s", e.GetErrorNumber(), strerror(e.GetErrorNumber()));
      if (!g_daemon)
        fprintf(stderr, "instance lookup failed: (%i) %s\n", e.GetErrorNumber(), strerror(e.GetErrorNumber()));
      ret = 1;
      goto error;
    }
    
    prepare_pipe();
    
    Inotify in;
    in.SetNonBlock(true);
    in.SetCloseOnExec(true);
    
    uint32_t wm = IN_CLOSE_WRITE | IN_DELETE | IN_MOVE | IN_DELETE_SELF | IN_UNMOUNT;
    InotifyWatch stw(sysBase, wm);
    in.Add(stw);
    InotifyWatch utw(userBase, wm);
    in.Add(utw);
    
    EventDispatcher ed(g_cldPipe[0], &in, &stw, &utw);
    
    try {
      load_tables(&ed);
    } catch (InotifyException e) {
      int err = e.GetErrorNumber();
      syslog(LOG_CRIT, "%s: (%i) %s", e.GetMessage().c_str(), err, strerror(err));
      ret = 1;
      goto error;
    }
    
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGCHLD, on_signal);
    
    syslog(LOG_NOTICE, "ready to process filesystem events");
    
    while (!g_fFinish) {
      
      int res = poll(ed.GetPollData(), ed.GetSize(), -1);
      
      if (res > 0) {
        if (ed.ProcessEvents())
          UserTable::FinishDone();
      }
      else if (res < 0) {
        if (errno != EINTR)
          throw InotifyException("polling failed", errno, NULL);
      }
      
    }
    
    if (g_cldPipe[0] != -1)
      close(g_cldPipe[0]);
    if (g_cldPipe[1] != -1)
      close(g_cldPipe[1]);
  } catch (InotifyException e) {
    int err = e.GetErrorNumber();
    syslog(LOG_CRIT, "*** unhandled exception occurred ***");
    syslog(LOG_CRIT, "  %s", e.GetMessage().c_str());
    syslog(LOG_CRIT, "  error: (%i) %s", err, strerror(err));
    ret = 1;
  }

error:

  syslog(LOG_NOTICE, "stopping service");
  
  closelog();
  
  return ret;
}
