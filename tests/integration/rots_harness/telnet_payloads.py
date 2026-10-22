"""Decodes and encodes the subnegotiation payloads of TTYPE, CHARSET, NAWS, MSDP and MSSP.

Payloads here are the bytes between the option byte and IAC SE, with IAC IAC already reduced;
telnet.subnegotiation_bytes doubles any 0xFF an encoded payload holds.
"""

from __future__ import annotations

from typing import Dict, List, Union

# TTYPE: the client's answer, followed by its terminal type.
TTYPE_IS = 0
# TTYPE: the server asks for the client's terminal type.
TTYPE_SEND = 1

# CHARSET: a separator byte and the character sets offered, split by it.
CHARSET_REQUEST = 1
# CHARSET: the one character set chosen from a request.
CHARSET_ACCEPTED = 2
# CHARSET: none of the offered character sets is acceptable.
CHARSET_REJECTED = 3

# MSDP: starts a variable name.
MSDP_VAR = 1
# MSDP: starts a value of the variable before it.
MSDP_VAL = 2
# MSDP: starts a table of name and value pairs.
MSDP_TABLE_OPEN = 3
# MSDP: ends a table.
MSDP_TABLE_CLOSE = 4
# MSDP: starts an array of values.
MSDP_ARRAY_OPEN = 5
# MSDP: ends an array.
MSDP_ARRAY_CLOSE = 6

# The MSDP bytes that end a text name or value.
MSDP_MARKERS = frozenset(
    (MSDP_VAR, MSDP_VAL, MSDP_TABLE_OPEN, MSDP_TABLE_CLOSE, MSDP_ARRAY_OPEN, MSDP_ARRAY_CLOSE)
)

# MSSP: starts a variable name.
MSSP_VAR = 1
# MSSP: starts a value of the variable before it.
MSSP_VAL = 2

# The MSSP bytes that end a text name or value.
MSSP_MARKERS = frozenset((MSSP_VAR, MSSP_VAL))

# The largest width or height NAWS carries: two bytes each.
NAWS_DIMENSION_LIMIT = 0xFFFF

# One MSDP value: text, an array of values, or a table of named values.
MsdpValue = Union[str, List["MsdpValue"], Dict[str, "MsdpValue"]]


class TelnetPayloadError(ValueError):
    """A subnegotiation payload does not follow its option's grammar; the message names the byte offset."""


def decode_msdp(payload: bytes) -> dict[str, MsdpValue]:
    """The variables of one MSDP frame: one or more VAR name VAL value (VAL value)*.

    Several VALs for one VAR give a list. Text is decoded as Latin-1 and returned as sent, so the
    server's JSON escapes are not undone. A name repeated in one frame or table is an error, since
    keeping either value would hide what the server sent.
    """
    reader = _MsdpReader(payload)
    variables = reader.read_pairs(None)
    if not variables:
        raise TelnetPayloadError("MSDP frame at byte 0 holds no variable")
    return variables


def encode_msdp(name: str, *values: str) -> bytes:
    """VAR name, then VAL value for each value; raises ValueError for marker bytes or no values."""
    if not name:
        raise ValueError("an MSDP variable name may not be empty")
    if not values:
        raise ValueError(f"MSDP variable {name!r} needs at least one value")
    encoded = bytearray([MSDP_VAR])
    encoded.extend(_encode_msdp_text(name))
    for value in values:
        encoded.append(MSDP_VAL)
        encoded.extend(_encode_msdp_text(value))
    return bytes(encoded)


def decode_mssp(payload: bytes) -> dict[str, list[str]]:
    """Every MSSP field: VAR name VAL value (VAL value)*, each name mapped to its values.

    Text is decoded as Latin-1. A name repeated in one payload is an error, as in decode_msdp.
    """
    fields: dict[str, list[str]] = {}
    offset = 0
    while offset < len(payload):
        if payload[offset] != MSSP_VAR:
            raise TelnetPayloadError(f"MSSP expected VAR at byte {offset}")
        variable_offset = offset
        name, offset = _read_text(payload, offset + 1, MSSP_MARKERS)
        if not name:
            raise TelnetPayloadError(f"MSSP VAR at byte {variable_offset} has an empty name")
        if name in fields:
            raise TelnetPayloadError(f"MSSP repeats {name!r} at byte {variable_offset}")
        values: list[str] = []
        while offset < len(payload) and payload[offset] == MSSP_VAL:
            value, offset = _read_text(payload, offset + 1, MSSP_MARKERS)
            values.append(value)
        if not values:
            raise TelnetPayloadError(f"MSSP VAR {name!r} at byte {variable_offset} has no VAL")
        fields[name] = values
    if not fields:
        raise TelnetPayloadError("MSSP payload at byte 0 holds no field")
    return fields


def decode_charset_request(payload: bytes) -> list[str]:
    """The character sets a CHARSET REQUEST offers, split on the separator byte after REQUEST.

    Empty names between separators are dropped. The optional [TTABLE] form is not supported.
    """
    if not payload or payload[0] != CHARSET_REQUEST:
        raise TelnetPayloadError("CHARSET expected REQUEST at byte 0")
    if len(payload) < 2:
        raise TelnetPayloadError("CHARSET REQUEST has no separator at byte 1")
    separator = payload[1:2]
    names = [name.decode("latin1") for name in payload[2:].split(separator) if name]
    if not names:
        raise TelnetPayloadError("CHARSET REQUEST offers no character set after byte 1")
    return names


def encode_naws(width: int, height: int) -> bytes:
    """Width then height, each two bytes big-endian; raises ValueError outside 0 to 65535."""
    for label, dimension in (("width", width), ("height", height)):
        if not 0 <= dimension <= NAWS_DIMENSION_LIMIT:
            raise ValueError(f"NAWS {label} {dimension} is outside 0 to {NAWS_DIMENSION_LIMIT}")
    return width.to_bytes(2, "big") + height.to_bytes(2, "big")


def _read_text(payload: bytes, offset: int, markers: frozenset[int]) -> tuple[str, int]:
    """The Latin-1 text from offset up to the next marker or the end, and the offset after it."""
    end = offset
    while end < len(payload) and payload[end] not in markers:
        end += 1
    return payload[offset:end].decode("latin1"), end


def _encode_msdp_text(text: str) -> bytes:
    encoded = text.encode("latin1")
    for position, byte in enumerate(encoded):
        if byte in MSDP_MARKERS:
            raise ValueError(f"MSDP text {text!r} holds marker byte {byte} at position {position}")
    return encoded


class _MsdpReader:
    """Recursive descent over one MSDP payload, tracking the offset of the next unread byte."""

    def __init__(self, payload: bytes) -> None:
        # The frame being decoded.
        self._payload = payload
        # The offset of the next unread byte.
        self._offset = 0

    def read_pairs(self, table_offset: int | None) -> dict[str, MsdpValue]:
        """Name and value pairs up to the end of the frame, or up to TABLE_CLOSE inside a table.

        table_offset is the offset of the TABLE_OPEN being read, or None for the whole frame.
        """
        pairs: dict[str, MsdpValue] = {}
        while True:
            if self._offset >= len(self._payload):
                if table_offset is not None:
                    raise TelnetPayloadError(
                        f"MSDP TABLE_OPEN at byte {table_offset} has no TABLE_CLOSE"
                        f" before the end at byte {self._offset}"
                    )
                return pairs
            byte = self._payload[self._offset]
            if table_offset is not None and byte == MSDP_TABLE_CLOSE:
                self._offset += 1
                return pairs
            if byte != MSDP_VAR:
                raise TelnetPayloadError(f"MSDP expected VAR at byte {self._offset}, found {byte}")
            variable_offset = self._offset
            name, self._offset = _read_text(self._payload, self._offset + 1, MSDP_MARKERS)
            if not name:
                raise TelnetPayloadError(f"MSDP VAR at byte {variable_offset} has an empty name")
            if name in pairs:
                raise TelnetPayloadError(f"MSDP repeats {name!r} at byte {variable_offset}")
            values: list[MsdpValue] = []
            while self._offset < len(self._payload) and self._payload[self._offset] == MSDP_VAL:
                self._offset += 1
                values.append(self._read_value())
            if not values:
                raise TelnetPayloadError(f"MSDP VAR {name!r} at byte {variable_offset} has no VAL")
            pairs[name] = values[0] if len(values) == 1 else values

    def _read_value(self) -> MsdpValue:
        if self._offset >= len(self._payload):
            return ""
        byte = self._payload[self._offset]
        if byte == MSDP_TABLE_OPEN:
            table_offset = self._offset
            self._offset += 1
            return self.read_pairs(table_offset)
        if byte == MSDP_ARRAY_OPEN:
            return self._read_array()
        text, self._offset = _read_text(self._payload, self._offset, MSDP_MARKERS)
        return text

    def _read_array(self) -> list[MsdpValue]:
        array_offset = self._offset
        self._offset += 1
        items: list[MsdpValue] = []
        while True:
            if self._offset >= len(self._payload):
                raise TelnetPayloadError(
                    f"MSDP ARRAY_OPEN at byte {array_offset} has no ARRAY_CLOSE"
                    f" before the end at byte {self._offset}"
                )
            byte = self._payload[self._offset]
            if byte == MSDP_ARRAY_CLOSE:
                self._offset += 1
                return items
            if byte != MSDP_VAL:
                raise TelnetPayloadError(
                    f"MSDP expected VAL or ARRAY_CLOSE at byte {self._offset}, found {byte}"
                )
            self._offset += 1
            items.append(self._read_value())
