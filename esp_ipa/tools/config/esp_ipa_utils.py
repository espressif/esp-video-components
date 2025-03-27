# SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import sys

class fatal_error(RuntimeError):
    def __init__(self, message):
        RuntimeError.__init__(self, message)

    @staticmethod
    def WithResult(message, result):
        message += ' (result was %s)' % hexify(result)
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
        for i in range(0, len(s)):
            if s[i] != ' ':
                return s[i:]

    count = 0
    lines = s.splitlines()
    new_lines = list()
    for l in lines:
        l = rm_hs(l)
        if l != None:
            tmp = count
            if '{' in l and '}' in l:
                pass
            elif '{' in l:
                count += 1
            elif '}' in l:
                count -= 1
                tmp -= 1
            if len(l) > 0:
                new_lines.append(tmp * 4 * ' ' + rm_hs(l))
    
    return '\n'.join(new_lines) + '\n' * 2
