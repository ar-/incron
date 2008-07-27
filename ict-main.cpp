
/// inotify cron table manipulator main file
/**
 * \file ict-main.cpp
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
 

#include <argp.h>
#include <pwd.h>
#include <string>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/inotify.h>

#include "incron.h"
#include "incrontab.h"

// Alternative editor
#define INCRON_ALT_EDITOR "/etc/alternatives/editor"

// #define INCRON_DEFAULT_EDITOR "nano" // for vim haters like me ;-)
#define INCRON_DEFAULT_EDITOR "vim"


const char* argp_program_version = INCRON_TAB_NAME " " INCRON_VERSION;
const char* argp_program_bug_address = INCRON_BUG_ADDRESS;

static char doc[] = "incrontab - incron table manipulator\n(c) Lukas Jelinek, 2006, 2007";

static char args_doc[] = "FILE";

static struct argp_option options[] = {
  {"list",    'l', 0,      0,  "List the current table" },
  {"remove",  'r', 0,      0,  "Remove the table completely" },
  {"edit",    'e', 0,      0,  "Edit the table" },
  {"types",   't', 0,      0,  "List all supported event types" },
  {"user",    'u', "USER", 0,  "Override the current user" },
  { 0 }
};

/// incrontab operations
typedef enum
{
  OPER_NONE,    /// nothing
  OPER_LIST,    /// list table
  OPER_REMOVE,  /// remove table
  OPER_EDIT,    /// edit table
  OPER_TYPES    /// list event types
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
    case 't':
      arguments->oper = OPER_TYPES;
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
 */
bool list_table(const char* user)
{
  std::string tp(InCronTab::GetUserTablePath(user));
  
  if (euidaccess(tp.c_str(), R_OK) != 0) {
    if (errno == ENOENT) {
      fprintf(stderr, "no table for %s\n", user);
      return true;
    }
    else {
      fprintf(stderr, "cannot read table for %s: %s\n", user, strerror(errno));
      return false;
    }
  }
  
  FILE* f = fopen(tp.c_str(), "r");
  if (f == NULL)
    return false;
    
  char s[1024];
  while (fgets(s, 1024, f) != NULL) {
    fputs(s, stdout);
  }
  
  fclose(f);
  
  return true;
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
  uid_t gid = ppwd->pw_gid;
  
  char s[NAME_MAX];
  strcpy(s, "/tmp/incron.table-XXXXXX");
  
  uid_t iu = geteuid();
  uid_t ig = getegid();

  if (setegid(gid) != 0 || seteuid(uid) != 0) {
    fprintf(stderr, "cannot change effective UID/GID for user %s: %s\n", user, strerror(errno));
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
        fprintf(stderr, "cannot get empty table for %s: %s\n", user, strerror(errno));
        fclose(out);
        goto end;
      }
    }
    else {
      fprintf(stderr, "cannot read old table for %s: %s\n", user, strerror(errno));
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
  // 3. Check presence of /etc/alternatives/editor
  // 4. Use hard-wired editor
  e = getenv("EDITOR");
  if (e == NULL) {
    e = getenv("VISUAL");
    if (e == NULL) {
      if (access(INCRON_ALT_EDITOR, X_OK) == 0)
        e = INCRON_ALT_EDITOR;
      else
        e = INCRON_DEFAULT_EDITOR;
    }
  }
  
  // this block is explicite due to gotos' usage simplification
  {
    pid_t pid = fork();
    if (pid == 0) {
      if (setgid(gid) != 0 || setuid(uid) != 0) {
        fprintf(stderr, "cannot set user %s: %s\n", user, strerror(errno));
        goto end;
      }    
      
      execlp(e, e, s, NULL);
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
    InCronTab ict;
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
    case OPER_TYPES:
      list_types();
      break;
    default:
      fprintf(stderr, "invalid usage\n");
      return 1;
  }
  
  return 0;
}
