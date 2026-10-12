# tools/vectors.py test/vectors.h -- regenerate the codec test vectors
import sys
# Independent construction of OFTP command vectors from the RFC layouts.
def X(s,w): return s.ljust(w)[:w].encode('ascii')
def N(v,w): return str(v).rjust(w,'0').encode('ascii')
CR=b'\r'
v={}
v['ssrm']=b'I'+b'ODETTE FTP READY '+CR
v['ssid5']=b'X'+N(5,1)+X('O0013000000MVS1',25)+X('SECRET',8)+N(4096,5)+b'B'+b'N'+b'Y'+b'N'+N(99,3)+b'N'+X('',4)+X('USER1',8)+CR
v['ssid2']=b'X'+N(2,1)+X('O0013000000MVS2',25)+X('PW',8)+N(128,5)+b'S'+b'N'+b'N'+b'N'+N(1,3)+b' '+X('',4)+X('',8)+CR
common=X('O0013000000MVS2',25)+X('O0013000000MVS1',25)
v['sfid13']=b'H'+X('PAYROLL.DATA',26)+X('',9)+X('261012',6)+X('174501',6)+X('U1',8)+common+b'F'+N(80,5)+N(12,7)+N(0,9)
v['sfid14']=b'H'+X('PAYROLL.DATA',26)+X('',3)+N(20261012,8)+N(1745010001,10)+X('U1',8)+common+b'V'+N(255,5)+N(12,7)+N(0,9)
v['sfid20']=b'H'+X('PAYROLL.DATA',26)+X('',3)+N(20261012,8)+N(1745010001,10)+X('U1',8)+common+b'T'+N(0,5)+N(12,13)+N(12,13)+N(0,17)+N(0,2)+N(0,2)+N(0,1)+N(0,1)+b'N'+N(4,3)+'Löw'.encode('utf-8')
v['sfpa14']=b'2'+N(42,9)
v['sfpa20']=b'2'+N(42,17)
v['sfna14']=b'3'+N(13,2)+b'Y'
v['sfna20']=b'3'+N(13,2)+b'Y'+N(9,3)+b'duplicate'
v['efid14']=b'T'+N(1000,9)+N(80000,12)
v['efid20']=b'T'+N(1000,17)+N(80000,17)
v['efna20']=b'5'+N(11,2)+N(0,3)
v['esid14']=b'F'+N(0,2)+CR
v['esid20']=b'F'+N(3,2)+N(2,3)+b'no'+CR
erp_id=X('PAYROLL.DATA',26)
v['eerp13']=b'E'+erp_id+X('',9)+X('261012',6)+X('174501',6)+X('U1',8)+X('O0013000000MVS1',25)+X('O0013000000MVS2',25)
v['eerp20']=b'E'+erp_id+X('',3)+N(20261012,8)+N(1745010001,10)+X('U1',8)+X('O0013000000MVS1',25)+X('O0013000000MVS2',25)+b'\0\0\0\0'
v['nerp20']=b'N'+erp_id+X('',6)+N(20261012,8)+N(1745010001,10)+X('O0013000000MVS1',25)+X('O0013000000MVS2',25)+X('O0013000000MVS2',25)+N(12,2)+N(0,3)+b'\0\0\0\0'
lens={'ssrm':19,'ssid5':61,'ssid2':61,'sfid13':128,'sfid14':128,'sfid20':169,'sfpa14':10,'sfpa20':18,'sfna14':4,'sfna20':16,'efid14':22,'efid20':35,'efna20':6,'esid14':4,'esid20':9,'eerp13':106,'eerp20':110,'nerp20':135}
for k,b in v.items(): assert len(b)==lens[k],(k,len(b))
out="/* vectors.h - OFTP commands built independently from the RFC layouts\n** (tools/vectors.py); ASCII octets as they go on the wire.\n*/\n"
for k,b in v.items():
    out+=f"static const unsigned char v_{k}[{len(b)}] = {{\n"
    for i in range(0,len(b),12): out+="    "+", ".join(f"0x{x:02X}" for x in b[i:i+12])+",\n"
    out+="};\n"
open(sys.argv[1],'w').write(out)
print("ok", len(v))
