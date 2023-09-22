#![allow(dead_code)]

use nom::error::ParseError;

trait ReadByteExt {
    fn read_byte(&mut self) -> std::io::Result<u8>;
}
impl ReadByteExt for &[u8] {
    #[inline]
    fn read_byte(&mut self) -> std::io::Result<u8> {
        let b = *self
            .first()
            .ok_or_else(|| std::io::Error::new(std::io::ErrorKind::UnexpectedEof, "eof"))?;
        *self = &self[1..];
        Ok(b)
    }
}

const OFFSET_TABLE: [usize; 12] = [0, 16, 80, 336, 1360, 5456, 15, 79, 335, 1359, 5455, 21839];

struct HuffmanTables {
    decode: [u16; 2516],
    array: [i16; 1258],
}

impl Default for HuffmanTables {
    fn default() -> Self {
        let mut tables = Self {
            decode: [0; 2516],
            array: [0; 1258],
        };
        for incr in 2..1258 {
            tables.decode[1258 + incr] = incr as u16 / 2;
            tables.array[incr] = 1;
        }
        let mut odd = 3;
        for even in 1..ODD {
            tables.decode[ODD + even] = odd as u16;
            odd += 2;
            tables.decode[even] = even as u16 * 2;
        }
        tables
    }
}

const ODD: usize = 629;
const INCR: usize = 1258;

impl HuffmanTables {
    fn check(&mut self, mut a0: u16, mut a1: u16) {
        let mut idb1 = a0;
        loop {
            let idb2 = self.decode[INCR + idb1 as usize];
            self.array[idb2 as usize] =
                self.array[a1 as usize].wrapping_add(self.array[a0 as usize]);
            a0 = idb2;
            if idb2 != 1 {
                let idb1 = self.decode[INCR + idb2 as usize];
                let idb2 = self.decode[idb1 as usize];
                a1 = idb2;
                if a0 == idb2 {
                    a1 = self.decode[ODD + idb1 as usize];
                }
            }
            idb1 = a0;
            if a0 == 1 {
                break;
            }
        }
        if self.array[1] == 0x7d0 {
            for cur in 1..1258 {
                self.array[cur] >>= 1;
            }
        }
    }
    fn update(&mut self, tblpos: usize) {
        let mut idb1 = tblpos + 0x275;
        self.array[idb1] += 1;
        if self.decode[INCR + idb1] != 1 {
            let mut tmp_incr = INCR + idb1;
            let mut idb2 = self.decode[tmp_incr] as usize;
            if idb1 == self.decode[idb2] as usize {
                self.check(idb1 as u16, self.decode[ODD + idb2]);
            } else {
                self.check(idb1 as u16, self.decode[idb2]);
            }
            loop {
                let incr_idx = self.decode[INCR + idb2] as usize;
                let even_val = self.decode[incr_idx];
                let idb3 = if idb2 == even_val as usize {
                    self.decode[ODD + incr_idx]
                } else {
                    even_val
                } as usize;
                if self.array[idb3] < self.array[idb1] {
                    if idb2 == even_val as usize {
                        self.decode[ODD + incr_idx] = idb1 as u16;
                    } else {
                        self.decode[incr_idx] = idb1 as u16;
                    }
                    let even_val = self.decode[idb2];
                    let idb4 = if idb1 == even_val as usize {
                        let idb4 = self.decode[ODD + idb2];
                        self.decode[idb2] = idb3 as u16;
                        idb4
                    } else {
                        self.decode[ODD + idb2] = idb3 as u16;
                        even_val
                    };
                    self.decode[INCR + idb3] = idb2 as u16;
                    self.decode[tmp_incr] = incr_idx as u16;
                    self.check(idb3 as u16, idb4);
                    tmp_incr = INCR + idb3;
                }
                idb1 = self.decode[tmp_incr] as usize;
                tmp_incr = INCR + idb1;
                idb2 = self.decode[tmp_incr] as usize;
                if idb2 == 1 {
                    break;
                }
            }
        }
    }
}

const HASH_SIZE: usize = 0x4000;
const WINDOW_SIZE: usize = 21902;
const OFFSET_MAX_SIZE: usize = 21839;

struct HuffmanDecoder<'a> {
    input: &'a [u8],
    bit_count: i8,
    bit_buffer: u8,
    window: [u8; WINDOW_SIZE],
    tables: HuffmanTables,
}

impl<'a> HuffmanDecoder<'a> {
    #[inline]
    fn new(input: &'a [u8]) -> Self {
        Self {
            input,
            bit_count: 0,
            bit_buffer: 0,
            window: [0; WINDOW_SIZE],
            tables: Default::default(),
        }
    }
    fn read_bit(&mut self) -> std::io::Result<bool> {
        let count = self.bit_count;
        self.bit_count = count - 1;
        if count < 1 {
            self.bit_buffer = self.input.read_byte()?;
            self.bit_count = 7;
        }
        let res = (self.bit_buffer & 0x80) != 0;
        self.bit_buffer <<= 1;
        Ok(res)
    }
    fn read_code(&mut self, byte: u8) -> std::io::Result<u32> {
        let mut res = 0;
        let mut i = 0;
        let mut shift = 1;
        if byte == 0 {
            return Ok(0);
        }
        while i != byte {
            if self.read_bit()? {
                res |= shift;
            }
            i += 1;
            shift <<= 1;
        }
        Ok(res)
    }
    fn start_decode(&mut self) -> std::io::Result<u16> {
        let mut lookup = 1u16;
        while lookup < 0x275 {
            if self.read_bit()? {
                lookup = self.tables.decode[ODD + lookup as usize];
            } else {
                lookup = self.tables.decode[lookup as usize];
            }
        }
        lookup -= 0x275;
        self.tables.update(lookup as usize);
        Ok(lookup)
    }
}

pub fn decode_d64<'a, E: ParseError<&'a [u8]>>(
    input: &'a [u8],
    cap: usize,
) -> nom::IResult<&'a [u8], Vec<u8>, E> {
    if input.is_empty() {
        return Ok((input, Vec::new()));
    }
    let mut output = Vec::with_capacity(cap);
    let mut decoder = Box::new(HuffmanDecoder::new(input));
    let mut incr = 0;
    let mut dec_byte = decoder
        .start_decode()
        .map_err(|_| crate::nom_fail(decoder.input))?;
    while dec_byte != 256 {
        if dec_byte < 256 {
            output.push((dec_byte & 0xff) as u8);
            decoder.window[incr] = (dec_byte & 0xff) as u8;
            incr += 1;
            if incr == WINDOW_SIZE {
                incr = 0;
            }
        } else {
            let shift_pos = (dec_byte - 257) / 62;
            let copy_cnt = (dec_byte - (shift_pos * 62)) - 254;
            let resc_byte = decoder
                .read_code((shift_pos * 2 + 4) as u8)
                .map_err(|_| crate::nom_fail(decoder.input))?;
            let mut copy_pos = incr as isize
                - (OFFSET_TABLE[shift_pos as usize] as isize
                    + resc_byte as isize
                    + copy_cnt as isize);
            if copy_pos < 0 {
                copy_pos += WINDOW_SIZE as isize;
            }
            let mut store_pos = incr;
            for _ in 0..copy_cnt {
                output.push(decoder.window[copy_pos as usize]);
                decoder.window[store_pos] = decoder.window[copy_pos as usize];
                store_pos += 1;
                copy_pos += 1;
                if store_pos == WINDOW_SIZE {
                    store_pos = 0;
                }
                if copy_pos == WINDOW_SIZE as isize {
                    copy_pos = 0;
                }
            }
            incr += copy_cnt as usize;
            if incr >= WINDOW_SIZE {
                incr -= WINDOW_SIZE;
            }
        }
        dec_byte = decoder
            .start_decode()
            .map_err(|_| crate::nom_fail(decoder.input))?;
    }
    Ok((decoder.input, output))
}

struct HuffmanEncoder {
    output: Vec<u8>,
    offset: usize,
    incr: usize,
    typ: usize,
    bit_count: u8,
    bit_buffer: u8,
    tables: HuffmanTables,
    hashtable: [u16; HASH_SIZE],
    hashtarget: [u16; HASH_SIZE],
    hashnext: [u16; WINDOW_SIZE],
    hashprev: [u16; WINDOW_SIZE],
    window: [u8; WINDOW_SIZE],
}

impl HuffmanEncoder {
    fn with_capacity(cap: usize) -> Self {
        Self {
            output: Vec::with_capacity(cap),
            offset: 0,
            incr: 3,
            typ: 0,
            bit_count: 0,
            bit_buffer: 0,
            tables: Default::default(),
            hashtable: [u16::MAX; HASH_SIZE],
            hashtarget: [u16::MAX; HASH_SIZE],
            hashnext: [0; WINDOW_SIZE],
            hashprev: [0; WINDOW_SIZE],
            window: [0; WINDOW_SIZE],
        }
    }
    fn write_bit(&mut self, bit: bool) {
        self.bit_buffer <<= 1;
        if bit {
            self.bit_buffer |= 1;
        }
        self.bit_count += 1;
        if self.bit_count == 8 {
            self.output.push(self.bit_buffer);
            self.bit_buffer = 0;
            self.bit_count = 0;
        }
    }
    fn write_code(&mut self, mut bin: u32, shift: u8) {
        for _ in 0..shift {
            self.write_bit((bin & 1) == 1);
            bin >>= 1;
        }
    }
    fn flush_bits(&mut self) {
        if self.bit_count > 0 {
            self.output.push(self.bit_buffer << (8 - self.bit_count));
            self.bit_buffer = 0;
            self.bit_count = 0;
        }
    }
    fn start_encode(&mut self, lookup: usize) {
        let mut binary = arrayvec::ArrayVec::<u8, 64>::new();
        let mut lookup_code = lookup + 0x275;
        loop {
            if lookup_code <= 1 {
                break;
            }
            let lookup_check =
                self.tables.decode[ODD + self.tables.decode[INCR + lookup_code] as usize] as usize;
            binary.push(if lookup_check == lookup_code { 1 } else { 0 });
            lookup_code = self.tables.decode[INCR + lookup_code] as usize;
        }
        while let Some(byte) = binary.pop() {
            self.output.push(byte);
        }
        self.tables.update(lookup);
    }
    fn hash(&self, start: usize) -> usize {
        (((self.window[start % WINDOW_SIZE] as u32
            ^ ((self.window[(start + 1) % WINDOW_SIZE] as u32) << 4))
            ^ ((self.window[(start + 2) % WINDOW_SIZE] as u32) << 8))
            & (HASH_SIZE - 1) as u32) as usize
    }
    fn find_match(&mut self, mut start: usize, count: usize) -> usize {
        let mut encodedlen = 0;
        if start == WINDOW_SIZE {
            start = 0;
        }
        let key = self.hash(start);
        let mut offset = self.hashtable[key];
        let mut i = 0;
        while offset != u16::MAX {
            i += 1;
            if i > count {
                break;
            }
            if self.window[(start + encodedlen) % WINDOW_SIZE]
                == self.window[(offset as usize + encodedlen) % WINDOW_SIZE]
            {
                let mut samelen = 0;
                let mut curr = start;
                let mut next = offset as usize;
                while self.window[curr] == self.window[next] {
                    if samelen >= 64 || next == start || curr == self.incr {
                        break;
                    }
                    samelen += 1;
                    curr += 1;
                    if curr == WINDOW_SIZE {
                        curr = 0;
                    }
                    next += 1;
                    if next == WINDOW_SIZE {
                        next = 0;
                    }
                }
                let mut encodedpos = start as isize - offset as isize;
                if encodedpos < 0 {
                    encodedpos += WINDOW_SIZE as isize;
                }
                encodedpos -= samelen;
                if (encodedlen as isize) < samelen
                    && encodedpos <= OFFSET_MAX_SIZE as isize
                    && (samelen > 3 || OFFSET_TABLE[9 + self.typ] as isize >= encodedpos)
                {
                    encodedlen = samelen as usize;
                    self.offset = encodedpos as usize;
                }
            }
            offset = self.hashnext[offset as usize];
        }
        encodedlen
    }
    fn insert_node_directory(&mut self, start: usize) {
        let key = self.hash(start);
        if self.hashtable[key] == u16::MAX {
            self.hashtarget[key] = start as u16;
            self.hashnext[start] = u16::MAX;
        } else {
            self.hashnext[start] = self.hashtable[key];
            self.hashprev[self.hashtable[key] as usize] = start as u16;
        }
        self.hashtable[key] = start as u16;
        self.hashprev[start] = u16::MAX;
    }
    fn delete_node_directory(&mut self, start: usize) {
        let key = self.hash(start);
        if self.hashtable[key] == self.hashtarget[key] {
            self.hashtable[key] = u16::MAX;
        } else {
            self.hashnext[self.hashprev[self.hashtarget[key] as usize] as usize] = u16::MAX;
            self.hashtarget[key] = self.hashprev[self.hashtarget[key] as usize];
        }
    }
}

#[allow(unreachable_code, unused_variables, unused_mut)]
pub fn encode_d64(mut input: &[u8]) -> Vec<u8> {
    panic!("do not use - incomplete and broken");
    let mut encoder = Box::new(HuffmanEncoder::with_capacity(input.len() * 2));
    for i in 0..3 {
        if let Ok(b) = input.read_byte() {
            encoder.start_encode(b as usize);
            encoder.window[i] = b;
        }
    }
    for _ in 0..64 {
        if let Ok(b) = input.read_byte() {
            if b >= 128 {
                encoder.typ = 1;
            }
            encoder.window[encoder.incr] = b;
            encoder.incr += 1;
        } else {
            break;
        }
    }
    let (cnt_min, cnt_max) = match encoder.typ {
        1 => (20, 200),
        _ => (50, 1000),
    };
    if !input.is_empty() {
        let mut copy_count = 0isize;
        let mut skip_copy = false;
        let mut node_pos = 0;
        let mut read_pos = 3;
        let mut delete_node = false;
        while read_pos != encoder.incr {
            encoder.insert_node_directory(node_pos);
            if !skip_copy {
                let copy_count_next = encoder.find_match(read_pos + 1, cnt_min) as isize;
                copy_count = encoder.find_match(read_pos, cnt_max) as isize;
                if copy_count >= 3 && copy_count >= copy_count_next {
                    let mut val_extra = encoder.offset;
                    let mut shift = 0x04;
                    for i in 1..6 {
                        if val_extra >= OFFSET_TABLE[i]
                            && (i == 5 || val_extra < OFFSET_TABLE[i + 1])
                        {
                            shift = i * 2 + 4;
                            val_extra -= OFFSET_TABLE[i];
                        }
                    }

                    let lookup_code = 0x101 + 62 * ((shift as isize - 4) / 2) + copy_count - 3;
                    encoder.start_encode(lookup_code as usize);
                    encoder.write_code(val_extra as u32, shift as u8);
                    skip_copy = true;
                } else {
                    encoder.start_encode(encoder.window[read_pos] as usize);
                }
            }
            copy_count -= 1;
            if copy_count == 0 {
                skip_copy = false;
            }
            read_pos += 1;
            if read_pos == WINDOW_SIZE {
                read_pos = 0;
            }
            node_pos += 1;
            if node_pos == WINDOW_SIZE {
                node_pos = 0;
            }
            if let Ok(byte) = input.read_byte() {
                encoder.window[encoder.incr] = byte;
                encoder.incr += 1;
                if encoder.incr == WINDOW_SIZE {
                    encoder.incr = 0;
                    delete_node = true;
                }
            }
            if delete_node && !input.is_empty() {
                encoder.delete_node_directory(encoder.incr);
            }
        }
    }
    encoder.start_encode(0x100);
    encoder.flush_bits();
    encoder.output
}
