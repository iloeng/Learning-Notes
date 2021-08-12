"""
Generic socket server classes.
通用 socket 服务器类

This module tries to capture the various aspects of defining a server:
该模块试图捕捉定义服务器的各个方面：

For socket-based servers:
对于基于 socket 的服务器：

- address family:
        - AF_INET{,6}: IP (Internet Protocol) sockets (default) IP 套接字
        - AF_UNIX: Unix domain sockets  Unix 域套接字
        - others, e.g. AF_DECNET are conceivable (see <socket.h> 其他
- socket type:
        - SOCK_STREAM (reliable stream, e.g. TCP)   可靠的： TCP
        - SOCK_DGRAM (datagrams, e.g. UDP)          数据包： UDP

For request-based servers (including socket-based):
对于基于请求的服务器 （包括基于 socket）

- client address verification before further looking at the request
  在进一步查看请求之前验证客户端地址
        (This is actually a hook for any processing that needs to look
         at the request before anything else, e.g. logging)
        （这实际上是任何需要先查看请求的处理的钩子，例如 日志记录）
- how to handle multiple requests:
  如何处理多个请求
        - synchronous (one request is handled at a time)  同步处理（一次处理一个请求）
        - forking (each request is handled by a new process) 分叉（每个请求都由一个新进程处理）
        - threading (each request is handled by a new thread) 线程（每个请求都由一个新线程处理）

The classes in this module favor the server type that is simplest to
write: a synchronous TCP/IP server.  This is bad class design, but
saves some typing.  (There's also the issue that a deep class hierarchy
slows down method lookups.)
本模块中的类支持最容易编写的服务器类型：同步 TCP/IP 服务器。 这是糟糕的类设计，
但可以节省一些输入。 （还有一个问题，即深层次的类层次会减慢方法查找的速度。）

There are five classes in an inheritance diagram, four of which represent
synchronous servers of four types:
一个继承图中有五个类，其中四个代表四种类型的同步服务器：

        +------------+
        | BaseServer |
        +------------+
              |
              v
        +-----------+        +------------------+
        | TCPServer |------->| UnixStreamServer |
        +-----------+        +------------------+
              |
              v
        +-----------+        +--------------------+
        | UDPServer |------->| UnixDatagramServer |
        +-----------+        +--------------------+

Note that UnixDatagramServer derives from UDPServer, not from
UnixStreamServer -- the only difference between an IP and a Unix
stream server is the address family, which is simply repeated in both
unix server classes.
请注意， UnixDatagramServer 派生自 UDPServer，而不是 UnixStreamServer —— IP 和
Unix 流服务器之间的唯一区别是地址族，这在两个 unix 服务器类中都简单重复。

Forking and threading versions of each type of server can be created
using the ForkingMixIn and ThreadingMixIn mix-in classes.  For
instance, a threading UDP server class is created as follows:
可以使用 ForkingMixIn 和 ThreadingMixIn 混合类创建每种类型服务器的分叉和线程版本。
例如，一个线程 UDP 服务器类的创建如下：

        class ThreadingUDPServer(ThreadingMixIn, UDPServer): pass

The Mix-in class must come first, since it overrides a method defined
in UDPServer! Setting the various member variables also changes
the behavior of the underlying server mechanism.
Mix-in 类必须先出现，因为它覆盖了 UDPServer 中定义的方法！ 设置各种成员变量也会改变底
层服务器机制的行为

To implement a service, you must derive a class from
BaseRequestHandler and redefine its handle() method.  You can then run
various versions of the service by combining one of the server classes
with your request handler class.
要实现服务，您必须从 BaseRequestHandler 派生一个类并重新定义其 handle() 方法。 然后，
您可以通过将服务器类之一与您的请求处理程序类组合来运行服务的各种版本。

The request handler class must be different for datagram or stream
services.  This can be hidden by using the request handler
subclasses StreamRequestHandler or DatagramRequestHandler.
数据报或流服务的请求处理程序类必须不同。 这可以通过使用请求处理程序子类
StreamRequestHandler 或 DatagramRequestHandler 来隐藏。

Of course, you still have to use your head!
当然，你还是得用你的脑袋！

For instance, it makes no sense to use a forking server if the service
contains state in memory that can be modified by requests (since the
modifications in the child process would never reach the initial state
kept in the parent process and passed to each child).  In this case,
you can use a threading server, but you will probably have to use
locks to avoid two requests that come in nearly simultaneous to apply
conflicting changes to the server state.
例如，如果服务在内存中包含可由请求修改的状态，那么使用分叉服务器是没有意义的（因为子进程
中的修改永远不会达到父进程中保存的初始状态并传递给每个子进程） . 在这种情况下，您可以使
用线程服务器，但您可能必须使用锁来避免两个几乎同时出现的请求以将冲突更改应用于服务器状态。

On the other hand, if you are building e.g. an HTTP server, where all
data is stored externally (e.g. in the file system), a synchronous
class will essentially render the service "deaf" while one request is
being handled -- which may be for a very long time if a client is slow
to read all the data it has requested.  Here a threading or forking
server is appropriate.
另一方面，如果您正在构建例如 一个 HTTP 服务器，其中所有数据都存储在外部（例如在文件系统
中），同步类本质上将在处理一个请求时使服务“聋”——如果客户端很慢，这可能会持续很长时间读取
它请求的所有数据。 这里适合使用线程或分叉服务器。

In some cases, it may be appropriate to process part of a request
synchronously, but to finish processing in a forked child depending on
the request data.  This can be implemented by using a synchronous
server and doing an explicit fork in the request handler class
handle() method.
在某些情况下，同步处理请求的一部分可能是合适的，但根据请求数据在分叉的子进程中完成处理。
这可以通过使用同步服务器并在请求处理程序类 handle() 方法中执行显式分叉来实现。

Another approach to handling multiple simultaneous requests in an
environment that supports neither threads nor fork (or where these are
too expensive or inappropriate for the service) is to maintain an
explicit table of partially finished requests and to use a selector to
decide which request to work on next (or whether to handle a new
incoming request).  This is particularly important for stream services
where each client can potentially be connected for a long time (if
threads or subprocesses cannot be used).
在既不支持线程也不支持 fork 的环境中处理多个同时请求的另一种方法（或者这些对服务来说太
昂贵或不合适的地方）是维护一个部分完成的请求的显式表，并使用选择器来决定哪个请求工作 在
下一个（或是否处理新的传入请求）。 这对于每个客户端可能连接很长时间的流服务尤其重要（如
果不能使用线程或子进程）。

Future work:
- Standard classes for Sun RPC (which uses either UDP or TCP)
  Sun RPC 的标准类（使用 UDP 或 TCP）
- Standard mix-in classes to implement various authentication
  and encryption schemes
  标准混合类以实现各种身份验证和加密方案

XXX Open problems:
- What to do with out-of-band data?
  如何处理带外数据？

BaseServer:
- split generic "request" functionality out into BaseServer class.
  将通用“请求”功能拆分为 BaseServer 类。
  Copyright (C) 2000  Luke Kenneth Casson Leighton <lkcl@samba.org>

  example: read entries from a SQL database (requires overriding
  get_request() to return a table entry from the database).
  entry is processed by a RequestHandlerClass.
  从 SQL 数据库读取条目（需要覆盖 get_request() 以从数据库返回表条目）。 条目由
  RequestHandlerClass 处理。

"""

# Author of the BaseServer patch: Luke Kenneth Casson Leighton

__version__ = "0.4"


import socket
import selectors
import os
import sys
import threading
from io import BufferedIOBase
from time import monotonic as time

__all__ = ["BaseServer", "TCPServer", "UDPServer",
           "ThreadingUDPServer", "ThreadingTCPServer",
           "BaseRequestHandler", "StreamRequestHandler",
           "DatagramRequestHandler", "ThreadingMixIn"]
if hasattr(os, "fork"):
    __all__.extend(["ForkingUDPServer","ForkingTCPServer", "ForkingMixIn"])
if hasattr(socket, "AF_UNIX"):
    __all__.extend(["UnixStreamServer","UnixDatagramServer",
                    "ThreadingUnixStreamServer",
                    "ThreadingUnixDatagramServer"])

# poll/select have the advantage of not requiring any extra file descriptor,
# contrarily to epoll/kqueue (also, they require a single syscall).
if hasattr(selectors, 'PollSelector'):
    _ServerSelector = selectors.PollSelector
else:
    _ServerSelector = selectors.SelectSelector


class BaseServer:

    """Base class for server classes.

    Methods for the caller:

    - __init__(server_address, RequestHandlerClass)
    - serve_forever(poll_interval=0.5)
    - shutdown()
    - handle_request()  # if you do not use serve_forever()
    - fileno() -> int   # for selector

    Methods that may be overridden:

    - server_bind()
    - server_activate()
    - get_request() -> request, client_address
    - handle_timeout()
    - verify_request(request, client_address)
    - server_close()
    - process_request(request, client_address)
    - shutdown_request(request)
    - close_request(request)
    - service_actions()
    - handle_error()

    Methods for derived classes:

    - finish_request(request, client_address)

    Class variables that may be overridden by derived classes or
    instances:

    - timeout
    - address_family
    - socket_type
    - allow_reuse_address

    Instance variables:

    - RequestHandlerClass
    - socket

    """

    timeout = None

    def __init__(self, server_address, RequestHandlerClass):
        """Constructor.  May be extended, do not override."""
        """
        1. BaseServer 基类初始化
        2. 拥有两个参数， server_address 和 RequestHandlerClass
        3. 拥有 4 个属性， 可以被继承， 不能被重写
        4. self.server_address 就是传的第一个参数 server_address， self.RequestHandlerClass
           是传的第二个参数 RequestHandlerClass； self.__is_shut_down 是 threading.Event()
           线程的 Event() 对象， 以双下划线开头，表示为私有成员，只允许类本身访问，
           子类也不行。在文本上被替换为_class__method；  self.__shutdown_request 
           初始为 False
        """
        self.server_address = server_address
        self.RequestHandlerClass = RequestHandlerClass
        self.__is_shut_down = threading.Event()
        self.__shutdown_request = False

    def server_activate(self):
        """Called by constructor to activate the server.

        May be overridden.

        server_activate 方法可能被重写
        """
        pass

    def serve_forever(self, poll_interval=0.5):
        """Handle one request at a time until shutdown.

        Polls for shutdown every poll_interval seconds. Ignores
        self.timeout. If you need to do periodic tasks, do them in
        another thread.

        一次处理一个请求直到关闭
        每 poll_interval 秒轮询一次关机。 忽略 self.timeout。 如果您需要执行周期性任
        务，请在另一个线程中执行。

        1. 首先重置 __is_shut_down
        """
        self.__is_shut_down.clear()
        try:
            # XXX: Consider using another file descriptor or connecting to the
            # socket to wake this up instead of polling. Polling reduces our
            # responsiveness to a shutdown request and wastes cpu at all other
            # times.
            with _ServerSelector() as selector:
                selector.register(self, selectors.EVENT_READ)

                while not self.__shutdown_request:
                    ready = selector.select(poll_interval)
                    # bpo-35017: shutdown() called during select(), exit immediately.
                    if self.__shutdown_request:
                        break
                    if ready:
                        self._handle_request_noblock()

                    self.service_actions()
        finally:
            self.__shutdown_request = False
            self.__is_shut_down.set()

    def shutdown(self):
        """Stops the serve_forever loop.

        Blocks until the loop has finished. This must be called while
        serve_forever() is running in another thread, or it will
        deadlock.

        停止 serve_forever 循环， 阻塞 serve_forever 直到循环停止。 这个方法只能在
        serve_forever 在另一回线程中执行的时候调用， 否则将会死锁

        执行这个函数后 __shutdown_request 属性将会置为 True， 并执行 __is_shut_down.wait
        函数
        """
        self.__shutdown_request = True
        self.__is_shut_down.wait()

    def service_actions(self):
        """Called by the serve_forever() loop.

        May be overridden by a subclass / Mixin to implement any code that
        needs to be run during the loop.

        在 serve_forever 循环中被调用， 可能会被子类或混合类重写为任何需要在循环中执
        行的代码
        """
        pass

    # The distinction between handling, getting, processing and finishing a
    # request is fairly arbitrary.  Remember:
    #
    # - handle_request() is the top-level call.  It calls selector.select(),
    #   get_request(), verify_request() and process_request()
    # - get_request() is different for stream or datagram sockets
    # - process_request() is the place that may fork a new process or create a
    #   new thread to finish the request
    # - finish_request() instantiates the request handler class; this
    #   constructor will handle the request all by itself

    def handle_request(self):
        """Handle one request, possibly blocking.

        Respects self.timeout.

        1. 处理请求， 很可能会阻塞
        2. 首先获取超时时间
        """
        # Support people who used socket.settimeout() to escape
        # handle_request before self.timeout was available.
        timeout = self.socket.gettimeout()
        if timeout is None:
            timeout = self.timeout
        elif self.timeout is not None:
            timeout = min(timeout, self.timeout)
        if timeout is not None:
            deadline = time() + timeout

        # Wait until a request arrives or the timeout expires - the loop is
        # necessary to accommodate early wakeups due to EINTR.
        with _ServerSelector() as selector:
            selector.register(self, selectors.EVENT_READ)

            while True:
                ready = selector.select(timeout)
                if ready:
                    # 当请求 ready 时， 执行 _handle_request_noblock 进行非阻塞的请求处理
                    return self._handle_request_noblock()
                else:
                    if timeout is not None:
                        timeout = deadline - time()
                        if timeout < 0:
                            return self.handle_timeout()

    def _handle_request_noblock(self):
        """Handle one request, without blocking.

        I assume that selector.select() has returned that the socket is
        readable before this function was called, so there should be no risk of
        blocking in get_request().

        1. 无阻塞处理请求
        2. 假设 selector.select() 在调用这个函数之前已经返回套接字是可读的，所以在
           get_request() 中应该没有阻塞的风险
        """
        try:
            request, client_address = self.get_request()
        except OSError:
            return
        if self.verify_request(request, client_address):
            # 当 request 应该继续处理时， 就尝试执行 process_request， 当出现其他情
            # 况时， 就直接处理相关的错误， 并关闭这个请求
            try:
                self.process_request(request, client_address)
            except Exception:
                self.handle_error(request, client_address)
                self.shutdown_request(request)
            except:
                self.shutdown_request(request)
                raise
        else:
            # 当不应该继续处理时， 直接关闭请求
            self.shutdown_request(request)

    def handle_timeout(self):
        """Called if no new request arrives within self.timeout.

        Overridden by ForkingMixIn.

        这个函数会在当 self.timeout 时间内没有新的请求到来时调用， 会被 ForkingMixIn
        重写
        """
        pass

    def verify_request(self, request, client_address):
        """Verify the request.  May be overridden.

        Return True if we should proceed with this request.

        验证给定的请求， 当应该继续处理这个请求的时候， 返回 True
        """
        return True

    def process_request(self, request, client_address):
        """Call finish_request.

        Overridden by ForkingMixIn and ThreadingMixIn.

        处理请求函数， 会被 ForkingMixIn 和 ThreadingMixIn 类重写， 完成一个请求后再
        关闭这个请求
        """
        self.finish_request(request, client_address)
        self.shutdown_request(request)

    def server_close(self):
        """Called to clean-up the server.

        May be overridden.

        关闭 server， 100% 可能被重写
        """
        pass

    def finish_request(self, request, client_address):
        """Finish one request by instantiating RequestHandlerClass."""
        """ 用一个实例化的 RequestHandlerClass 完成一个请求 """
        self.RequestHandlerClass(request, client_address, self)

    def shutdown_request(self, request):
        """Called to shutdown and close an individual request."""
        """ 与 close_request 功能一致， 但是有可能会被重写 """
        self.close_request(request)

    def close_request(self, request):
        """Called to clean up an individual request."""
        """ 用于关闭一个单独的请求， 100% 会被重写， 因为该函数中就一个 pass """
        pass

    def handle_error(self, request, client_address):
        """Handle an error gracefully.  May be overridden.

        The default is to print a traceback and continue.

        """
        """
        1. 当处理出错时， 将错误信息按照一定格式输出到标准输出中
        """
        print('-'*40, file=sys.stderr)
        print('Exception happened during processing of request from',
            client_address, file=sys.stderr)
        import traceback
        traceback.print_exc()
        print('-'*40, file=sys.stderr)

    def __enter__(self):
        """
        1. __enter__ 和 __exit__ 构成上下文， 可以使用 with 关键字
        2. 使用 with 时， 先执行 __enter__， with 内的语句执行完毕后执行 __exit__
        """
        return self

    def __exit__(self, *args):
        """
        1. 同 __enter__ 函数中的描述， BaseServer 执行完毕后， 会执行 __exit__ 函数
           从而调用 server_close 来关闭 server
        """
        self.server_close()


class TCPServer(BaseServer):

    """Base class for various socket-based server classes.

    Defaults to synchronous IP stream (i.e., TCP).

    Methods for the caller:

    - __init__(server_address, RequestHandlerClass, bind_and_activate=True)
    - serve_forever(poll_interval=0.5)
    - shutdown()
    - handle_request()  # if you don't use serve_forever()
    - fileno() -> int   # for selector

    Methods that may be overridden:

    - server_bind()
    - server_activate()
    - get_request() -> request, client_address
    - handle_timeout()
    - verify_request(request, client_address)
    - process_request(request, client_address)
    - shutdown_request(request)
    - close_request(request)
    - handle_error()

    Methods for derived classes:

    - finish_request(request, client_address)

    Class variables that may be overridden by derived classes or
    instances:

    - timeout
    - address_family
    - socket_type
    - request_queue_size (only for stream sockets)
    - allow_reuse_address

    Instance variables:

    - server_address
    - RequestHandlerClass
    - socket

    """

    address_family = socket.AF_INET

    socket_type = socket.SOCK_STREAM

    request_queue_size = 5

    allow_reuse_address = False

    def __init__(self, server_address, RequestHandlerClass, bind_and_activate=True):
        """Constructor.  May be extended, do not override."""
        BaseServer.__init__(self, server_address, RequestHandlerClass)
        self.socket = socket.socket(self.address_family,
                                    self.socket_type)
        if bind_and_activate:
            try:
                self.server_bind()
                self.server_activate()
            except:
                self.server_close()
                raise

    def server_bind(self):
        """Called by constructor to bind the socket.

        May be overridden.

        """
        if self.allow_reuse_address:
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.socket.bind(self.server_address)
        self.server_address = self.socket.getsockname()

    def server_activate(self):
        """Called by constructor to activate the server.

        May be overridden.

        """
        self.socket.listen(self.request_queue_size)

    def server_close(self):
        """Called to clean-up the server.

        May be overridden.

        """
        self.socket.close()

    def fileno(self):
        """Return socket file number.

        Interface required by selector.

        """
        return self.socket.fileno()

    def get_request(self):
        """Get the request and client address from the socket.

        May be overridden.

        """
        return self.socket.accept()

    def shutdown_request(self, request):
        """Called to shutdown and close an individual request."""
        try:
            #explicitly shutdown.  socket.close() merely releases
            #the socket and waits for GC to perform the actual close.
            request.shutdown(socket.SHUT_WR)
        except OSError:
            pass #some platforms may raise ENOTCONN here
        self.close_request(request)

    def close_request(self, request):
        """Called to clean up an individual request."""
        request.close()


class UDPServer(TCPServer):

    """UDP server class."""

    allow_reuse_address = False

    socket_type = socket.SOCK_DGRAM

    max_packet_size = 8192

    def get_request(self):
        data, client_addr = self.socket.recvfrom(self.max_packet_size)
        return (data, self.socket), client_addr

    def server_activate(self):
        # No need to call listen() for UDP.
        pass

    def shutdown_request(self, request):
        # No need to shutdown anything.
        self.close_request(request)

    def close_request(self, request):
        # No need to close anything.
        pass

if hasattr(os, "fork"):
    class ForkingMixIn:
        """Mix-in class to handle each request in a new process."""

        timeout = 300
        active_children = None
        max_children = 40
        # If true, server_close() waits until all child processes complete.
        block_on_close = True

        def collect_children(self, *, blocking=False):
            """Internal routine to wait for children that have exited."""
            if self.active_children is None:
                return

            # If we're above the max number of children, wait and reap them until
            # we go back below threshold. Note that we use waitpid(-1) below to be
            # able to collect children in size(<defunct children>) syscalls instead
            # of size(<children>): the downside is that this might reap children
            # which we didn't spawn, which is why we only resort to this when we're
            # above max_children.
            while len(self.active_children) >= self.max_children:
                try:
                    pid, _ = os.waitpid(-1, 0)
                    self.active_children.discard(pid)
                except ChildProcessError:
                    # we don't have any children, we're done
                    self.active_children.clear()
                except OSError:
                    break

            # Now reap all defunct children.
            for pid in self.active_children.copy():
                try:
                    flags = 0 if blocking else os.WNOHANG
                    pid, _ = os.waitpid(pid, flags)
                    # if the child hasn't exited yet, pid will be 0 and ignored by
                    # discard() below
                    self.active_children.discard(pid)
                except ChildProcessError:
                    # someone else reaped it
                    self.active_children.discard(pid)
                except OSError:
                    pass

        def handle_timeout(self):
            """Wait for zombies after self.timeout seconds of inactivity.

            May be extended, do not override.
            """
            self.collect_children()

        def service_actions(self):
            """Collect the zombie child processes regularly in the ForkingMixIn.

            service_actions is called in the BaseServer's serve_forever loop.
            """
            self.collect_children()

        def process_request(self, request, client_address):
            """Fork a new subprocess to process the request."""
            pid = os.fork()
            if pid:
                # Parent process
                if self.active_children is None:
                    self.active_children = set()
                self.active_children.add(pid)
                self.close_request(request)
                return
            else:
                # Child process.
                # This must never return, hence os._exit()!
                status = 1
                try:
                    self.finish_request(request, client_address)
                    status = 0
                except Exception:
                    self.handle_error(request, client_address)
                finally:
                    try:
                        self.shutdown_request(request)
                    finally:
                        os._exit(status)

        def server_close(self):
            super().server_close()
            self.collect_children(blocking=self.block_on_close)


class ThreadingMixIn:
    """Mix-in class to handle each request in a new thread."""

    # Decides how threads will act upon termination of the
    # main process
    daemon_threads = False
    # If true, server_close() waits until all non-daemonic threads terminate.
    block_on_close = True
    # For non-daemonic threads, list of threading.Threading objects
    # used by server_close() to wait for all threads completion.
    _threads = None

    def process_request_thread(self, request, client_address):
        """Same as in BaseServer but as a thread.

        In addition, exception handling is done here.

        """
        try:
            self.finish_request(request, client_address)
        except Exception:
            self.handle_error(request, client_address)
        finally:
            self.shutdown_request(request)

    def process_request(self, request, client_address):
        """Start a new thread to process the request."""
        t = threading.Thread(target = self.process_request_thread,
                             args = (request, client_address))
        t.daemon = self.daemon_threads
        if not t.daemon and self.block_on_close:
            if self._threads is None:
                self._threads = []
            self._threads.append(t)
        t.start()

    def server_close(self):
        super().server_close()
        if self.block_on_close:
            threads = self._threads
            self._threads = None
            if threads:
                for thread in threads:
                    thread.join()


if hasattr(os, "fork"):
    class ForkingUDPServer(ForkingMixIn, UDPServer): pass
    class ForkingTCPServer(ForkingMixIn, TCPServer): pass

class ThreadingUDPServer(ThreadingMixIn, UDPServer): pass
class ThreadingTCPServer(ThreadingMixIn, TCPServer): pass

if hasattr(socket, 'AF_UNIX'):

    class UnixStreamServer(TCPServer):
        address_family = socket.AF_UNIX

    class UnixDatagramServer(UDPServer):
        address_family = socket.AF_UNIX

    class ThreadingUnixStreamServer(ThreadingMixIn, UnixStreamServer): pass

    class ThreadingUnixDatagramServer(ThreadingMixIn, UnixDatagramServer): pass

class BaseRequestHandler:

    """Base class for request handler classes.

    This class is instantiated for each request to be handled.  The
    constructor sets the instance variables request, client_address
    and server, and then calls the handle() method.  To implement a
    specific service, all you need to do is to derive a class which
    defines a handle() method.

    The handle() method can find the request as self.request, the
    client address as self.client_address, and the server (in case it
    needs access to per-server information) as self.server.  Since a
    separate instance is created for each request, the handle() method
    can define other arbitrary instance variables.

    """

    def __init__(self, request, client_address, server):
        self.request = request
        self.client_address = client_address
        self.server = server
        self.setup()
        try:
            self.handle()
        finally:
            self.finish()

    def setup(self):
        pass

    def handle(self):
        pass

    def finish(self):
        pass


# The following two classes make it possible to use the same service
# class for stream or datagram servers.
# Each class sets up these instance variables:
# - rfile: a file object from which receives the request is read
# - wfile: a file object to which the reply is written
# When the handle() method returns, wfile is flushed properly


class StreamRequestHandler(BaseRequestHandler):

    """Define self.rfile and self.wfile for stream sockets."""

    # Default buffer sizes for rfile, wfile.
    # We default rfile to buffered because otherwise it could be
    # really slow for large data (a getc() call per byte); we make
    # wfile unbuffered because (a) often after a write() we want to
    # read and we need to flush the line; (b) big writes to unbuffered
    # files are typically optimized by stdio even when big reads
    # aren't.
    rbufsize = -1
    wbufsize = 0

    # A timeout to apply to the request socket, if not None.
    timeout = None

    # Disable nagle algorithm for this socket, if True.
    # Use only when wbufsize != 0, to avoid small packets.
    disable_nagle_algorithm = False

    def setup(self):
        self.connection = self.request
        if self.timeout is not None:
            self.connection.settimeout(self.timeout)
        if self.disable_nagle_algorithm:
            self.connection.setsockopt(socket.IPPROTO_TCP,
                                       socket.TCP_NODELAY, True)
        self.rfile = self.connection.makefile('rb', self.rbufsize)
        if self.wbufsize == 0:
            self.wfile = _SocketWriter(self.connection)
        else:
            self.wfile = self.connection.makefile('wb', self.wbufsize)

    def finish(self):
        if not self.wfile.closed:
            try:
                self.wfile.flush()
            except socket.error:
                # A final socket error may have occurred here, such as
                # the local error ECONNABORTED.
                pass
        self.wfile.close()
        self.rfile.close()

class _SocketWriter(BufferedIOBase):
    """Simple writable BufferedIOBase implementation for a socket

    Does not hold data in a buffer, avoiding any need to call flush()."""

    def __init__(self, sock):
        self._sock = sock

    def writable(self):
        return True

    def write(self, b):
        self._sock.sendall(b)
        with memoryview(b) as view:
            return view.nbytes

    def fileno(self):
        return self._sock.fileno()

class DatagramRequestHandler(BaseRequestHandler):

    """Define self.rfile and self.wfile for datagram sockets."""

    def setup(self):
        from io import BytesIO
        self.packet, self.socket = self.request
        self.rfile = BytesIO(self.packet)
        self.wfile = BytesIO()

    def finish(self):
        self.socket.sendto(self.wfile.getvalue(), self.client_address)
