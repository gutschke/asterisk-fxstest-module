#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <asterisk.h>
ASTERISK_FILE_VERSION(__FILE__, "1.0")
#include <asterisk/cli.h>
#include <asterisk/module.h>
#include <asterisk/logger.h>
#include <dahdi/user.h>
#include <dahdi/wctdm_user.h>


static void fxs_stat(char **out, int channel, const char *dev) {
  errno = 0;
  int fd, dev_fd = open(dev, O_RDONLY|O_NOFOLLOW);
  if (dev_fd >= 0) {
    fd = dev_fd;
  } else {
    fd = -1;
    int findfd(const char *path, const struct stat *sb, int typeflag,
               struct FTW *ftwbuf) {
      char buf[20];
      ssize_t rc = readlink(path, buf, sizeof(buf));
      if (rc > 0 && rc < (ssize_t)sizeof(buf)) {
        buf[rc] = '\000';
        if (!strcmp(buf, "/dev/dahdi/channel")) {
          fd = atoi(path + ftwbuf->base);
          int cur_channel = -1;
          if (!ioctl(fd, DAHDI_CHANNO, &cur_channel) && cur_channel == channel) {
            return -1;
          }
          fd = -1;
        }
      }
      return 0;
    }

    nftw("/proc/self/fd", findfd, 4, FTW_MOUNT|FTW_PHYS);
  }
  if (fd < 0) {
    ast_log(LOG_NOTICE, "Device busy: \"%s\"\n", dev);
  } else {
    struct wctdm_stats stats = { 0 };
    if (!ioctl(fd, WCTDM_GET_STATS, &stats)) {
      char *tmp = realloc(*out, (*out ? strlen(*out) : 0) + 80);
      if (tmp != NULL) {
        if (*out == NULL) {
          *tmp = '\000';
        }
        *out = tmp;
        sprintf(strrchr(tmp, '\000'),
                "Channel #%d, TIP: %3d.%dV, RING: %3d.%dV, BAT: %3d.%dV\n",
                channel,
                stats.tipvolt /1000, abs(stats.tipvolt )%1000/100,
                stats.ringvolt/1000, abs(stats.ringvolt)%1000/100,
                stats.batvolt /1000, abs(stats.batvolt )%1000/100);
      }
    } else {
      ast_log(LOG_NOTICE, "Unable to obtain stats\n");
    }
    if (dev_fd >= 0) {
      close(dev_fd);
    }
  }
}

static int devcmp(const void *a, const void *b, void *c) {
  return strcmp((const char *)b, (const char *)a);
}

static char *handle_cli_fxstest(struct ast_cli_entry *e, int cmd,
                                struct ast_cli_args *a) {
  switch (cmd) {
  case CLI_INIT:
    e->command = "fxstest";
    e->usage = "Usage: fxstest <num>\n"
               "       Print information about FXS interfaces.\n"
               "Examples:\n"
               "       fxstest\n"
               "       fxstest 1\n";
    return NULL;
  case CLI_GENERATE:
    return NULL;
  }

  int channel = 0;
  switch (a->argc) {
  case 0:
  case 1:
    break;
  case 2:
    errno = 0;
    char *endptr = NULL;
    long res = strtol(a->argv[1], &endptr, 10);
    if (errno == 0 && endptr && !*endptr && res > 0) {
      channel = (int)res;
      break;
    }
    // Fall through on error
  default:
    ast_cli(a->fd, "Illegal arguments\n");
    return NULL;
  }

  char **devices   = NULL;
  size_t n_devices = 0;
  int traverse(const char *path, const struct stat *sb, int typeflag,
               struct FTW *ftwbuf) {
    if (typeflag == FTW_F && S_ISCHR(sb->st_mode)) {
      char **tmp = realloc(devices, sizeof(char *)*(n_devices+1));
      if (!tmp) {
        return -1;
      }
      devices = tmp;
      if (!(devices[n_devices++] = strdup(path))) {
        return -1;
      }
    }
    return 0;
  }

  char *out = NULL;
  if (nftw("/dev/dahdi/chan", traverse, 4, FTW_MOUNT|FTW_PHYS) != -1 &&
      n_devices > 0) {
    qsort_r(devices, n_devices, sizeof(char *), devcmp, NULL);
    if (!channel) {
      for (size_t i = 0; i < n_devices; ++i) {
        fxs_stat(&out, i+1, devices[i]);
      } 
    } else if (channel > 0 && (size_t)channel <= n_devices) {
      fxs_stat(&out, channel, devices[channel-1]);
    } else {
      ast_log(LOG_NOTICE, "No such device found\n");
    }
  }
  if (out) {
    ast_cli(a->fd, "%s", out);
    free(out);
  }

  for (size_t i = 0; i < n_devices; ++i) {
    free(devices[i]);
  }
  free(devices);

  return CLI_SUCCESS;
}

static struct ast_cli_entry cli_fxstest[] = {
  AST_CLI_DEFINE(handle_cli_fxstest, "Test the FXS interface"),
};

static int load_module(void) {
  ast_cli_register_multiple(cli_fxstest, ARRAY_LEN(cli_fxstest));
  return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void) {
  ast_cli_unregister_multiple(cli_fxstest, ARRAY_LEN(cli_fxstest));
  return 0;
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "FXS Test");
