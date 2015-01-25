
/// inotify cron table manipulator main file
/**
 * \file ict-main.cpp
 *
 * inotify cron system
 *
 * Copyright (C) 2006, 2007, 2008, 2012 Lukas Jelinek, <lukas@aiken.cz>
 * Copyright (C) 2012, 2013 Andreas Altair Redmer, <altair.ibn.la.ahad.sy@gmail.com>
 *
 * This program is free software; you can use it, redistribute
 * it and/or modify it under the terms of the GNU General Public
 * License, version 2 (see LICENSE-GPL).
 *
 * Credits:
 *   kolter (fix for segfaulting on --user)
 *   Christian Ruppert (new include to build with GCC 4.4+)
 *
 */


#include <argp.h>
#include <pwd.h>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>
#include <cstring>
#include <cstdio>

#include "inotify-cxx.h"
#include "appargs.h"

#include "incron.h"
#include "incrontab.h"
#include "incroncfg.h"


/// Alternative editor
#define INCRON_ALT_EDITOR "/etc/alternatives/editor"

/// Default (hard-wired) editor
#define INCRON_DEFAULT_EDITOR "vim"

/// incrontab version string
#define INCRONTAB_VERSION INCRONTAB_NAME " " INCRON_VERSION

/// incrontab description string
#define INCRONTAB_DESCRIPTION "incrontab - inotify cron table manipulator\n" \
                              "(c) Lukas Jelinek, 2006, 2007, 208"

/// incrontab help string
#define INCRONTAB_HELP INCRONTAB_DESCRIPTION "\n\n" \
          "usage: incrontab [<options>] <operation>\n" \
          "       incrontab [<options>] <FILE-TO-IMPORT>\n\n" \
          "<operation> may be one of the following:\n" \
          "  -?, --about                  gives short information about program\n" \
          "  -h, --help                   prints this help text\n" \
          "  -l, --list                   lists user table\n" \
          "  -r, --remove                 removes user table\n" \
          "  -e, --edit                   provides editing user table\n" \
          "  -t, --types                  list supported event types\n" \
          "  -d, --reload                 request incrond to reload user table\n" \
          "  -V, --version                prints program version\n\n" \
          "\n" \
          "These options may be used:\n" \
          "  -u <USER>, --user=<USER>     overrides current user (requires root privileges)\n" \
          "  -f <FILE>, --config=<FILE>   overrides default configuration file  (requires root privileges)\n\n" \
          "For reporting bugs please use https://github.com/ar-/incron/issues\n"




/// Copies a file to an user table.
/**
 * \param[in] rPath path to file
 * \param[in] rUser user name
 * \return true = success, false = failure
 */
bool copy_from_file(const std::string& rPath, const std::string& rUser)
{
  fprintf(stderr, "copying table from file '%s'\n", rPath.c_str());

  IncronTab tab;
  std::string s(rPath);
  if (s == "-")
    s = "/dev/stdin";
  if (!tab.Load(s)) {
    fprintf(stderr, "cannot load table from file '%s'\n", rPath.c_str());
    return false;
  }

  std::string out(IncronTab::GetUserTablePath(rUser));
  if (!tab.Save(out)) {
    fprintf(stderr, "cannot create table for user '%s'\n", rUser.c_str());
    return false;
  }

  struct passwd* ppwd = getpwnam(rUser.c_str());
  if (ppwd == NULL) {
    fprintf(stderr, "cannot find user '%s': %s\n", rUser.c_str(), strerror(errno));
    return false;
  }
  if (chown(out.c_str(), ppwd->pw_uid, -1) != 0) {
    fprintf(stderr, "cannot set owner '%s' to table '%s': %s\n", rUser.c_str(), out.c_str(), strerror(errno));
    return false;
  }

  return true;
}

/// Removes an user table.
/**
 * \param[in] rUser user name
 * \return true = success, false = failure
 */
bool remove_table(const std::string& rUser)
{
  fprintf(stderr, "removing table for user '%s'\n", rUser.c_str());

  std::string tp(IncronTab::GetUserTablePath(rUser));

  if (unlink(tp.c_str()) != 0) {
    if (errno == ENOENT) {
      fprintf(stderr, "table for user '%s' does not exist\n", rUser.c_str());
      return true;
    }
    else {
      fprintf(stderr, "cannot remove table for user '%s': %s\n", rUser.c_str(), strerror(errno));
      return false;
    }
  }

  fprintf(stderr, "table for user '%s' successfully removed\n", rUser.c_str());
  return true;
}

/// Lists an user table.
/**
 * \param[in] rUser user name
 * \return true = success, false = failure
 */
bool list_table(const std::string& rUser)
{
  std::string tp(IncronTab::GetUserTablePath(rUser));

  FILE* f = fopen(tp.c_str(), "r");
  if (f == NULL) {
    if (errno == ENOENT) {
      fprintf(stderr, "no table for %s\n", rUser.c_str());
      return true;
    }
    else {
      fprintf(stderr, "cannot read table for '%s': %s\n", rUser.c_str(), strerror(errno));
      return false;
    }
  }

  char s[1024];
  while (fgets(s, 1024, f) != NULL) {
    fputs(s, stdout);
  }

  fclose(f);

  return true;
}

/// Allows to edit an user table.
/**
 * \param[in] rUser user name
 * \return true = success, false = failure
 *
 * \attention This function is very complex and may contain
 *            various bugs including security ones. Please keep
 *            it in mind..
 */
bool edit_table(const std::string& rUser)
{
  std::string tp(IncronTab::GetUserTablePath(rUser));

  struct passwd* ppwd = getpwnam(rUser.c_str());
  if (ppwd == NULL) {
    fprintf(stderr, "cannot find user '%s': %s\n", rUser.c_str(), strerror(errno));
    return false;
  }

  uid_t uid = ppwd->pw_uid;
  uid_t gid = ppwd->pw_gid;

  char s[NAME_MAX];
  strcpy(s, "/tmp/incron.table-XXXXXX");

  uid_t iu = geteuid();
  uid_t ig = getegid();

  if (setegid(gid) != 0 || seteuid(uid) != 0) {
    fprintf(stderr, "cannot change effective UID/GID for user '%s': %s\n", rUser.c_str(), strerror(errno));
    return false;
  }

  int fd = mkstemp(s);
  if (fd == -1) {
    fprintf(stderr, "cannot create temporary file: %s\n", strerror(errno));
    return false;
  }

  bool ok = false;
  FILE* out = NULL;
  FILE* in = NULL;
  time_t mt = (time_t) 0;
  const char* e = NULL;
  std::string ed;

  if (setegid(ig) != 0 || seteuid(iu) != 0) {
    fprintf(stderr, "cannot change effective UID/GID: %s\n", strerror(errno));
    close(fd);
    goto end;
  }

  out = fdopen(fd, "w");
  if (out == NULL) {
    fprintf(stderr, "cannot write to temporary file: %s\n", strerror(errno));
    close(fd);
    goto end;
  }

  in = fopen(tp.c_str(), "r");
  if (in == NULL) {
    if (errno == ENOENT) {
      in = fopen("/dev/null", "r");
      if (in == NULL) {
        fprintf(stderr, "cannot get empty table for '%s': %s\n", rUser.c_str(), strerror(errno));
        fclose(out);
        goto end;
      }
    }
    else {
      fprintf(stderr, "cannot read old table for '%s': %s\n", rUser.c_str(), strerror(errno));
      fclose(out);
      goto end;
    }
  }

  char buf[1024];
  while (fgets(buf, 1024, in) != NULL) {
    fputs(buf, out);
  }
  fclose(in);
  fclose(out);

  struct stat st;
  if (stat(s, &st) != 0) {
    fprintf(stderr, "cannot stat temporary file: %s\n", strerror(errno));
    goto end;
  }

  mt = st.st_mtime; // save modification time for detecting its change

  // Editor selecting algorithm:
  // 1. Check EDITOR environment variable
  // 2. Check VISUAL environment variable
  // 3. Try to get from configuration
  // 4. Check presence of /etc/alternatives/editor
  // 5. Use hard-wired editor

  e = getenv("EDITOR");
  if (e == NULL) {
    e = getenv("VISUAL");
    if (e == NULL) {

      if (!IncronCfg::GetValue("editor", ed))
        throw InotifyException("configuration is corrupted", EINVAL);

      if (!ed.empty()) {
        e = ed.c_str();
      }
      else {
        if (access(INCRON_ALT_EDITOR, X_OK) == 0)
          e = INCRON_ALT_EDITOR;
        else
          e = INCRON_DEFAULT_EDITOR;
      }
    }
  }

  // this block is explicite due to gotos' usage simplification
  {
    pid_t pid = fork();
    if (pid == 0) {
      if (setgid(gid) != 0 || setuid(uid) != 0) {
        fprintf(stderr, "cannot set user '%s': %s\n", rUser.c_str(), strerror(errno));
        goto end;
      }

      execlp(e, e, s, (const char*) NULL);
      _exit(1);
    }
    else if (pid > 0) {
      int status;
      if (wait(&status) != pid) {
        perror("error while waiting for editor");
        goto end;
      }
      if (!(WIFEXITED(status)) || WEXITSTATUS(status) != 0) {
        perror("editor finished with error");
        goto end;
      }
    }
    else {
      perror("cannot start editor");
      goto end;
    }
  }

  if (stat(s, &st) != 0) {
    fprintf(stderr, "cannot stat temporary file: %s\n", strerror(errno));
    goto end;
  }

  if (st.st_mtime == mt) {
    fprintf(stderr, "table unchanged\n");
    ok = true;
    goto end;
  }

  {
    IncronTab ict;
    if (ict.Load(s) && ict.Save(tp)) {
      if (chmod(tp.c_str(), S_IRUSR | S_IWUSR) != 0) {
        fprintf(stderr, "cannot change mode of temporary file: %s\n", strerror(errno));
      }
    }
    else {
      fprintf(stderr, "cannot move temporary table: %s\n", strerror(errno));
      goto end;
    }

  }

  ok = true;
  fprintf(stderr, "table updated\n");

end:

  unlink(s);
  return ok;
}


/// Prints the list of all available inotify event types.
void list_types()
{
  printf( "IN_ACCESS,IN_MODIFY,IN_ATTRIB,IN_CLOSE_WRITE,"\
          "IN_CLOSE_NOWRITE,IN_OPEN,IN_MOVED_FROM,IN_MOVED_TO,"\
          "IN_CREATE,IN_DELETE,IN_DELETE_SELF,IN_CLOSE,IN_MOVE,"\
          "IN_ONESHOT,IN_ALL_EVENTS");

#ifdef IN_DONT_FOLLOW
  printf(",IN_DONT_FOLLOW");
#endif // IN_DONT_FOLLOW

#ifdef IN_ONLYDIR
  printf(",IN_ONLYDIR");
#endif // IN_ONLYDIR

#ifdef IN_MOVE_SELF
  printf(",IN_MOVE_SELF");
#endif // IN_MOVE_SELF

  printf("\n");
}

/// Reloads an user table.
/**
 * \param[in] rUser user name
 * \return true = success, false = otherwise
 */
bool reload_table(const std::string& rUser)
{
  fprintf(stderr, "requesting table reload for user '%s'...\n", rUser.c_str());

  std::string tp(IncronTab::GetUserTablePath(rUser));

  int fd = open(tp.c_str(), O_WRONLY | O_APPEND);
  if (fd == -1) {
    if (errno == ENOENT) {
      fprintf(stderr, "no table for '%s'\n", rUser.c_str());
      return true;
    }
    else {
      fprintf(stderr, "cannot access table for '%s': %s\n", rUser.c_str(), strerror(errno));
      return false;
    }
  }

  close(fd);

  fprintf(stderr, "request done\n");

  return true;
}

int main(int argc, char** argv)
{
  AppArgs::Init();

  if (!(  AppArgs::AddOption("about",   '?', AAT_NO_VALUE, false)
      &&  AppArgs::AddOption("help",    'h', AAT_NO_VALUE, false)
      &&  AppArgs::AddOption("list",    'l', AAT_NO_VALUE, false)
      &&  AppArgs::AddOption("remove",  'r', AAT_NO_VALUE, false)
      &&  AppArgs::AddOption("edit",    'e', AAT_NO_VALUE, false)
      &&  AppArgs::AddOption("types",   't', AAT_NO_VALUE, false)
      &&  AppArgs::AddOption("reload",  'd', AAT_NO_VALUE, false)
      &&  AppArgs::AddOption("user",    'u', AAT_MANDATORY_VALUE, false)
      &&  AppArgs::AddOption("config",  'f', AAT_MANDATORY_VALUE, false)
      &&  AppArgs::AddOption("version", 'V', AAT_NO_VALUE, false)))
  {
    fprintf(stderr, "error while initializing application");
    return 1;
  }

  AppArgs::Parse(argc, argv);

  if (AppArgs::ExistsOption("help")) {
    fprintf(stderr, "%s\n", INCRONTAB_HELP);
    return 0;
  }

  if (AppArgs::ExistsOption("about")) {
    fprintf(stderr, "%s\n", INCRONTAB_DESCRIPTION);
    return 0;
  }

  if (AppArgs::ExistsOption("version")) {
    fprintf(stderr, "%s\n", INCRONTAB_VERSION);
    return 0;
  }

  bool oper = AppArgs::ExistsOption("list")
          ||  AppArgs::ExistsOption("remove")
          ||  AppArgs::ExistsOption("edit")
          ||  AppArgs::ExistsOption("types")
          ||  AppArgs::ExistsOption("reload");

  size_t vals = AppArgs::GetValueCount();

  if (!oper && vals == 0) {
    fprintf(stderr, "invalid arguments - specify operation or source file\n");
    return 1;
  }

  if (oper && vals > 0) {
    fprintf(stderr, "invalid arguments - operation and source file cannot be combined\n");
    return 1;
  }

  uid_t uid = getuid();

  std::string user;
  bool chuser = AppArgs::GetOption("user", user);

  if (uid != 0 && chuser) {
    fprintf(stderr, "cannot override user to '%s': insufficient privileges\n", user.c_str());
    return 1;
  }

  struct passwd* ppwd = NULL;

  if (chuser) {
	if ((ppwd = getpwnam(user.c_str())) != NULL) {
      if (    setenv("LOGNAME",   ppwd->pw_name,   1) != 0
		  ||  setenv("USER",      ppwd->pw_name,   1) != 0
		  ||  setenv("USERNAME",  ppwd->pw_name,   1) != 0
		  ||  setenv("HOME",      ppwd->pw_dir,    1) != 0
		  ||  setenv("SHELL",     ppwd->pw_shell,  1) != 0)
      {
		perror("cannot set environment variables");
		return 1;
	  }
	} else {
	  fprintf(stderr, "user '%s' not found\n", user.c_str());
	  return 1;
	}
  } else {
    ppwd = getpwuid(uid);
    if (ppwd == NULL) {
      fprintf(stderr, "cannot determine current user\n");
      return 1;
    }
    user = ppwd->pw_name;
  }

  try {

    IncronCfg::Init();

    std::string cfg(INCRON_CONFIG);
    if (AppArgs::GetOption("config", cfg)) {
      if (uid != 0) {
        fprintf(stderr, "insufficient privileges to use custom configuration (will use default)\n");
      }
      else if (euidaccess(cfg.c_str(), R_OK) != 0) {
        perror("cannot read configuration file (will use default)");
      }
    }

    IncronCfg::Load(cfg);

    if (!IncronTab::CheckUser(user)) {
      fprintf(stderr, "user '%s' is not allowed to use incron\n", user.c_str());
      return 1;
    }

    if (!oper) {
      std::string file;
      if (!AppArgs::GetValue(0, file)
          || !copy_from_file(file, user))
      {
        return 1;
      }
    }
    else {
      if (AppArgs::ExistsOption("list")) {
        if (!list_table(user))
          return 1;
      }
      else if (AppArgs::ExistsOption("remove")) {
        if (!remove_table(user))
          return 1;
      }
      else if (AppArgs::ExistsOption("edit")) {
        if (!edit_table(user))
          return 1;
      }
      else if (AppArgs::ExistsOption("types")) {
        list_types();
      }
      else if (AppArgs::ExistsOption("reload")) {
        if (!reload_table(user))
          return 1;
      }
      else {
        fprintf(stderr, "invalid usage\n");
        return 1;
      }
    }

    return 0;

  } catch (InotifyException e) {
    fprintf(stderr, "*** unhandled exception occurred ***\n");
    fprintf(stderr, "%s\n", e.GetMessage().c_str());
    fprintf(stderr, "error: (%i) %s\n", e.GetErrorNumber(), strerror(e.GetErrorNumber()));

    return 1;
  }
}
