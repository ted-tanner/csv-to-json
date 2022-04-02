from pathlib import Path

import ctypes
import os
import platform


__c_csv_libs_path = os.path.join(Path(__file__).resolve().parent, 'libs')

__os_name = platform.system()

if __os_name == 'Linux':
    __dll_name = 'csv-to-json.so'
elif __os_name == 'Windows':
    __dll_name = 'csv-to-json.dll'
elif __os_name == 'Darwin':
    if 'x86_64' == platform.uname().machine:
        __dll_name = 'csv-to-json-x86.dylib'
    else:
        __dll_name = 'csv-to-json-arm64.dylib'
else:
    raise ImportError('csvtojson module not supported on this system')

__c_csv_lib_path = os.path.join(__c_csv_libs_path, __dll_name)
__c_csv_lib = ctypes.cdll.LoadLibrary(__c_csv_lib_path)

__c_csv_lib.csv_to_json.argtypes = (ctypes.c_char_p, ctypes.c_size_t)
__c_csv_lib.csv_to_json.restype = ctypes.c_void_p

__c_csv_lib.free_json.argtypes = [ctypes.c_void_p]
__c_csv_lib.free_json.restype = None


def csv_to_json(csv, as_str=False):
    if isinstance(csv, str):
        csv = bytes(csv, encoding='utf-8')

    ptr = __c_csv_lib.csv_to_json(csv, len(csv))
    json = ctypes.cast(ptr, ctypes.c_char_p).value

    __c_csv_lib.free_json(ptr)

    if as_str:
        return json.decode('utf-8')
    else:
        return json
