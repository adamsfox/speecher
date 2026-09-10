// speecher-keywatchd: reads the keyboards as root at startup, then permanently
// drops to the speecher-keywatch system user, empties its capability bounding
// set, sets no-new-privs and installs a seccomp filter with no openat/execve/
// socket. A client names one allowlisted key; the daemon thereafter reports
// only whether that one key is down. Every other key's events are read and
// dropped inside this process. See keywatch-security-design.md.
//
// Plain C++, no Qt, no D-Bus. libsystemd only, for the activated socket.

#include "KeywatchProtocol.h"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <grp.h>
#include <linux/audit.h>
#include <linux/capability.h>
#include <linux/filter.h>
#include <linux/input.h>
#include <linux/seccomp.h>
#include <pwd.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <systemd/sd-daemon.h>

namespace {

using speecher::keywatch::KeyEvent;
using speecher::keywatch::permittedKeyById;
using speecher::keywatch::PermittedKey;
using speecher::keywatch::Refusal;
using speecher::keywatch::WatchReply;
using speecher::keywatch::WatchRequest;

constexpr char systemUser[] = "speecher-keywatch";
constexpr int maxClients = 16;
constexpr int idleExitSeconds = 30;
// Per-uid connection budget: no more than this many WATCH attempts in the
// window, so the allowlist cannot be rebuilt out of repeated connects.
constexpr int rateLimitConnects = 8;
constexpr int rateLimitWindowSec = 60;

void logLine(const std::string &text)
{
    const std::string line = "speecher-keywatchd: " + text + "\n";
    (void)!write(STDERR_FILENO, line.data(), line.size());
}

// Startup only: enumerate /dev/input/event*, keep the keyboards. Opened
// read-only and O_CLOEXEC; these fds survive the drop, which is what lets the
// daemon keep reading without keeping the right to open anything new.
std::vector<int> openKeyboards()
{
    std::vector<int> keyboards;
    DIR *dir = opendir("/dev/input");
    if (!dir) {
        return keyboards;
    }
    while (dirent *entry = readdir(dir)) {
        if (std::strncmp(entry->d_name, "event", 5) != 0) {
            continue;
        }
        const std::string path = std::string("/dev/input/") + entry->d_name;
        const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }
        unsigned long evBits = 0;
        unsigned char keyBits[(KEY_MAX / 8) + 1] = {};
        // A keyboard advertises EV_KEY and the letter range; a mouse or tablet
        // advertising EV_KEY without letters is rejected.
        const bool hasKeyEv = ioctl(fd, EVIOCGBIT(0, sizeof(evBits)), &evBits) >= 0
            && (evBits & (1UL << EV_KEY));
        bool looksLikeKeyboard = false;
        if (hasKeyEv && ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) >= 0) {
            looksLikeKeyboard = (keyBits[KEY_A / 8] & (1 << (KEY_A % 8)))
                && (keyBits[KEY_Z / 8] & (1 << (KEY_Z % 8)));
        }
        if (looksLikeKeyboard) {
            keyboards.push_back(fd);
        } else {
            close(fd);
        }
    }
    closedir(dir);
    return keyboards;
}

bool dropPrivileges()
{
    const passwd *account = getpwnam(systemUser);
    if (!account) {
        logLine(std::string("system user ") + systemUser + " does not exist");
        return false;
    }
    if (setgroups(0, nullptr) != 0
        || setresgid(account->pw_gid, account->pw_gid, account->pw_gid) != 0
        || setresuid(account->pw_uid, account->pw_uid, account->pw_uid) != 0) {
        logLine("could not drop to the system user");
        return false;
    }
    // A regained-root check: setuid back must fail now.
    if (setuid(0) == 0) {
        logLine("privilege drop did not stick");
        return false;
    }
    for (int capability = 0; capability <= CAP_LAST_CAP; ++capability) {
        prctl(PR_CAPBSET_DROP, capability, 0, 0, 0);
    }
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
        logLine("could not set no-new-privs");
        return false;
    }
    return true;
}

// The read loop's allowlist. openat, execve, socket, ptrace, clone and the
// rest are absent, so a code-execution bug after the drop yields the keyboards
// already open and nothing else: no new file, no process, no network. The
// arithmetic/memory syscalls (brk, mmap, futex, rt_sigreturn) are here because
// glibc needs them to run at all; none of them opens a file, execs or reaches
// the network. getsockopt and write extend the design's illustrative list for
// SO_PEERCRED and journal lines respectively.
bool installSeccomp()
{
#if defined(__x86_64__)
    constexpr std::uint32_t audit_arch = AUDIT_ARCH_X86_64;
#elif defined(__aarch64__)
    constexpr std::uint32_t audit_arch = AUDIT_ARCH_AARCH64;
#else
#error "speecher-keywatchd seccomp filter has no rule for this architecture"
#endif
    static const int allowed[] = {
        SYS_read, SYS_write, SYS_close, SYS_epoll_wait, SYS_epoll_pwait,
        SYS_epoll_ctl, SYS_accept4, SYS_getsockopt, SYS_recvmsg, SYS_sendmsg,
        SYS_clock_gettime, SYS_clock_nanosleep, SYS_exit, SYS_exit_group,
        SYS_rt_sigreturn, SYS_rt_sigprocmask, SYS_futex, SYS_brk, SYS_mmap,
        SYS_munmap, SYS_mprotect, SYS_restart_syscall,
#ifdef SYS_epoll_pwait2
        SYS_epoll_pwait2,
#endif
#ifdef SYS_newfstatat
        SYS_newfstatat,
#endif
    };
    std::vector<sock_filter> program;
    program.push_back(BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, arch)));
    program.push_back(BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, audit_arch, 1, 0));
    program.push_back(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS));
    program.push_back(BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, nr)));
    for (const int syscall : allowed) {
        program.push_back(BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, std::uint32_t(syscall), 0, 1));
        program.push_back(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW));
    }
    program.push_back(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS));

    sock_fprog fprog{};
    fprog.len = static_cast<unsigned short>(program.size());
    fprog.filter = program.data();
    if (syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &fprog) != 0) {
        logLine("could not install the seccomp filter");
        return false;
    }
    return true;
}

std::uint64_t monotonicUsec()
{
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return std::uint64_t(now.tv_sec) * 1000000 + std::uint64_t(now.tv_nsec) / 1000;
}

struct Client {
    int fd = -1;
    uid_t uid = 0;
    pid_t pid = 0;
    std::uint16_t evdev = 0; // 0 until a WATCH is accepted.
    bool down = false;
};

struct RateBucket {
    uid_t uid = 0;
    int count = 0;
    time_t windowStart = 0;
};

class Server {
public:
    Server(int listenFd, std::vector<int> keyboards)
        : m_listenFd(listenFd)
        , m_keyboards(std::move(keyboards))
    {
    }

    int run()
    {
        m_epoll = epoll_create1(EPOLL_CLOEXEC);
        if (m_epoll < 0) {
            return 1;
        }
        addToEpoll(m_listenFd);
        for (const int fd : m_keyboards) {
            addToEpoll(fd);
        }
        std::array<epoll_event, 32> events{};
        while (true) {
            const int timeoutMs = m_clientCount == 0 ? idleExitSeconds * 1000 : -1;
            const int ready = epoll_wait(m_epoll, events.data(), events.size(), timeoutMs);
            if (ready == 0 && m_clientCount == 0) {
                return 0; // Idle: exit so nothing privileged-adjacent lingers.
            }
            if (ready < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return 1;
            }
            for (int index = 0; index < ready; ++index) {
                dispatch(events[index].data.fd);
            }
        }
    }

private:
    void addToEpoll(int fd) const
    {
        epoll_event event{};
        event.events = EPOLLIN;
        event.data.fd = fd;
        epoll_ctl(m_epoll, EPOLL_CTL_ADD, fd, &event);
    }

    void dispatch(int fd)
    {
        if (fd == m_listenFd) {
            acceptClient();
            return;
        }
        for (const int keyboard : m_keyboards) {
            if (fd == keyboard) {
                readKeyboard(keyboard);
                return;
            }
        }
        for (Client &client : m_clients) {
            if (client.fd == fd) {
                readClient(client);
                return;
            }
        }
    }

    bool rateLimited(uid_t uid)
    {
        const time_t now = time(nullptr);
        RateBucket *existing = nullptr;
        RateBucket *free = nullptr;
        for (RateBucket &bucket : m_rate) {
            if (bucket.count > 0 && now - bucket.windowStart >= rateLimitWindowSec) {
                bucket = RateBucket{}; // The window closed; the bucket is free again.
            }
            if (bucket.count > 0 && bucket.uid == uid) {
                existing = &bucket;
            } else if (bucket.count == 0 && !free) {
                free = &bucket;
            }
        }
        if (existing) {
            if (existing->count >= rateLimitConnects) {
                return true;
            }
            existing->count += 1;
            return false;
        }
        if (free) {
            free->uid = uid;
            free->windowStart = now;
            free->count = 1;
        }
        return false; // No free bucket: fail open rather than lock everyone out.
    }

    void acceptClient()
    {
        const int fd = accept4(m_listenFd, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd < 0) {
            return;
        }
        ucred credentials{};
        socklen_t length = sizeof(credentials);
        if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &length) != 0) {
            close(fd);
            return;
        }
        if (m_clientCount >= maxClients || rateLimited(credentials.uid)) {
            const WatchReply reply{speecher::keywatch::protocolVersion,
                                   std::uint8_t(Refusal::TooManyRequests)};
            (void)!write(fd, &reply, sizeof(reply));
            logLine("refused uid " + std::to_string(credentials.uid) + ": rate limited");
            close(fd);
            return;
        }
        for (Client &client : m_clients) {
            if (client.fd == -1) {
                client = Client{};
                client.fd = fd;
                client.uid = credentials.uid;
                client.pid = credentials.pid;
                addToEpoll(fd);
                m_clientCount += 1;
                return;
            }
        }
        close(fd);
    }

    void dropClient(Client &client)
    {
        epoll_ctl(m_epoll, EPOLL_CTL_DEL, client.fd, nullptr);
        close(client.fd);
        client = Client{};
        m_clientCount -= 1;
    }

    void readClient(Client &client)
    {
        WatchRequest request{};
        const ssize_t got = read(client.fd, &request, sizeof(request));
        if (got <= 0) {
            dropClient(client);
            return;
        }
        // One watch per peer: a second WATCH on the same connection is refused.
        if (client.evdev != 0) {
            const WatchReply reply{speecher::keywatch::protocolVersion,
                                   std::uint8_t(Refusal::AlreadyWatching)};
            (void)!write(client.fd, &reply, sizeof(reply));
            return;
        }
        Refusal refusal = Refusal::None;
        const PermittedKey *key = nullptr;
        if (request.version != speecher::keywatch::protocolVersion) {
            refusal = Refusal::BadVersion;
        } else if (!(key = permittedKeyById(request.keyId))) {
            refusal = Refusal::KeyNotPermitted;
        }
        const WatchReply reply{speecher::keywatch::protocolVersion, std::uint8_t(refusal)};
        (void)!write(client.fd, &reply, sizeof(reply));
        if (refusal != Refusal::None) {
            logLine("refused uid " + std::to_string(client.uid) + " pid "
                    + std::to_string(client.pid) + " key " + std::to_string(request.keyId));
            dropClient(client);
            return;
        }
        client.evdev = key->evdev;
        logLine("watching for uid " + std::to_string(client.uid) + " pid "
                + std::to_string(client.pid) + " key " + std::string(key->code));
    }

    void readKeyboard(int fd)
    {
        input_event event{};
        ssize_t got = 0;
        while ((got = read(fd, &event, sizeof(event))) == sizeof(event)) {
            if (event.type != EV_KEY || event.value == 2) {
                continue; // value 2 is auto-repeat, which is not a transition.
            }
            report(event.code, event.value == 1);
        }
    }

    // The only thing that ever leaves the daemon: down/up for a key a client
    // already asked for, with a monotonic timestamp and no key identity.
    void report(std::uint16_t evdev, bool down)
    {
        for (Client &client : m_clients) {
            if (client.fd == -1 || client.evdev != evdev || client.down == down) {
                continue;
            }
            client.down = down;
            KeyEvent message{};
            message.down = down ? 1 : 0;
            message.monotonicUsec = monotonicUsec();
            if (write(client.fd, &message, sizeof(message)) != sizeof(message)) {
                dropClient(client);
            }
        }
    }

    int m_listenFd;
    std::vector<int> m_keyboards;
    int m_epoll = -1;
    std::array<Client, maxClients> m_clients{};
    std::array<RateBucket, maxClients * 2> m_rate{};
    int m_clientCount = 0;
};

} // namespace

int main()
{
    const int listenCount = sd_listen_fds(0);
    if (listenCount != 1) {
        logLine("expected exactly one socket-activation fd from systemd");
        return 1;
    }
    const int listenFd = SD_LISTEN_FDS_START;

    std::vector<int> keyboards = openKeyboards();
    if (keyboards.empty()) {
        logLine("found no keyboard devices to watch");
        return 1;
    }

    if (!dropPrivileges() || !installSeccomp()) {
        return 1;
    }

    Server server(listenFd, std::move(keyboards));
    return server.run();
}
