#include <log.h>

#define MAX_CALLBACKS 32

typedef struct
{
    log_LogFn fn;
    void *udata;
    int level;
} Callback;

static struct
{
    void *udata;
    log_LockFn lock;
    int level;
    bool quiet;
    Callback callbacks[MAX_CALLBACKS];
} L;

static const char *level_strings[] = {
    "DEBUG", "INFO", "WARN", "ERROR"};

static const char *level_colors[] = {
    "\x1b[36m", "\x1b[32m", "\x1b[33m", "\x1b[31m"};

static void stdout_callback(log_Event *ev)
{
    char buf[16];
    buf[strftime(buf, sizeof(buf), "%H:%M:%S", ev->time)] = '\0';
    fprintf(
        (FILE *)ev->udata, "%s %s%-5s\x1b[0m \x1b[90m%s:%d:\x1b[0m ",
        buf, level_colors[ev->level], level_strings[ev->level],
        ev->file, ev->line);

    vfprintf((FILE *)ev->udata, ev->fmt, ev->ap);
    fprintf((FILE *)ev->udata, "\n");
    fflush((FILE *)ev->udata);
}

static void lock(void)
{
    if (L.lock)
    {
        L.lock(true, L.udata);
    }
}

static void unlock(void)
{
    if (L.lock)
    {
        L.lock(false, L.udata);
    }
}

const char *log_level_string(int level)
{
    return level_strings[level];
}

void log_set_lock(log_LockFn fn, void *udata)
{
    L.lock = fn;
    L.udata = udata;
}

void log_set_level(int level)
{
    L.level = level;
}

void log_set_quiet(bool enable)
{
    L.quiet = enable;
}

static void init_event(log_Event *ev, void *udata)
{
    if (!ev->time)
    {
        time_t t = time(NULL);
        ev->time = localtime(&t);
    }
    ev->udata = udata;
}

void log_log(int level, const char *file, int line, const char *fmt, ...)
{
    log_Event ev = {
        .fmt = fmt,
        .file = file,
        .line = line,
        .level = level,
    };

    lock();

    if (!L.quiet && level >= L.level)
    {
        init_event(&ev, stderr);
        va_start(ev.ap, fmt);
        stdout_callback(&ev);
        va_end(ev.ap);
    }

    unlock();
}