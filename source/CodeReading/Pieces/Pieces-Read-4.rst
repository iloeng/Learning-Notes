Pieces 源码阅读系列 3 
---------------------------------

PeerConnection 类分析
======================

为了方便，将代码拿到这里：

.. code-block:: python

    class PeerConnection:

        def __init__(self, queue: Queue, info_hash,
                    peer_id, piece_manager, on_block_cb=None):
            """
            Constructs a PeerConnection and add it to the asyncio event-loop.

            Use `stop` to abort this connection and any subsequent connection
            attempts

            :param queue: The async Queue containing available peers
            :param info_hash: The SHA1 hash for the meta-data's info
            :param peer_id: Our peer ID used to to identify ourselves
            :param piece_manager: The manager responsible to determine which pieces
                                to request
            :param on_block_cb: The callback function to call when a block is
                                received from the remote peer
            """
            self.my_state = []
            self.peer_state = []
            self.queue = queue
            self.info_hash = info_hash
            self.peer_id = peer_id
            self.remote_id = None
            self.writer = None
            self.reader = None
            self.piece_manager = piece_manager
            self.on_block_cb = on_block_cb
            self.future = asyncio.ensure_future(self._start())  # Start this worker

初始化的时候，传入了 5 个参数，4个必填参数，一个可选参数，分别是：

1. queue，是异步模块中的 Queue 对象

2. info_hash，是 meta_info 中的 info 字段的 SHA1 Hash 

3. peer_id 是自己的 peer ID

4. piece_manager 是 PieceManager 类实例化对象。

5. on_block_cb 是可选参数，默认为 None ，一个回调函数，当从远端接受到一个 block 后调用。

初始化的前面都分析完毕，重点看最后一步， ``self.future`` 中直接调用的是 ``_start`` 函\
数, 其代码为：

.. code-block:: python

    async def _start(self):
        while 'stopped' not in self.my_state:
            ip, port = await self.queue.get()
            logging.info('Got assigned peer with: {ip}'.format(ip=ip))

            try:
                # TODO For some reason it does not seem to work to open a new
                # connection if the first one drops (i.e. second loop).
                self.reader, self.writer = await asyncio.open_connection(
                    ip, port)
                logging.info('Connection open to peer: {ip}'.format(ip=ip))

                # It's our responsibility to initiate the handshake.
                buffer = await self._handshake()

                # TODO Add support for sending data
                # Sending BitField is optional and not needed when client does
                # not have any pieces. Thus we do not send any bitfield message

                # The default state for a connection is that peer is not
                # interested and we are choked
                self.my_state.append('choked')

                # Let the peer know we're interested in downloading pieces
                await self._send_interested()
                self.my_state.append('interested')

                # Start reading responses as a stream of messages for as
                # long as the connection is open and data is transmitted
                async for message in PeerStreamIterator(self.reader, buffer):
                    if 'stopped' in self.my_state:
                        break
                    if type(message) is BitField:
                        self.piece_manager.add_peer(self.remote_id,
                                                    message.bitfield)
                    elif type(message) is Interested:
                        self.peer_state.append('interested')
                    elif type(message) is NotInterested:
                        if 'interested' in self.peer_state:
                            self.peer_state.remove('interested')
                    elif type(message) is Choke:
                        self.my_state.append('choked')
                    elif type(message) is Unchoke:
                        if 'choked' in self.my_state:
                            self.my_state.remove('choked')
                    elif type(message) is Have:
                        self.piece_manager.update_peer(self.remote_id,
                                                       message.index)
                    elif type(message) is KeepAlive:
                        pass
                    elif type(message) is Piece:
                        self.my_state.remove('pending_request')
                        self.on_block_cb(
                            peer_id=self.remote_id,
                            piece_index=message.index,
                            block_offset=message.begin,
                            data=message.block)
                    elif type(message) is Request:
                        # TODO Add support for sending data
                        logging.info('Ignoring the received Request message.')
                    elif type(message) is Cancel:
                        # TODO Add support for sending data
                        logging.info('Ignoring the received Cancel message.')

                    # Send block request to remote peer if we're interested
                    if 'choked' not in self.my_state:
                        if 'interested' in self.my_state:
                            if 'pending_request' not in self.my_state:
                                self.my_state.append('pending_request')
                                await self._request_piece()

            except ProtocolError as e:
                logging.exception('Protocol error')
            except (ConnectionRefusedError, TimeoutError):
                logging.warning('Unable to connect to peer')
            except (ConnectionResetError, CancelledError):
                logging.warning('Connection closed')
            except Exception as e:
                logging.exception('An error occurred')
                self.cancel()
                raise e
            self.cancel()

首先判断自己的状态 ``self.my_state`` 是不是已经停止了，如果没有停止，就会执行\
接下来的步骤，等待获取到 IP 和端口，获取到后，记录日志。然后尝试执行接下来的步\
骤，打开一个异步连接，然后获取握手数据，执行函数为： 

.. code-block:: python

    async def _handshake(self):
        """
        Send the initial handshake to the remote peer and wait for the peer
        to respond with its handshake.
        """
        self.writer.write(Handshake(self.info_hash, self.peer_id).encode())
        await self.writer.drain()

        buf = b''
        tries = 1
        while len(buf) < Handshake.length and tries < 10:
            tries += 1
            buf = await self.reader.read(PeerStreamIterator.CHUNK_SIZE)

        response = Handshake.decode(buf[:Handshake.length])
        if not response:
            raise ProtocolError('Unable receive and parse a handshake')
        if not response.info_hash == self.info_hash:
            raise ProtocolError('Handshake with invalid info_hash')

        # TODO: According to spec we should validate that the peer_id received
        # from the peer match the peer_id received from the tracker.
        self.remote_id = response.peer_id
        logging.info('Handshake with peer was successful')

        # We need to return the remaining buffer data, since we might have
        # read more bytes then the size of the handshake message and we need
        # those bytes to parse the next message.
        return buf[Handshake.length:]

无法进行下去了，缺少异步知识，先补补知识在继续阅读。