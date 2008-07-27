
/// inotify cron table manipulator main file
/**
 * \file ict-main.cpp
 * 
 * inotify cron system
 * 
 * Copyright (C) 2006 Lukas Jelinek, <lukas@aiken.cz>
 * 
 * This program is free software; you can use it, redistribute
 * it and/or modify it under the terms of the GNU General Public
 * License, version 2 (see LICENSE-GPL).
 *  
 */
 

#include <argp.h>
#include <pwd.h>
#include <string>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "incron.h"
#include "incrontab.h"


// #define INCRON_DEFAULT_EDITOR "nano" // for vim haters like me ;-)
#define INCRON_DEFAULT_EDITOR "vim"


const char* argp_program_version = INCRON_TAB_NAME " " INCRON_VERSION;
const char* argp_program_bug_address = INCRON_BUG_ADDRESS;

static char doc[] = "incrontab - incron table manipulator";

static char args_doc[] = "FILE";

static struct argp_option options[] = {
  {"list",    'l', 0,      0,  "List the current table" },
  {"remove",  'r', 0,      0,  "Remove the table completely" },
  {"edit",    'e', 0,      0,  "Edit the table" },
  {"user",    'u', "USER", 0,  "Override the current user" },
  { 0 }
};

/// incrontab operations
typedef enum
{
  OPER_NONE,    /// nothing
  OPER_LIST,    /// list table
  OPER_REMOVE,  /// remove table
  OPER_EDIT     /// edit table
} InCronTab_Operation_t;

/// incrontab arguments
struct arguments
{
  char *user;     /// user name
  int oper;       /// operation code
  char *file;     /// file to import
};

/// Parses the program options (arguments).
/**
 * \param[in] key argument key (name)
 * \param[in] arg argument value
 * \param[out] state options setting
 * \return 0 on success, ARGP_ERR_UNKNOWN on unknown argument(s)
 */
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
  struct arguments* arguments = (struct arguments*) state->input;
     
  switch (key) {
    case 'l':
      arguments->oper = OPER_LIST;
      break;
    case 'r':
      arguments->oper = OPER_REMOVE;
      break;
    case 'e':
      arguments->oper = OPER_EDIT;
      break;
    case 'u':
      arguments->user = arg;
      break;
    case ARGP_KEY_ARG:
      if (state->arg_num >= 1)
        argp_usage(state);
      arguments->file = arg;
      break;
    case ARGP_KEY_END:
      break;
    default:
      return ARGP_ERR_UNKNOWN;
  }
  
  return 0;
}

/// Program arguments
static struct argp argp = { options, parse_opt, args_doc, doc };

/// Unlink a file with temporarily changed UID.
/**
 * \param[in] file file to unlink
 * \param[in] uid UID for unlink processing
 * 
 * \attention No error checking is done!
 */
void unlink_suid(const char* file, uid_t uid)
{
  uid_t iu = geteuid();
  seteuid(uid);
  if (unlink(file) != 0)
    perror("cannot remove temporary file");
  seteuid(iu);
}

/// Copies a file to an user table.
/**
 * \param[in] path path to file
 * \param[in] user user name
 * \return true = success, false = failure
 */
bool copy_from_file(const char* path, const char* user)
{
  InCronTab tab;
  std::string s(path);
  if (s == "-")
    s = "/dev/stdin";
  if (!tab.Load(s)) {
    fprintf(stderr, "cannot load table from file: %s\n", path);
    return false;
  }
  
  std::string out(InCronTab::GetUserTablePath(user));
  if (!tab.Save(out)) {
    fprintf(stderr, "cannot create table for user: %s\n", user);
    return false;
  }
  
  return true;
}

/// Removes an user table.
/**
 * \param[in] user user name
 * \return true = success, false = failure
 */ 
bool remove_table(const char* user)
{
  std::string tp(InCronTab::GetUserTablePath(user));
 
  if (unlink(tp.c_str()) != 0 && errno != ENOENT) {
    fprintf(stderr, "cannot remove table for user: %s\n", user);
    return false;
  }
  
  return true;
}

/// Lists an user table.
/**
 * \param[in] user user name
 * \return true = success, false = failure
 * 
 * \attention Listing is currently done through 'cat'.
 */
bool list_table(const char* user)
{
  std::string tp(InCronTab::GetUserTablePath(user));
  
  if (access(tp.c_str(), R_OK) != 0) {
    if (errno == ENOENT) {
      fprintf(stderr, "no table for %s\n", user);
      return true;
    }
    else {
      fprintf(stderr, "cannot read table for %s: %s\n", user, strerror(errno));
      return false;
    }
  }
  
  std::string cmd("cat ");
  cmd.append(tp);
  return system(cmd.c_str()) == 0;
}

/// Allows to edit an user table.
/**
 * \param[in] user user name
 * \return true = success, false = failure
 * 
 * \attention This function is very complex and may contain
 *            various bugs including security ones. Please keep
 *            it in mind..
 */
bool edit_table(const char* user)
{
  std::string tp(InCronTab::GetUserTablePath(user));
  
  struct passwd* ppwd = getpwnam(user);
  if (ppwd == NULL) {
    fprintf(stderr, "cannot find user %s: %s\n", user, strerror(errno));
    return false;
  }
  uid_t uid = ppwd->pw_uid;
  
  char s[NAME_MAX];
  strcpy(s, "/tmp/incron.table-XXXXXX");
  
  uid_t iu = geteuid();

  if (seteuid(uid) != 0) {
    fprintf(stderr, "cannot change effective UID to %i: %s\n", (int) uid, strerror(errno));
    return false;
  }
  
  int fd = mkstemp(s);
  if (fd == -1) {
    fprintf(stderr, "cannot create temporary file: %s\n", strerror(errno));
    return false;
  }
  
  if (fchmod(fd, 0644) != 0) {
    fprintf(stderr, "cannot change mode of temporary file: %s\n", strerror(errno));
    close(fd);
    unlink_suid(s, uid);
    return false;
  }
  
  if (seteuid(iu) != 0) {
    fprintf(stderr, "cannot change effective UID: %s\n", strerror(errno));
    close(fd);
    unlink_suid(s, uid);
    return false;
  }
    
  FILE* out = fdopen(fd, "w");
  if (out == NULL) {
    fprintf(stderr, "cannot write to temporary file: %s\n", strerror(errno));
    close(fd);
    unlink_suid(s, uid);
    return false;
  }
  
  FILE* in = fopen(tp.c_str(), "r");
  if (in == NULL) {
    if (errno == ENOENT) {
      in = fopen("/dev/null", "r");
      if (in == NULL) {
        fprintf(stderr, "cannot get empty table for %s: %s\n", user, strerror(errno));
        fclose(out);
        unlink_suid(s, uid);
        return false;
      }
    }
    else {
      fprintf(stderr, "cannot read old table for %s: %s\n", user, strerror(errno));
      fclose(out);
      unlink_suid(s, uid);
      return false;
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
    unlink_suid(s, uid);
    return false;
  }
  
  time_t mt = st.st_mtime;
  
  const char* e = getenv("EDITOR");
  if (e == NULL)
    e = INCRON_DEFAULT_EDITOR;
  
  pid_t pid = fork();
  if (pid == 0) {
    if (setuid(uid) != 0) {
      fprintf(stderr, "cannot set user %s: %s\n", user, strerror(errno));
      return false;
    }    
    
    execlp(e, e, s, NULL);
    _exit(1);
  }
  else if (pid > 0) {
    int status;
    if (wait(&status) != pid) {
      perror("error while waiting for editor");
      unlink_suid(s, uid);
      return false;
    }
    if (!(WIFEXITED(status)) || WEXITSTATUS(status) != 0) {
      perror("editor finished with error");
      unlink_suid(s, uid);
      return false;
    }
  }
  else {
    perror("cannot start editor");
    unlink_suid(s, uid);
    return false;
  }
  
  if (stat(s, &st) != 0) {
    fprintf(stderr, "cannot stat temporary file: %s\n", strerror(errno));
    unlink_suid(s, uid);
    return false;
  }
  
  if (st.st_mtime == mt) {
    fprintf(stderr, "table unchanged\n");
    unlink_suid(s, uid);
    return true;
  }
  
  InCronTab ict;
  if (!ict.Load(s) || !ict.Save(tp)) {
    fprintf(stderr, "cannot move temporary table: %s\n", strerror(errno));
    unlink(s);
    return false;
  }
  
  fprintf(stderr, "table updated\n");
  
  unlink_suid(s, uid);
  return true;
}


int main(int argc, char** argv)
{
  struct arguments arguments;
  
  arguments.user = NULL;
  arguments.oper = OPER_NONE;
  arguments.file = NULL;
  
  argp_parse (&argp, argc, argv, 0, 0, &arguments);
  
  if (arguments.file != NULL && arguments.oper != OPER_NONE) {
    fprintf(stderr, "invalid arguments - specify source file or operation\n");
    return 1;
  }
  if (arguments.file == NULL && arguments.oper == OPER_NONE) {
    fprintf(stderr, "invalid arguments - specify source file or operation\n");
    return 1;
  }
  
  uid_t uid = getuid();
  
  if (uid != 0 && arguments.user != NULL) {
    fprintf(stderr, "cannot access table for user %s: permission denied\n", arguments.user);
    return 1;
  }
  
  struct passwd pwd;
  
  if (arguments.user == NULL) {
    struct passwd* ppwd = getpwuid(uid);
    if (ppwd == NULL) {
      fprintf(stderr, "cannot determine current user\n");
      return 1;
    }
    memcpy(&pwd, ppwd, sizeof(pwd));
    arguments.user = pwd.pw_name;
  }
  else if (getpwnam(arguments.user) == NULL) {
    fprintf(stderr, "user %s not found\n", arguments.user);
    return 1;
  }
  
  if (!InCronTab::CheckUser(arguments.user)) {
    fprintf(stderr, "user %s is not allowed to use incron\n", arguments.user);
    return 1;
  }
  
  switch (arguments.oper) {
    case OPER_NONE:
      fprintf(stderr, "copying table from file: %s\n", arguments.file);
      if (!copy_from_file(arguments.file, arguments.user))
        return 1;
      break;
    case OPER_LIST:
      if (!list_table(arguments.user))
        return 1;
      break;
    case OPER_REMOVE:
      fprintf(stderr, "removing table for user %s\n", arguments.user);
      if (!remove_table(arguments.user))
        return 1;
      break;
    case OPER_EDIT:
      if (!edit_table(arguments.user))
        return 1;
      break;
    default:
      fprintf(stderr, "invalid usage\n");
      return 1;
  }
  
  return 0;
}
