/*
  Copyright (c) 2010, Gilles BERNARD lordikc at free dot fr
  All rights reserved.
  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  * Neither the name of the Author nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  Modified by jaseg <s@jaseg.de> (c) 2011
*/
#include <fuse.h>
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <utime.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>
#include <regex.h>
#include <vector>


struct wofs_opt_t
{
  const char* conf;
  const char* dir;
};

struct wofs_opt_t  wofs_opt;

std::vector<regex_t> allowRules;
std::vector<regex_t> denyRules;

/* bool match(const char* path,const std::vector<regex_t>& rules)
 * Test if path match one of the rules
 * Return true if one match was found
 * path: Path of the file or directory
 * rules: Set of rules to test
 */
bool match(const char* path,const std::vector<regex_t>& rules)
{
  for(std::vector<regex_t>::const_iterator iter=rules.begin();
      iter!=rules.end(); iter++)
    {
      regex_t r=*iter;
      if(!regexec(&r,path,0,NULL,0))
	return true;
    }
  return false;
}

/* static char* wofs_add_path(const char* path)
 * Concat base directory and the given relative path. The returned pointer must be freed by caller.
 * path: Path in the fuse file system.
 */
static char* wofs_add_path(const char* path)
{
  char *fullPath = (char*)malloc(sizeof(char)*(strlen(path)+strlen(wofs_opt.dir)+1));

  strcpy(fullPath,wofs_opt.dir);
  strcat(fullPath,path);

  return fullPath;
}


static int wofs_getattr(const char *path,
                        struct stat *st_data) 
{
  char *fullPath=wofs_add_path(path);

  if (!fullPath) { return -ENOMEM;}

  int res = lstat(fullPath, st_data);
  free(fullPath);
  if(res == -1) 
    {
      return -errno;
    }

  return 0;
}


static int wofs_readlink(const char *path, char *buf, size_t size) 
{
  return -EPERM;
}

static int wofs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			off_t offset, struct fuse_file_info *fi)
{
  // Ignore silently
  return 0;
}

static int wofs_mknod(const char *path, mode_t mode, dev_t rdev) 
{
  return -EPERM;
}

static int wofs_mkdir(const char *path,
		      mode_t mode) 
{
  if(match(path,denyRules))
    {
      fprintf(stderr,"%s denied\n",path);
      return -EPERM;
    }
  if(allowRules.size()&&(!match(path,allowRules)))
    {
      fprintf(stderr,"%s not allowed\n",path);
      return -EPERM;
    }
  
  char *fullPath=wofs_add_path(path);
  int res = mkdir(fullPath, mode|S_IRUSR|S_IWUSR|S_IXUSR);
  if(res == -1)
    return -errno;
  return 0;
}

static int wofs_unlink(const char *path) 
{
  return -EPERM;
}

static int wofs_rmdir(const char *path) 
{
  return -EPERM;
}

static int wofs_rename(const char *from,
		       const char *to) 
{
  return -EPERM;
}

static int wofs_symlink(const char *from, const char *to)
{
  return -EPERM;
}

static int wofs_link(const char *from, const char *to) 
{
  return -EPERM;
}

static int wofs_chmod(const char *path,
		      mode_t mode) 
{
  // Ignore change mode
  return 0;
}

static int wofs_chown(const char *path,
		      uid_t uid, gid_t gid) 
{
  return -EPERM;
}

static int wofs_truncate(const char *path,
			 off_t size) 
{
  return -EPERM;
}

static int wofs_utime(const char *path, struct utimbuf *buf)
{
  return -EPERM;
}

static int wofs_open(const char *path,
		     struct fuse_file_info *finfo) 
{
  return -EPERM;
}

static int wofs_read(const char *path,
		     char *buf,
		     size_t size,
		     off_t offset,
		     struct fuse_file_info *finfo) 
{
  // Read is forbidden
  return -EPERM;
}

static int wofs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *finfo) 
{
  int res;
  res = pwrite(finfo->fh, buf, size, offset);
  if ((res==-1)||(res != size)) return -errno;
  return res;
}

static int wofs_statfs(const char *path, struct statvfs *st_buf) 
{
  int res = statvfs(wofs_opt.dir, st_buf);

  if (res == -1) {
    return -errno;
  }
  return 0;
}

static int wofs_release(const char *path, struct fuse_file_info *finfo) 
{
  return 0;
}

static int wofs_fsync(const char *path, int crap, struct fuse_file_info *finfo) {
  return 0;
}

static int wofs_access(const char *path, int mode) 
{
  // Check if it's write access and deny other operation
  if (mode & W_OK) 
    return -EPERM;

  // Check if path is allowed or denied
  if(match(path,denyRules))
    {
      fprintf(stderr,"%s denied\n",path);
      return -EPERM;
    }
  if(allowRules.size()&&(!match(path,allowRules)))
    {
      fprintf(stderr,"%s not allowed\n",path);
      return -EPERM;
    }

  // Check if we have access right from the os
  int res;
  char *fullPath=wofs_add_path(path);
  if (!fullPath) { return -ENOMEM;}

  res = access(fullPath, mode);
  free(fullPath);
  if (res == -1 ) 
    {
      return -errno;
    }
  return res;
}


static int wofs_create(const char *path,
		       mode_t mode,
		       struct fuse_file_info *fi) 
{
  // Check if path is allowed or denied
  if(match(path,denyRules))
    {
      fprintf(stderr,"%s denied\n",path);
      return -EPERM;
    }
  if(allowRules.size()&&(!match(path,allowRules)))
    {
      fprintf(stderr,"%s not allowed\n",path);
      return -EPERM;
    }

  // Create the file
  char *fullPath=wofs_add_path(path);
  int fd = open(fullPath, fi->flags, mode|S_IWUSR);
  if (fd == -1)
    return -errno;
  
  fi->fh = fd;
  return 0;
}

struct fuse_operations wofs_oper;

enum {
  KEY_HELP,
  KEY_VERSION,
};

void wofs_usage()
{
  fprintf(stderr,"%s (c) Gilles BERNARD %s\n",PACKAGE_STRING,PACKAGE_BUGREPORT);
  fprintf(stderr,"%s provides a write only filesystem with naming restriction.\n",PACKAGE_NAME);
  fprintf(stderr,"Usage : %s -o options mount_point\n",PACKAGE_NAME);
  fprintf(stderr,"  - dir=storage_directory directory where the files will be written.\n");
  fprintf(stderr,"  - conf=configuration_directory directory containing allow.rules and deny.rules.\n");
  fprintf(stderr,"From the mount point, one can only create directories and write files.\n");
  fprintf(stderr,"Attempt to read a file fail with permission denied.\n");
  fprintf(stderr,"Attempt to read a directory returns no file.\n");
  fprintf(stderr,"Unfortunately one can know if a file or a directory exist if he knows its name. But nothing can be read out of it.\n");
  fprintf(stderr,"The configuration files allow.rules and deny.rules contains one regular expression per line.\n");
  fprintf(stderr,"File or directory creation will be allowed only if its path match an allowed rule.\n");
  fprintf(stderr,"File or directory creation will be denied if its path match a denied rule.\n");
}


static int wofs_parse_opt(void *data, const char *arg, int key,
			  struct fuse_args *outargs)
{
  switch (key)
    {
    case FUSE_OPT_KEY_NONOPT:
    case FUSE_OPT_KEY_OPT:
      return 1;
    case KEY_HELP:
      wofs_usage();
      exit(1);
    default:
      wofs_usage();
      exit(1);
    }
  return 1;
}

#define WOFS_OPT(t, p, v) { t, offsetof(struct wofs_opt_t, p), v }
#undef FUSE_OPT_END
#define FUSE_OPT_END { NULL,0,0 }

static struct fuse_opt wofs_opts[] = 
  {
    WOFS_OPT("conf=%s",conf,0),
    WOFS_OPT("dir=%s",dir,0),
    FUSE_OPT_KEY("-h",          KEY_HELP),
    FUSE_OPT_KEY("--help",      KEY_HELP),
    FUSE_OPT_END
  };

/* void read_file(FILE* f, std::vector<regex_t>& rules)
 * Read a file containing one regular expression per line.
 * Read regexp are compiled and added to the vector.
 */
void read_file(FILE* f, std::vector<regex_t>& rules)
{
  char tmp[PATH_MAX];
  fgets(tmp,PATH_MAX,f);
  while(!feof(f))
    {
      char*c = strstr(tmp,"\n");
      if (c) *c='\0';
      regex_t r;
      fprintf(stderr,"   %s\n",tmp);
      regcomp(&r,tmp,REG_NOSUB|REG_EXTENDED );
      rules.insert(rules.end(),r);
      fgets(tmp,PATH_MAX,f);
    }
}

int main(int argc, char *argv[])
{
  wofs_opt.conf=PREFIX "/etc/wofs";
  wofs_opt.dir=PREFIX "/var/wofs";
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  memset(&wofs_oper,0,sizeof(wofs_oper));
  wofs_oper.mknod	= wofs_mknod;
  wofs_oper.symlink	= wofs_symlink;
  wofs_oper.unlink	= wofs_unlink;
  wofs_oper.rmdir	= wofs_rmdir;
  wofs_oper.rename	= wofs_rename;
  wofs_oper.link	= wofs_link;
  wofs_oper.chmod	= wofs_chmod;
  wofs_oper.chown	= wofs_chown;
  wofs_oper.utime	= wofs_utime;
  wofs_oper.getattr	= wofs_getattr;
  wofs_oper.readlink	= wofs_readlink;
  wofs_oper.readdir	= wofs_readdir;
  wofs_oper.mkdir	= wofs_mkdir;
  wofs_oper.truncate	= wofs_truncate;
  wofs_oper.open	= wofs_open;
  wofs_oper.read	= wofs_read;
  wofs_oper.write	= wofs_write;
  wofs_oper.statfs	= wofs_statfs;
  wofs_oper.release	= wofs_release;
  wofs_oper.fsync	= wofs_fsync;
  wofs_oper.access	= wofs_access;
  wofs_oper.create      = wofs_create;

  // Parse options
  int res = fuse_opt_parse(&args, &wofs_opt, wofs_opts, wofs_parse_opt);

  fprintf(stderr,"Configuration directory: %s\n",wofs_opt.conf);
  fprintf(stderr,"Storage directory: %s\n",wofs_opt.dir);

  struct stat stbuf;
  if(stat(wofs_opt.dir,&stbuf)!=0 || !S_ISDIR(stbuf.st_mode))
    {
      fprintf(stderr,"Storage directory %s does not exist or is not a directory.\n",wofs_opt.dir);
      wofs_usage();
      exit(1);
    }

  if(stat(wofs_opt.conf,&stbuf)!=0 || !S_ISDIR(stbuf.st_mode))
    {
      fprintf(stderr,"Configuration directory %s does not exist or is not a directory. Disabling naming restrictions.\n",wofs_opt.conf);
    }
  else
    {
      // Read file allow.rules
      char cnf[1024];
      sprintf(cnf,"%s/allow.rules",wofs_opt.conf);
      printf("%s\n",wofs_opt.conf);
      FILE* f=fopen(cnf,"r");
      if(f!=NULL)
	{
	  fprintf(stderr,"Rules from %s:\n",cnf);
	  read_file(f,allowRules);
	  fclose(f);
	}

      // Read file deny.rules
      sprintf(cnf,"%s/deny.rules",wofs_opt.conf);
      f=fopen(cnf,"r");
      if(f!=NULL)
	{
	  fprintf(stderr,"Rules from %s:\n",cnf);
	  read_file(f,denyRules);
	  fclose(f);
	}
    }
  
  return fuse_main(args.argc, args.argv, &wofs_oper, NULL);
}
