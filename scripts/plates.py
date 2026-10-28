"""Licence plate patterns: the parser the data build and check_plates.py share.

The grammar is the one dasmig/vehiclegen.hpp implements (doc/data.md):

  L  a letter of the format's alphabet      D  a digit 0-9
  N  a digit 1-9                            X  a letter or a digit
  R  the region code                        S  the region's office number (2 digits)
  C  the region's office letter             A  British age identifier (2 digits)
  Y  British prefix year letter             V  year of registration (2 digits)
  H  half of the year (1 or 2)              J  Japanese serial number (12-34)
  T^m or T^m-n   token T repeated m (to n) times
  [k:...]        a sequential group of significance k (1 the most significant)
  {a|b|...}      one of the literal options
  'text' or \\c   literal text
  any other character except A-Z is literal.
"""

TOKENS = set("LDNXRSCAYVHJ")


class PatternError(ValueError):
    pass


def letters(spec):
    """Expand an alphabet such as 'A-Z!IOQ' into a list of characters."""
    keep, _, drop = spec.partition("!")
    out = []
    i = 0
    while i < len(keep):
        if i + 2 < len(keep) and keep[i + 1] == "-":
            out.extend(chr(c) for c in range(ord(keep[i]), ord(keep[i + 2]) + 1))
            i += 3
        else:
            out.append(keep[i])
            i += 1
    return [c for c in dict.fromkeys(out) if c not in drop]


def parse(pattern, group=0):
    """Parse a pattern into items: (kind, value, min, max, group)."""
    items = []
    i = 0
    n = len(pattern)

    def add(kind, value):
        items.append([kind, value, 1, 1, group])

    while i < n:
        c = pattern[i]
        if c in TOKENS:
            add("token", c)
            i += 1
        elif c == "[":
            close = pattern.find("]", i)
            colon = pattern.find(":", i)
            if group or close < 0 or colon < 0 or colon > close:
                raise PatternError(f"bad group at {i}")
            significance = int(pattern[i + 1:colon])
            if significance < 1:
                raise PatternError("group significance must be 1 or more")
            for item in parse(pattern[colon + 1:close], significance):
                if item[0] != "token" or item[1] not in "LDN" or item[2] != item[3]:
                    raise PatternError("a sequential group holds only fixed L, D and N")
                items.append(item)
            i = close + 1
        elif c == "{":
            close = pattern.find("}", i)
            if close < 0:
                raise PatternError("unclosed {")
            add("choice", pattern[i + 1:close].split("|"))
            i = close + 1
        elif c == "'":
            close = pattern.find("'", i + 1)
            if close < 0:
                raise PatternError("unclosed quote")
            add("literal", pattern[i + 1:close])
            i = close + 1
        elif c == "\\":
            if i + 1 >= n:
                raise PatternError("dangling escape")
            add("literal", pattern[i + 1])
            i += 2
        elif c == "^":
            if not items or items[-1][0] != "token" or items[-1][4] != group:
                raise PatternError("^ follows a token")
            j = i + 1
            while j < n and pattern[j].isdigit():
                j += 1
            low = int(pattern[i + 1:j])
            high = low
            if j < n and pattern[j] == "-" and j + 1 < n and pattern[j + 1].isdigit():
                k = j + 1
                while k < n and pattern[k].isdigit():
                    k += 1
                high = int(pattern[j + 1:k])
                j = k
            if high < low:
                raise PatternError("repeat range reversed")
            items[-1][2], items[-1][3] = low, high
            i = j
        elif "A" <= c <= "Z":
            raise PatternError(f"letter {c} is not a token; quote it")
        else:
            add("literal", c)
            i += 1
    return items


def sequential_layout(items, alphabet):
    """The radices of a sequential pattern's characters, most significant first."""
    slots = [(item[4], position, item[1]) for position, item in enumerate(items) if item[4]]
    slots.sort()
    radix = {"L": len(alphabet), "D": 10, "N": 9}
    return [(position, radix[token]) for _, position, token in slots]


def plate_index(plate, items, alphabet):
    """The index of a plate written in a sequential pattern, or None if it does not match."""
    chars = {}
    pos = 0
    for position, item in enumerate(items):
        kind, value = item[0], item[1]
        if kind == "literal":
            if not plate.startswith(value, pos):
                return None
            pos += len(value)
        elif kind == "token" and item[4]:
            if pos >= len(plate):
                return None
            c = plate[pos]
            if value == "L":
                if c not in alphabet:
                    return None
                chars[position] = alphabet.index(c)
            elif value == "D":
                if not c.isdigit():
                    return None
                chars[position] = int(c)
            else:
                if c not in "123456789":
                    return None
                chars[position] = int(c) - 1
            pos += 1
        else:
            return None
    if pos != len(plate):
        return None
    index = 0
    for position, radix in sequential_layout(items, alphabet):
        index = index * radix + chars[position]
    return index


def capacity(items, alphabet):
    total = 1
    for _, radix in sequential_layout(items, alphabet):
        total *= radix
    return total
