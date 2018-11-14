/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "config.hpp"
#include "errno.hpp"
#include "fileinfo.hpp"
#include "fs_base_chmod.hpp"
#include "fs_path.hpp"
#include "rv.hpp"
#include "rwlock.hpp"
#include "ugid.hpp"

#include <fuse.h>

#include <string>
#include <vector>

using std::string;
using std::vector;
using mergerfs::Policy;
using mergerfs::Config;
using mergerfs::rwlock::ReadGuard;

namespace local
{
  static
  int
  chmod_loop_core(const string *basepath_,
                  const char   *fusepath_,
                  const mode_t  mode_,
                  const int     error_)
  {
    int rv;
    string fullpath;

    fullpath = fs::path::make(basepath_,fusepath_);

    rv = fs::chmod(fullpath,mode_);

    return error::calc(rv,error_,errno);
  }

  static
  int
  chmod_loop(const vector<const string*> &basepaths_,
             const char                  *fusepath_,
             const mode_t                 mode_)
  {
    int error;

    error = -1;
    for(size_t i = 0, ei = basepaths_.size(); i != ei; i++)
      {
        error = local::chmod_loop_core(basepaths_[i],fusepath_,mode_,error);
      }

    return -error;
  }

  static
  int
  chmod(Policy::Func::Action  actionFunc_,
        const Branches       &branches_,
        const uint64_t        minfreespace_,
        const char           *fusepath_,
        const mode_t          mode_)
  {
    int rv;
    vector<const string*> basepaths;

    rv = actionFunc_(branches_,fusepath_,minfreespace_,basepaths);
    if(rv == -1)
      return -errno;

    return local::chmod_loop(basepaths,fusepath_,mode_);
  }

  static
  int
  chmod(const uid_t   uid_,
        const gid_t   gid_,
        const Config *config_,
        const char   *fusepath_,
        const mode_t  mode_)
  {
    const ugid::Set ugid(uid_,gid_);
    const ReadGuard readlock(&config_->branches_lock);

    return local::chmod(config_->chmod,
                        config_->branches,
                        config_->minfreespace,
                        fusepath_,
                        mode_);
  }

  static
  int
  fchmod(const fuse_file_info *ffi_,
         const mode_t          mode_)
  {
    int rv;
    FileInfo *fi;

    fi = reinterpret_cast<FileInfo*>(ffi_->fh);

    rv = fs::fchmod(fi->fd,mode_);

    return ((rv == -1) ? -errno : 0);
  }
}

namespace mergerfs
{
  namespace fuse
  {
    int
    chmod(const char     *fusepath,
          mode_t          mode,
          fuse_file_info *ffi)
    {
      const fuse_context *fc     = fuse_get_context();
      const Config       &config = Config::get(fc);

      if((fusepath == NULL) && (ffi != NULL))
        return local::fchmod(ffi,mode);

      return local::chmod(fc->uid,
                          fc->gid,
                          &config,
                          fusepath,
                          mode);
    }
  }
}
