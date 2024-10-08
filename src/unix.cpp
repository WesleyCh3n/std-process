#ifndef _WIN32
#include "process.hpp"

#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <optional>
#include <span>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

using std::optional;
using std::pair;
using std::span;
using std::string;
using std::vector;

namespace FileDesc {
size_t read(int fd, span<std::byte> buffer) {
  ssize_t bytes_read;
  if ((bytes_read = ::read(fd, buffer.data(), buffer.size())) > 0) {
    return bytes_read;
  }
  return 0;
}

size_t write(int fd, span<const std::byte> buffer) {
  ssize_t written;
  if ((written = ::write(fd, buffer.data(), buffer.size())) < 0) {
    throw std::runtime_error("failed to write file to fd");
  }
  return static_cast<size_t>(written);
}

size_t read_to_end(int fd, vector<std::byte> &buffer) {
  size_t buf_init_len = size(buffer);
  vector<std::byte> tmp(2048);
  size_t tmp_size = 0;
  while ((tmp_size = read(fd, tmp)) > 0) {
    buffer.insert(end(buffer), begin(tmp), begin(tmp) + tmp_size);
  }
  return size(buffer) - buf_init_len;
}

size_t read_to_string(int fd, string &buffer) {
  size_t buf_init_len = size(buffer);
  string tmp(2048, '\0');
  size_t tmp_size = 0;
  while ((tmp_size = read(fd, std::as_writable_bytes(span{tmp}))) > 0) {
    buffer.insert(end(buffer), begin(tmp), begin(tmp) + tmp_size);
  }
  return size(buffer) - buf_init_len;
}

void read2(int h1, vector<std::byte> &buf1, int h2, vector<std::byte> &buf2) {
  vector<std::byte> tmp1(2048), tmp2(2048);
  size_t size1 = 0, size2 = 0;
  while ((size1 = read(h1, std::as_writable_bytes(span{tmp1}))) > 0 ||
         (size2 = read(h2, std::as_writable_bytes(span{tmp2}))) > 0) {
    if (size1 > 0) {
      buf1.insert(end(buf1), begin(tmp1), begin(tmp1) + size1);
    }
    if (size2 > 0) {
      buf2.insert(end(buf2), begin(tmp2), begin(tmp2) + size2);
    }
  }
}

void read2_to_string(int h1, string &buf1, int h2, string &buf2) {
  string tmp1(2048, '\0'), tmp2(2048, '\0');
  size_t size1 = 0, size2 = 0;
  while ((size1 = read(h1, std::as_writable_bytes(span{tmp1}))) > 0 ||
         (size2 = read(h2, std::as_writable_bytes(span{tmp2}))) > 0) {
    if (size1 > 0) {
      buf1.insert(end(buf1), begin(tmp1), begin(tmp1) + size1);
    }
    if (size2 > 0) {
      buf2.insert(end(buf2), begin(tmp2), begin(tmp2) + size2);
    }
  }
}
} // namespace FileDesc

namespace process {

struct Process {
  pid_t pid;
  int wait() {
    int wstatus;
    int exit_code;
    if (waitpid(pid, &wstatus, 0) == -1) {
      throw std::runtime_error("Failed to wait for child process");
    }
    if (WIFEXITED(wstatus)) {
      exit_code = WEXITSTATUS(wstatus);
    }
    return exit_code;
  }
  void kill() {
    if (::kill(pid, SIGKILL) != 0) {
      throw std::runtime_error(string("failed to kill process") +
                               std::to_string(pid));
    }
  }
};
/*============================================================================*/
struct ExitStatus::Impl {
  optional<int> code;
  Impl() : code(std::nullopt) {}
  Impl(int result) : code(result) {}
};
ExitStatus::ExitStatus() : impl_(std::make_unique<Impl>()) {}
ExitStatus::~ExitStatus() = default;
ExitStatus::ExitStatus(ExitStatus &&other) { *this = std::move(other); }
ExitStatus &ExitStatus::operator=(ExitStatus &&other) {
  if (this != &other) {
    this->impl_ = std::move(other.impl_);
  }
  return *this;
}
bool ExitStatus::success() { return impl_->code == 0; }
optional<int> ExitStatus::code() { return impl_->code; }

/*============================================================================*/
struct ChildStdin::Impl {
  int fd;
  Impl(int fd) : fd(fd) {}
};
ChildStdin::ChildStdin() : impl_(nullptr) {}
ChildStdin::~ChildStdin() {}
ChildStdin::ChildStdin(ChildStdin &&other) { *this = std::move(other); }
ChildStdin &ChildStdin::operator=(ChildStdin &&other) {
  if (this != &other) {
    this->impl_ = std::move(other.impl_);
  }
  return *this;
}
size_t ChildStdin::write(span<const std::byte> buffer) {
  return FileDesc::write(impl_->fd, buffer);
}

struct ChildStdout::Impl {
  int fd;
  Impl(int fd) : fd(fd) {}
};
ChildStdout::ChildStdout() : impl_(nullptr) {}
ChildStdout::~ChildStdout() {}
ChildStdout::ChildStdout(ChildStdout &&other) { *this = std::move(other); }
ChildStdout &ChildStdout::operator=(ChildStdout &&other) {
  if (this != &other) {
    this->impl_ = std::move(other.impl_);
  }
  return *this;
}
size_t ChildStdout::read(span<std::byte> buffer) {
  return FileDesc::read(impl_->fd, buffer);
}
size_t ChildStdout::read_to_end(std::vector<std::byte> &buffer) {
  return FileDesc::read_to_end(impl_->fd, buffer);
}
size_t ChildStdout::read_to_string(std::string &buffer) {
  return FileDesc::read_to_string(impl_->fd, buffer);
}

struct ChildStderr::Impl {
  int fd;
  Impl(int fd) : fd(fd) {}
};
ChildStderr::ChildStderr() : impl_(nullptr) {}
ChildStderr::~ChildStderr() {}
ChildStderr::ChildStderr(ChildStderr &&other) { *this = std::move(other); }
ChildStderr &ChildStderr::operator=(ChildStderr &&other) {
  if (this != &other) {
    this->impl_ = std::move(other.impl_);
  }
  return *this;
}
size_t ChildStderr::read(span<std::byte> buffer) {
  return FileDesc::read(impl_->fd, buffer);
}
size_t ChildStderr::read_to_end(std::vector<std::byte> &buffer) {
  return FileDesc::read_to_end(impl_->fd, buffer);
}
size_t ChildStderr::read_to_string(std::string &buffer) {
  return FileDesc::read_to_string(impl_->fd, buffer);
}

/*============================================================================*/
struct Stdio::Impl {
  Value value;
  Impl(Value v) : value(v), other(std::nullopt) {}
  optional<int> other;
  // me, child
  pair<optional<int>, optional<int>>
  to_fds(uint8_t id) { //{0: in, 1: out, 2: err}
    switch (value) {
    case Value::Inherit: {
      if (id == 0)
        return {STDIN_FILENO, std::nullopt};
      else if (id == 1)
        return {std::nullopt, STDOUT_FILENO};
      else if (id == 2)
        return {std::nullopt, STDERR_FILENO};
      else
        throw std::runtime_error("invalid handle id");
    }
    case Value::NewPipe: {
      int fds[2];
      if (::pipe(fds) == -1)
        throw std::runtime_error("Failed to create pipe");
      return id == 0 ? std::make_pair(fds[1], fds[0])
                     : std::make_pair(fds[0], fds[1]);
    }
    case Value::FromPipe: {
      if (id == 0)
        return {std::nullopt, other};
      else if (id == 1)
        return {std::nullopt, std::nullopt};
      else if (id == 2)
        return {std::nullopt, std::nullopt};
      else
        throw std::runtime_error("invalid handle id");
    }
    case Value::Null: {
      int null_fd = open("/dev/null", id == 0 ? O_RDONLY : O_WRONLY);
      if (null_fd == -1) {
        throw std::runtime_error("failed to open /dev/null");
      }
      return {std::nullopt, null_fd};
    }
    default:
      return {std::nullopt, std::nullopt};
    }
  }
};
Stdio::Stdio(Value v) : impl_(std::make_unique<Impl>(v)) {}
Stdio::~Stdio() = default;
Stdio::Stdio(Stdio &&other) { *this = std::move(other); };
Stdio &Stdio::operator=(Stdio &&other) {
  if (&other != this)
    this->impl_ = std::move(other.impl_);

  return *this;
}

Stdio Stdio::pipe() { return Stdio(Value::NewPipe); }
Stdio Stdio::inherit() { return Stdio(Value::Inherit); }
Stdio Stdio::null() { return Stdio(Value::Null); }
Stdio Stdio::from(ChildStdin other) {
  Stdio io = Stdio(Value::FromPipe);
  io.impl_->other = other.impl_->fd;
  return io;
}
Stdio Stdio::from(ChildStdout other) {
  Stdio io = Stdio(Value::FromPipe);
  io.impl_->other = other.impl_->fd;
  return io;
}
Stdio Stdio::from(ChildStderr other) {
  Stdio io = Stdio(Value::FromPipe);
  io.impl_->other = other.impl_->fd;
  return io;
}

/*============================================================================*/
struct Child::Impl {
  Process pi;

  Impl(Process pi) : pi(pi) {}

  int id() { return pi.pid; }
  void kill() { pi.kill(); }
  ExitStatus wait() {
    int code = pi.wait();
    ExitStatus status;
    status.impl_->code = code;
    return status;
  }
};

Child::Child() : impl_(nullptr){};
Child::~Child(){};
Child::Child(Child &&other) { *this = std::move(other); };
Child &Child::operator=(Child &&other) {
  if (&other != this) {
    this->impl_ = std::move(other.impl_);
  }
  return *this;
}

int Child::id() { return impl_->id(); }
ExitStatus Child::wait() { return impl_->wait(); }
void Child::kill() { impl_->kill(); }
Output Child::wait_with_output() {
  Output output;
  if (io_stdout and not io_stderr) {
    this->io_stdout->read_to_string(output.std_out);
  } else if (not io_stdout and io_stderr) {
    this->io_stderr->read_to_string(output.std_err);
  } else if (io_stdout and io_stderr) {
    FileDesc::read2_to_string(io_stdout->impl_->fd, output.std_out,
                              io_stderr->impl_->fd, output.std_err);
  } else { // nothing to capture
  }
  output.status = this->wait();
  return output;
}

/*============================================================================*/
class Command::Impl {
  string app;
  vector<string> args;
  optional<Stdio> io_stdin;
  optional<Stdio> io_stdout;
  optional<Stdio> io_stderr;
  optional<string> cwd;
  bool inherit_env;
  vector<pair<string, string>> envs;

public:
  Impl()
      : app("sh"), args(vector<string>()), io_stdin(std::nullopt),
        io_stdout(std::nullopt), io_stderr(std::nullopt), inherit_env(true) {}
  ~Impl() = default;
  void set_app(string str) { app = str; }
  void add_args(const string &arg) { args.push_back(arg); }
  void set_stdin(Stdio io) { io_stdin = std::move(io); }
  void set_stdout(Stdio io) { io_stdout = std::move(io); }
  void set_stderr(Stdio io) { io_stderr = std::move(io); }

  vector<char *> build_args() {
    std::vector<char *> exec_args = {const_cast<char *>(app.c_str())};
    if (!args.empty()) {
      for (const auto &arg : args) {
        exec_args.push_back(const_cast<char *>(arg.c_str()));
      }
    }
    exec_args.push_back(nullptr); // execvp needs a null-terminated array
    return exec_args;
  }
  void set_cwd(const string &path) {
    // if (!std::filesystem::exists(path))
    //   throw std::runtime_error(std::format("{} not exist", path));
    cwd = path;
  }
  void add_env(const string &key, const string &value) {
    envs.push_back({key, value});
  };
  void clear_env() { inherit_env = false; }
  void setup_io(Stdio::Value mode) {
    if (not io_stdin) {
      io_stdin = Stdio::inherit(); // FIX:
    }
    if (not io_stdout) {
      io_stdout = Stdio(mode);
    }
    if (not io_stderr) {
      io_stderr = Stdio(mode);
    }
  }
  Child spawn() {
    auto arguments = build_args();
    auto [our_stdin, their_stdin] = io_stdin->impl_->to_fds(0);
    auto [our_stdout, their_stdout] = io_stdout->impl_->to_fds(1);
    auto [our_stderr, their_stderr] = io_stderr->impl_->to_fds(2);
    pid_t pid = fork();
    if (pid == -1)
      throw std::runtime_error("Failed to fork");
    if (pid == 0) {
      close(*our_stdin);
      close(*our_stdout);
      close(*our_stderr);
      if (their_stdin.has_value() && their_stdin != STDIN_FILENO) {
        dup2(*their_stdin, STDIN_FILENO);
        close(*their_stdin);
      }
      if (their_stdout.has_value() && their_stdout != STDOUT_FILENO) {
        dup2(*their_stdout, STDOUT_FILENO);
        close(*their_stdout);
      }
      if (their_stderr.has_value() && their_stderr != STDERR_FILENO) {
        dup2(*their_stderr, STDERR_FILENO);
        close(*their_stderr);
      }
      if (not inherit_env) {
        environ = nullptr;
      }
      for (const auto &[key, value] : envs) {
        setenv(key.c_str(), value.c_str(), 1);
      }
      if (auto lpath = cwd) {
        string &path = *lpath;
        char *home = nullptr;
        if (path.front() == '~') {
          if (not(home = getenv("HOME"))) {
            throw std::runtime_error("HOME environment variable not set");
          }
        }
        if (chdir((home ? string(home) + path.substr(1) : path).c_str()) ==
            -1) {
          throw std::runtime_error("failed to change directory");
        }
      }
      if (execvp(app.c_str(), arguments.data()) == -1) {
        throw std::runtime_error("execvp failed");
        _exit(EXIT_FAILURE);
      }
    }
    if (their_stdin.has_value() && *their_stdin != STDIN_FILENO) {
      close(*their_stdin);
    }
    if (their_stdout.has_value() && *their_stdout != STDOUT_FILENO) {
      close(*their_stdout);
    }
    if (their_stderr.has_value() && *their_stderr != STDERR_FILENO) {
      close(*their_stderr);
    }

    Child s;
    if (our_stdin.has_value()) {
      s.io_stdin = std::make_optional<ChildStdin>();
      s.io_stdin->impl_ = std::make_unique<ChildStdin::Impl>(*our_stdin);
    }
    if (our_stdout.has_value()) {
      s.io_stdout = std::make_optional<ChildStdout>();
      s.io_stdout->impl_ = std::make_unique<ChildStdout::Impl>(*our_stdout);
    }
    if (our_stderr.has_value()) {
      s.io_stderr = std::make_optional<ChildStderr>();
      s.io_stderr->impl_ = std::make_unique<ChildStderr::Impl>(*our_stderr);
    }

    s.impl_ = std::make_unique<Child::Impl>(Process{.pid = pid});
    return s;
  }
};

Command::Command(const string &app) : impl_(std::make_unique<Impl>()) {
  if (!app.empty())
    impl_->set_app(app);
};
Command::~Command(){};
Command::Command(Command &&other) { *this = std::move(other); }
Command &Command::operator=(Command &&other) {
  if (this != &other) {
    this->impl_ = std::move(other.impl_);
  }
  return *this;
}

Command &&Command::arg(const string &arg) {
  impl_->add_args(arg);
  return std::move(*this);
}
Command &&Command::args(const std::vector<std::string> &args) {
  for (const auto &arg : args) {
    impl_->add_args(arg);
  }
  return std::move(*this);
}
Command &&Command::std_in(Stdio io) {
  impl_->set_stdin(std::move(io));
  return std::move(*this);
}
Command &&Command::std_out(Stdio io) {
  impl_->set_stdout(std::move(io));
  return std::move(*this);
}
Command &&Command::std_err(Stdio io) {
  impl_->set_stderr(std::move(io));
  return std::move(*this);
}
Command &&Command::current_dir(const std::string &path) {
  impl_->set_cwd(path);
  return std::move(*this);
}
Command &&Command::env(const string &key, const string &value) {
  impl_->add_env(key, value);
  return std::move(*this);
}
Command &&Command::env_clear() {
  impl_->clear_env();
  return std::move(*this);
}
Child Command::spawn() {
  impl_->setup_io(Stdio::Value::Inherit);
  return impl_->spawn();
}
ExitStatus Command::status() {
  impl_->setup_io(Stdio::Value::Inherit);
  Child child = impl_->spawn();
  return child.wait();
}

Output Command::output() {
  impl_->setup_io(Stdio::Value::NewPipe);
  Child child = impl_->spawn();
  return child.wait_with_output();
}

} // namespace process

#endif
