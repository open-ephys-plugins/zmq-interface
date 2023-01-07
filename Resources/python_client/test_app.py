import zmq
import sys
import numpy as np
import json
import uuid
import time

from threading import Thread, current_thread

class Event(object):
    """
    
    Represents an event received from a ZMQ Interface plugin
    
    """
    
    event_types = {0: 'TIMESTAMP', 1: 'BUFFER_SIZE', 2: 'PARAMETER_CHANGE',
                   3: 'TTL', 4: 'SPIKE', 5: 'MESSAGE', 6: 'BINARY_MSG'}

    def __init__(self, _d, _data=None):
        self.type = None
        self.stream = ''
        self.sample_num = 0
        self.source_node = 0
        self.event_state = 0
        self.event_line = 0
        self.event_word = 0
        self.numBytes = 0
        self.data = b''
        self.__dict__.update(_d)
        self.timestamp = None
        
        # noinspection PyTypeChecker
        self.type = Event.event_types[self.type]
        if _data:
            self.data = _data
            self.numBytes = len(_data)

            dfb = np.frombuffer(self.data, dtype=np.uint8)
            self.event_line = dfb[0]

            dfb = np.frombuffer(self.data, dtype=np.uint8, offset=1)
            self.event_state = dfb[0]

            dfb = np.frombuffer(self.data, dtype=np.uint64, offset=2)
            self.event_word = dfb[0]
        if self.type == 'TIMESTAMP':
            t = np.frombuffer(self.data, dtype=np.int64)
            self.timestamp = t[0]

    def set_data(self, _data):
        """ Sets event data """
        self.data = _data
        self.numBytes = len(_data)

    def __str__(self):
        """Prints info about the event"""
        ds = self.__dict__.copy()
        del ds['data']
        return str(ds)


class Spike(object):

    def __init__(self, _d, _data=None):
        self.stream = ''
        self.source_node = 0
        self.electrode = 0
        self.sample_num = 0
        self.num_channels = 0
        self.num_samples = 0
        self.sorted_id = 0
        self.threshold = []

        self.__dict__.update(_d)
        self.data = _data

    def __str__(self):
        """Prints info about the event"""
        ds = self.__dict__.copy()
        del ds['data']
        return str(ds)


class TestApp(object):
    """
    Python app used to test the ZMQ Interface plugin
    """
    def __init__(self, ip="tcp://localhost", port=5556):
        self._timer = None
        
        self.interval = 0.1
        self.is_running = False
        self.context = zmq.Context()
        self.heartbeat_socket = None
        self.data_socket = None
        self.poller = zmq.Poller()
        self.ip = ip
        self.port = port
        self.message_num = 0
        self.socket_waits_reply = False

        self.uuid = str(uuid.uuid4())
        self.app_name = 'Test App ' + self.uuid[:4]

        print(f'App name: {self.app_name}')

        self.last_heartbeat_time = 0
        self.last_reply_time = time.time()

        self.init_socket()
        self.start()

    def init_socket(self):
        """Initialize the data socket"""
        if not self.data_socket:
            ip_string = f'{self.ip}:{self.port}'
            print("Initializing data socket on " + ip_string)
            self.data_socket = self.context.socket(zmq.SUB)
            self.data_socket.connect(ip_string)

            self.data_socket.setsockopt(zmq.SUBSCRIBE, b'')
            self.poller.register(self.data_socket, zmq.POLLIN)

        if not self.heartbeat_socket:
            ip_string = f'{self.ip}:{self.port + 1}'
            print("Initializing heartbeat socket on " + ip_string)
            self.heartbeat_socket = self.context.socket(zmq.REQ)
            self.heartbeat_socket.connect(ip_string)

            self.poller.register(self.heartbeat_socket, zmq.POLLIN)

    def start(self):
        """Start the callback thread"""

        t = Thread(target=self.callback)
        self.message_num = 0

        try:
            t.start()
            while t.is_alive():
                t.join(0.5)
        except KeyboardInterrupt as e:
            print('Keyboard interrupt')
            print('Total messages received: ', self.message_num)
            t.alive = False
            t.join()

    def send_heartbeat(self):
        """Sends heartbeat message to ZMQ Interface,
           to indicate that the app is alive   
        """
        d = {'application': self.app_name,
             'uuid': self.uuid,
             'type': 'heartbeat'}
        j_msg = json.dumps(d)
        print("sending heartbeat")
        self.heartbeat_socket.send(j_msg.encode('utf-8'))
        self.last_heartbeat_time = time.time()
        self.socket_waits_reply = True

    def callback(self):

        t = current_thread()
        t.alive = True

        while t.alive:

            if (time.time() - self.last_heartbeat_time) > 2.:
                
                if self.socket_waits_reply:
                    
                    print("heartbeat haven't got reply, retrying...")
                    
                    self.last_heartbeat_time += 1.
                    
                    if (time.time() - self.last_reply_time) > 10.:
                        # reconnecting the socket as per
                        # the "lazy pirate" pattern (see the ZeroMQ guide)
                        print("connection lost, trying to reconnect")
                        self.poller.unregister(self.data_socket)
                        self.data_socket.close()
                        self.data_socket = None

                        self.init_socket()

                        self.socket_waits_reply = False
                        self.last_reply_time = time.time()
                else:
                    self.send_heartbeat()

            # check poller
            socks = dict(self.poller.poll(1))
            
            if not socks:
                continue

            if self.data_socket in socks:
                
                try:
                    message = self.data_socket.recv_multipart(zmq.NOBLOCK)
                except zmq.ZMQError as err:
                    print("got error: {0}".format(err))
                    break
                    
                if message:

                    self.message_num += 1

                    if len(message) < 2:
                        print("no frames for message: ", message[0])
                    try:
                        header = json.loads(message[1].decode('utf-8'))
                    except ValueError as e:
                        print("ValueError: ", e)
                        print(message[1])
                    
                    if header['message_num'] != self.message_num:
                        print("Missed a message at number", self.message_num)

                    self.message_num = header['message_num']
                    
                    if header['type'] == 'data':
                        c = header['content']
                        num_samples = c['num_samples']
                        channel_num = c['channel_num']
                        sample_rate = c['sample_rate']

                        if channel_num == 1:
                            try:
                                n_arr = np.frombuffer(message[2],
                                                        dtype=np.float32)
                                n_arr = np.reshape(n_arr, num_samples)

                                if num_samples > 0:
                                    print(f"Received {num_samples} samples")

                            except IndexError as e:
                                print(e)
                                print(header)
                                print(message[1])
                                if len(message) > 2:
                                    print(len(message[2]))
                                else:
                                    print("only one frame???")

                    elif header['type'] == 'event':

                        if header['data_size'] > 0:
                            event = Event(header['content'],
                                                    message[2])
                        else:
                            event = Event(header['content'])

                        print(event)

                    elif header['type'] == 'spike':
                        spike = Spike(header['spike'],
                                                    message[2])
                        print(spike)
                    else:
                        raise ValueError("message type unknown")
                else:
                    print("No data in message, breaking")

            elif self.heartbeat_socket in socks and self.socket_waits_reply:
                message = self.heartbeat_socket.recv()
                print(f'Heartbeat reply: {message}')
                if self.socket_waits_reply:
                    self.socket_waits_reply = False
                else:
                    print("Received reply before sending a message?")

