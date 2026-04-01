# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

from abc import ABCMeta, abstractmethod
from dataclasses import dataclass

@dataclass
class base_default(object):
    def __init__(self):
        pass

class ipa_unit_c(metaclass=ABCMeta):
    def __init__(self, obj, name, unit_type):
        self.name = name
        self.type = unit_type
        self.text = str()
        self.decode(obj)
    
    @abstractmethod
    def decode(self, obj):
        pass

    def customized_name(self):
        return None

    def get_text(self):
        return self.text

class fatal_error(RuntimeError):
    def __init__(self, message):
        RuntimeError.__init__(self, message)

    @staticmethod
    def WithResult(message, result):
        message += f' (result was {hex(result)})'
        return fatal_error(message)

def dict_object(d):
    class C:
        pass

    if isinstance(d, list):
        d = [dict_object(x) for x in d] 
 
    if not isinstance(d, dict):
        return d

    obj = C()
  
    for k in d:
        obj.__dict__[k] = dict_object(d[k])
  
    return obj

def cfmt_string(s):
    def rm_hs(s):
        if not s:
            return ''
        for i in range(0, len(s)):
            if s[i] != ' ':
                return s[i:]
        return ''

    def brace_delta(line):
        '''Count net open braces in line, ignoring braces inside string literals.'''
        delta = 0
        i = 0
        in_single = False
        in_double = False
        while i < len(line):
            c = line[i]
            if in_single:
                if c == '\\':
                    i += 1
                elif c == '\'':
                    in_single = False
            elif in_double:
                if c == '\\':
                    i += 1
                elif c == '"':
                    in_double = False
            else:
                if c == '\'':
                    in_single = True
                elif c == '"':
                    in_double = True
                elif c == '{':
                    delta += 1
                elif c == '}':
                    delta -= 1
            i += 1
        return delta

    count = 0
    lines = s.splitlines()
    new_lines = list()
    for l in lines:
        l = rm_hs(l)
        if l:
            tmp = count
            delta = brace_delta(l)
            count += delta
            if delta < 0:
                tmp = count
            if len(l) > 0:
                new_lines.append(tmp * 4 * ' ' + l)
    
    return '\n'.join(new_lines) + '\n' * 2
