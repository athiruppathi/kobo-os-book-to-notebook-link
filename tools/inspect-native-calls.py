"""Offline call analysis for the pinned Kobo 4.46.23836 native library.

Install requirements-analysis.txt into your Python environment first.
No firmware is executed. Only resolved direct calls are printed; indirect calls,
data pools, and control-flow reachability require separate analysis.
"""
import argparse
import hashlib
import struct
from elftools.elf.elffile import ELFFile
from capstone import Cs, CS_ARCH_ARM, CS_MODE_ARM, CS_MODE_THUMB
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('library')
parser.add_argument('--symbol', action='append', help='Exact exported symbol to inspect; repeat for multiple functions')
args = parser.parse_args()
with open(args.library, 'rb') as f:
    if hashlib.file_digest(f, 'sha256').hexdigest() != '25ec19024666dca7f134335af311593a21cfbd75a0235f1b77c27d9a28f733dd':
        raise SystemExit('Unsupported firmware hash; review PLT layout before adapting this tool')
    f.seek(0)
    elf=ELFFile(f)
    syms=list(elf.get_section_by_name('.dynsym').iter_symbols())
    names={s['st_value'] & ~1:s.name for s in syms if s['st_value']}
    plt=elf.get_section_by_name('.plt')
    rel=elf.get_section_by_name('.rel.plt')
    targets={r['r_offset']:syms[r['r_info_sym']].name for r in rel.iter_relocations()}
    def immediate(w):
        n=w&255; rot=((w>>8)&15)*2
        return ((n>>rot)|(n<<(32-rot)))&0xffffffff if rot else n
    raw=plt.data()
    for offset in range(0,len(raw)-11,4):
        a,b,c=struct.unpack_from('<III',raw,offset)
        if a&0xfffff000 == 0xe28fc000 and b&0xfffff000 == 0xe28cc000 and c&0xfffff000 == 0xe5bcf000:
            address=plt['sh_addr']+offset
            got=address+8+immediate(a)+immediate(b)+(c&4095)
            if got in targets:
                names[address]=targets[got]
                if offset >= 4 and struct.unpack_from('<I',raw,offset-4)[0] == 0x46c04778:
                    names[address-4]=targets[got] # Thumb bx pc; nop veneer
    for sym in syms:
        if sym.name not in (args.symbol or ['_ZN14N3ReaderOpener11openNotePadERK6Volume','_ZN14N3ReaderOpener11showNotePadERK6Volume']):
            continue
        section=elf.get_section(sym['st_shndx'])
        addr=sym['st_value'] & ~1
        start=addr-section['sh_addr']
        data=section.data()[start:start+sym['st_size']]
        print('\n'+sym.name,hex(addr),'size',len(data))
        for ins in Cs(CS_ARCH_ARM, CS_MODE_THUMB if sym['st_value']&1 else CS_MODE_ARM).disasm(data,addr):
            annotation=''
            if ins.mnemonic in ['bl','blx','b','b.w'] and ins.op_str.startswith('#'):
                annotation=names.get(int(ins.op_str[1:],0)&~1,'')
            if annotation: print(hex(ins.address),ins.mnemonic,annotation)
