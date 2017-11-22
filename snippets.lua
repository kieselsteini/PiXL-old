--[[----------------------------------------------------------------------------

      Some useful code snippets.
      You can use these snippets in your games for free.

      This is free and unencumbered software released into the public domain.

      Anyone is free to copy, modify, publish, use, compile, sell, or
      distribute this software, either in source code form or as a compiled
      binary, for any purpose, commercial or non-commercial, and by any
      means.

      In jurisdictions that recognize copyright laws, the author or authors
      of this software dedicate any and all copyright interest in the
      software to the public domain. We make this dedication for the benefit
      of the public at large and to the detriment of our heirs and
      successors. We intend this dedication to be an overt act of
      relinquishment in perpetuity of all present and future rights to this
      software under copyright law.

      THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
      EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
      MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
      IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
      OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
      ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
      OTHER DEALINGS IN THE SOFTWARE.

      For more information, please refer to <https://unlicense.org>

--]]----------------------------------------------------------------------------

--[[----------------------------------------------------------------------------

      Simple network abstraction which will provide "reliable" packets.

      Still work in progress... ;)

--]]----------------------------------------------------------------------------
local network = {}

-- reset the network peers
function network.reset()
  network.peers = {}
end

-- find a peer using host & port, if the peer is not existing create a new peer
function network.find_peer(host, port)
  local id = host .. ':' .. port
  local peer = network.peers[id]
  if not peer then
    peer = {
      host = host,
      port = port,
      incoming = {
        count = 1,
        queue = {},
      },
      outgoing = {
        count = 1,
        queue = {},
      },
      userdata = {},
    }
    network.peers[id] = peer
  end
  return peer
end

-- start serving on the given port
function network.serve(port)
  pixl.bind(port or 4040)
end

-- "connect" to hostname and port (simply create a peer)
function network.connect(hostname, port)
  local host = assert(pixl.resolve(hostname))
  return network.find_peer(host, port or 4040)
end

-- send data to a peer (packets will be resent)
function network.send(peer, data)
  -- create new packet, place it in outgoing queue and do initial send
  local packet_data = string.pack('>I4B', peer.outgoing.count, 1)
  peer.outgoing.queue[peer.outgoing.count] = {
    data = packet_data,
    count = 1,
  }
  peer.outgoing.count = peer.outgoing.count + 1
  pixl.send(peer.host, peer.port, packet_data)
end

-- broadcast data to all known peers (excluding the given peer)
function network.broadcast(data, exclude_peer)
  for _, peer in pairs(network.peers) do
    if peer ~= exclude_peer then
      network.send(peer, data)
    end
  end
end

-- update networking. This should be called once in the update() callback
-- triggers the fn(peer, data) when data for a peer is available
-- it is guaranteed that packets will received in the correct order
function network.update(fn)
  -- check for incoming packets
  while true do
    local packet_data, host, port = pixl.recv()
    if not packet_data then break end
    local peer = network.find_peer(host, port)
    -- decode packet
    local packet_id, command = string.unpack('>I4B', packet_data)
    if command == 1 then
      -- got an packet with data, place it in incoming queue and send ack
      -- don't place the packet in queue if the packet was older than expected
      -- but still send and ack as the peer might not know, that we already have it
      if packet_id >= peer.incoming.count then
        peer.incoming.queue[packet_id] = string.sub(packet_data, 6)
      end
      pixl.send(peer.host, peer.port, string.pack('>I4B', packet_id, 2))
    elseif command == 2 then
      -- got an ack for an outgoing packet
      peer.outgoing.queue[packet_id] = nil
    end
  end
  -- iterate over all peers to check incoming / outgoing queues
  for peer_id, peer in pairs(network.peers) do
    -- check incoming queue and fire callback
    while peer.incoming.queue[peer.incoming.count] do
      local packet_data = peer.incoming.queue[peer.incoming.count]
      peer.incoming.queue[peer.incoming.count] = nil
      peer.incoming.count = peer.incoming.count + 1
      fn(peer, packet_data)
    end
    -- check outgoing queue and try to resend if necessary
    for packet_id, packet in pairs(peer.outgoing.queue) do
      packet.count = packet.count + 1
      if packet.count > 10 then
        packet.count = 1
        pixl.send(peer.host, peer.port, packet.data)
      end
    end
  end
end
