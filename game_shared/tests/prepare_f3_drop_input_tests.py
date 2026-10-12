#!/usr/bin/env python3
import math, struct, unittest
from prepare_f3_drop_input import extract, encode
def model():
    data=bytearray(244+224);data[:4]=b'IDST';struct.pack_into('<i',data,4,10);struct.pack_into('<i',data,72,len(data));struct.pack_into('<ii',data,140,2,244)
    for i in range(2):
        at=244+i*112;name=('root' if i==0 else 'tip').encode();data[at:at+len(name)]=name;struct.pack_into('<i',data,at+32,i-1);struct.pack_into('<6f',data,at+64,1,2,3,.1,.2,.3)
    return data
class InputTests(unittest.TestCase):
    def test_reference(self):
        b=extract(model());self.assertEqual(b[1][:2],('tip',0));self.assertAlmostEqual(b[1][2][4],.2);self.assertIn('"tip" 0',encode(b));self.assertEqual(encode(b),encode(extract(model())))
    def test_bounds(self):
        for size in [0,4,243,245,467]:
            with self.assertRaises(ValueError):extract(model()[:size])
        data=model();struct.pack_into('<i',data,144,0x7fffffff)
        with self.assertRaises(ValueError):extract(data)
    def test_names_parents_floats(self):
        for mutate in [lambda d:struct.pack_into('<f',d,308,math.nan),lambda d:struct.pack_into('<i',d,276,1),lambda d:d.__setitem__(slice(356,360),b'root')]:
            data=model();mutate(data)
            with self.assertRaises(ValueError):extract(data)
if __name__=='__main__':unittest.main()
