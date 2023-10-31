# vchatd protocol
messages between the server and clients follow a shared
routine

 u4: size of message (in bytes)
 u2: type of message
...: map of arguments

## argument map
the argument map is similar to a json
an arg map is a data structure that maps some strings ("keys")
	to some values ("datum")
an argument map looks like:
 u4: size (number of key-datum pairs)
...: key-data pairs

key-data pairs look like:
 string: data
 u1: type
 u4/string/timestamp: data

strings look like:
 u2: len (in bytes)
...: strdata (no null terminator)

## message types

message types are split into different categories,
which are a generalization of what the types under the category are

0x00: message codes (e.g. failure, success)
	0x00: OK
	0x10: NON-EXISTENT
		failure response when a client attempt to perform an action
			in a room that either does not exist or they do not have access to
	0x50: NO-ACCESS
		failure response when a client attempt to perform an 
			action they do not have access to

0x40: client side chat messages
	0x00: SEND
		attempt to broadcast a message into a room
		\[0] u8 "id"      | room id
		\[1] vchar "data" | message data
		\[2] time "time"  | when was this message sent
	0x80: REQUEST
		attempt to aquire some chat messages from a room from a certain timeframe
		\[0] u8 "id"     | room id
		\[1] time "time" | latest time (set to unix epoch for "now")

0x80: server side chat distribution
	0x00: BROADCAST
		\[0] u8 "id"

0xFF: meta-messages, e.g. ping
	0x00: PING
		server or client must reply with a "PONG"
	0x01: PONG
		reply to a "PING" packet
